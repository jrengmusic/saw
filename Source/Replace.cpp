#include "Replace.h"
#include "Arguments.h"
#include "Jobs.h"
#include "LineDiff.h"
#include "Paths.h"
#include "Pattern.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Transaction.h"
#include "generated/Generated.h"

static bool isAsciiDigit (char byte) noexcept
{
    return byte >= static_cast<char> (Chars::zero) and byte <= static_cast<char> (Chars::nine);
}

static juce::Result getCaptureValidationStatus (std::string_view replacementText, int numCaptureGroups)
{
    for (size_t index { 0 }; index + 1 < replacementText.size(); ++index)
    {
        if (replacementText.at (index) == static_cast<char> (Chars::dollarSign))
        {
            const char nextChar { replacementText.at (index + 1) };

            if (nextChar == static_cast<char> (Chars::dollarSign))
                ++index;
            else if (isAsciiDigit (nextChar) and (nextChar - static_cast<char> (Chars::zero)) > numCaptureGroups)
                return juce::Result::fail (juce::String::formatted (text::Diagnostics::failCaptureInvalid,
                                                                    nextChar - static_cast<char> (Chars::zero)));
        }
    }

    return juce::Result::ok();
}

static std::pair<juce::Result, std::string> getReplacementFromFile (const juce::String& filePath)
{
    const auto targetFile { juce::File::getCurrentWorkingDirectory().getChildFile (filePath).getLinkedTarget() };
    const auto [loadStatus, document] { TextDocument::fromTextFile (targetFile, filePath) };

    if (loadStatus.failed())
        return { loadStatus, {} };

    return { juce::Result::ok(), std::string { document->getBytes() } };
}

static std::pair<juce::Result, std::string> getReplacementText (const Arguments& arguments,
                                                                bool hasWithStdin,
                                                                bool hasWithFile)
{
    if (hasWithStdin)
        return Stdio::getInput();

    if (hasWithFile)
        return getReplacementFromFile (arguments.getValue (Id::withFileOption));

    return { juce::Result::ok(), arguments.getPositionals().at (1).toStdString() };
}

static std::pair<juce::Result, std::string> getValidatedReplacementText (const Arguments& arguments,
                                                                         bool hasWithStdin,
                                                                         bool hasWithFile,
                                                                         bool isRegex,
                                                                         int numCaptureGroups)
{
    auto [replacementStatus, replacementText] { getReplacementText (arguments, hasWithStdin, hasWithFile) };

    if (replacementStatus.failed())
        return { replacementStatus, {} };

    if (isRegex)
    {
        const auto captureStatus { getCaptureValidationStatus (replacementText, numCaptureGroups) };

        if (captureStatus.failed())
            return { captureStatus, {} };
    }

    return { juce::Result::ok(), std::move (replacementText) };
}

static void appendRegexToken (std::string& output,
                              std::string_view replacementText,
                              size_t& index,
                              const jam::Array<std::optional<std::string_view>>& captures)
{
    const char nextChar { replacementText.at (index + 1) };

    if (nextChar == static_cast<char> (Chars::dollarSign))
    {
        output.push_back (static_cast<char> (Chars::dollarSign));
        ++index;
    }
    else if (isAsciiDigit (nextChar))
    {
        const int groupIndex { nextChar - static_cast<char> (Chars::zero) };
        jam::Format::appendBytes (output, captures.at (groupIndex).value_or (std::string_view {}));
        ++index;
    }
    else
    {
        output.push_back (static_cast<char> (Chars::dollarSign));
    }
}

static void appendExpandedReplacement (std::string& output,
                                       std::string_view replacementText,
                                       bool isRegex,
                                       const jam::Array<std::optional<std::string_view>>& captures)
{
    if (not isRegex)
    {
        jam::Format::appendBytes (output, replacementText);
    }
    else
    {
        for (size_t index { 0 }; index < replacementText.size(); ++index)
        {
            if (replacementText.at (index) == static_cast<char> (Chars::dollarSign) and index + 1 < replacementText.size())
                appendRegexToken (output, replacementText, index, captures);
            else
                output.push_back (replacementText.at (index));
        }
    }
}

static size_t getNextSearchOffset (size_t matchSize, size_t matchEnd, size_t lineSize) noexcept
{
    if (matchSize > 0)
        return matchEnd;

    if (matchEnd < lineSize)
        return matchEnd + 1;

    return lineSize + 1;
}

static std::pair<int, std::string> getReplacedLine (std::string_view line,
                                                    const Pattern& pattern,
                                                    std::string_view replacementText,
                                                    bool isRegex)
{
    std::string output;
    size_t offset { 0 };
    size_t lastEmitted { 0 };
    int matchCount { 0 };

    while (offset <= line.size())
    {
        const auto captures (pattern.findNext (line, offset));

        if (captures.isEmpty())
            break;

        const auto match { *captures.at (0) };
        const size_t matchStart { static_cast<size_t> (match.data() - line.data()) };
        const size_t matchEnd { matchStart + match.size() };
        jam::Format::appendBytes (output, line.substr (lastEmitted, matchStart - lastEmitted));
        appendExpandedReplacement (output, replacementText, isRegex, captures);
        ++matchCount;
        lastEmitted = matchEnd;
        offset = getNextSearchOffset (match.size(), matchEnd, line.size());
    }

    jam::Format::appendBytes (output, line.substr (lastEmitted));
    return { matchCount, std::move (output) };
}

static std::pair<LineDiff, int> getDocumentDiff (const TextDocument& document,
                                                 const Pattern& pattern,
                                                 std::string_view replacementText,
                                                 bool isRegex,
                                                 juce::Range<int> lineRange)
{
    jam::Array<LineDiff::Change> changes;
    int totalEdits { 0 };
    const auto activeRange { lineRange.getIntersectionWith (juce::Range<int> (1, document.getNumLines() + 1)) };

    for (int lineNumber { activeRange.getStart() }; lineNumber < activeRange.getEnd(); ++lineNumber)
    {
        auto [lineMatchCount, replacedLine] { getReplacedLine (document.getLine (lineNumber), pattern, replacementText, isRegex) };

        if (lineMatchCount > 0)
        {
            LineDiff::Change change;
            change.start = lineNumber - 1;
            change.length = 1;
            change.insertedLines = LineDiff::getLinesPreservingEmpty (replacedLine);
            changes.add (std::move (change));
            totalEdits += lineMatchCount;
        }
    }

    return { LineDiff { std::move (changes) }, totalEdits };
}

static int runStandardInput (const Arguments& arguments,
                             const Pattern& pattern,
                             std::string_view replacementText,
                             bool isRegex,
                             juce::Range<int> lineRange)
{
    if (arguments.contains (Id::applyOption))
    {
        Stdio::printError (text::Diagnostics::failApplyStandardInput);
        return Stdio::exitError;
    }

    auto [inputStatus, inputText] { Stdio::getInput() };

    if (inputStatus.failed())
    {
        Stdio::printError (inputStatus.getErrorMessage());
        return Stdio::exitError;
    }

    auto document { TextDocument::fromString (std::move (inputText), Id::stdinPath) };
    auto [diff, editCount] { getDocumentDiff (*document, pattern, replacementText, isRegex, lineRange) };
    Transaction transaction { Id::replace.toString() };
    transaction.addFile (juce::File {}, std::move (document), std::move (diff), editCount);
    return transaction.run (arguments, true);
}

static std::tuple<juce::Result, std::unique_ptr<TextDocument>, LineDiff, int>
getReplacedFile (const juce::File& targetFile,
                 const juce::String& displayPath,
                 bool isExplicit,
                 const Pattern& pattern,
                 std::string_view replacementText,
                 bool isRegex,
                 juce::Range<int> lineRange)
{
    auto [loadStatus, document] { TextDocument::fromFile (targetFile, displayPath) };

    if (loadStatus.failed())
        return { loadStatus, std::unique_ptr<TextDocument> {}, LineDiff { jam::Array<LineDiff::Change> {} }, 0 };

    if (document->isBinary())
    {
        if (isExplicit)
            return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failBinaryFile, displayPath.toRawUTF8())),
                     std::unique_ptr<TextDocument> {},
                     LineDiff { jam::Array<LineDiff::Change> {} },
                     0 };

        return { juce::Result::ok(),
                 std::unique_ptr<TextDocument> {},
                 LineDiff { jam::Array<LineDiff::Change> {} },
                 0 };
    }

    auto [diff, editCount] { getDocumentDiff (*document, pattern, replacementText, isRegex, lineRange) };
    return { juce::Result::ok(), std::move (document), std::move (diff), editCount };
}

static jam::Array<LineDiff> getInitialDiffs (int fileCount)
{
    jam::Array<LineDiff> diffs;
    diffs.ensureStorageAllocated (fileCount);

    for (int index { 0 }; index < fileCount; ++index)
        diffs.add (LineDiff { jam::Array<LineDiff::Change> {} });

    return diffs;
}

static int runTransactionForFiles (const Arguments& arguments,
                                   const std::vector<std::string>& filePaths,
                                   const juce::File& workingDirectory,
                                   const jam::Array<juce::Result>& statuses,
                                   jam::Array<std::unique_ptr<TextDocument>>& documents,
                                   jam::Array<LineDiff>& diffs,
                                   const jam::Array<int>& editCounts)
{
    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
        {
            Stdio::printError (statuses.at (index).getErrorMessage());
            return Stdio::exitError;
        }
    }

    Transaction transaction { Id::replace.toString() };

    for (int index { 0 }; index < static_cast<int> (filePaths.size()); ++index)
    {
        if (documents.at (index) != nullptr)
        {
            const auto targetFile { workingDirectory.getChildFile (jam::Strings::fromView (filePaths.at (static_cast<size_t> (index)))).getLinkedTarget() };
            transaction.addFile (targetFile, std::move (documents.at (index)), std::move (diffs.at (index)), editCounts.at (index));
        }
    }

    return transaction.run (arguments, false);
}

static int runParallelFilesReplace (const Arguments& arguments,
                                    const std::vector<std::string>& filePaths,
                                    const jam::Array<bool>& isExplicitFlags,
                                    const Pattern& pattern,
                                    std::string_view replacementText,
                                    bool isRegex,
                                    juce::Range<int> lineRange)
{
    const auto fileCount { static_cast<int> (filePaths.size()) };
    const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount);
    auto diffs (getInitialDiffs (fileCount));
    jam::Array<std::unique_ptr<TextDocument>> documents;
    documents.resize (fileCount);
    jam::Array<int> editCounts;
    editCounts.resize (fileCount);

    Jobs::run (static_cast<int> (filePaths.size()),
               [&filePaths, &isExplicitFlags, &workingDirectory, &pattern, replacementText, isRegex, lineRange, &statuses, &documents, &diffs, &editCounts] (int index)
               {
                   const auto displayPath { jam::Strings::fromView (filePaths.at (static_cast<size_t> (index))) };
                   const auto targetFile { workingDirectory.getChildFile (displayPath).getLinkedTarget() };
                   auto [fileStatus, document, diff, editCount] { getReplacedFile (targetFile,
                                                                                   displayPath,
                                                                                   isExplicitFlags.at (index),
                                                                                   pattern,
                                                                                   replacementText,
                                                                                   isRegex,
                                                                                   lineRange) };
                   statuses.at (index) = std::move (fileStatus);
                   documents.at (index) = std::move (document);
                   diffs.at (index) = std::move (diff);
                   editCounts.at (index) = editCount;
               });

    return runTransactionForFiles (arguments, filePaths, workingDirectory, statuses, documents, diffs, editCounts);
}

static int runFiles (const Arguments& arguments,
                     const jam::Strings& rawPaths,
                     const Pattern& pattern,
                     std::string_view replacementText,
                     bool isRegex,
                     juce::Range<int> lineRange)
{
    const auto [filesStatus, files, isExplicitFlags] { Paths::getFiles (rawPaths,
                                                                        arguments.getValues (Id::includeOption),
                                                                        arguments.getValues (Id::excludeOption),
                                                                        false,
                                                                        false) };

    if (filesStatus.failed())
    {
        Stdio::printError (filesStatus.getErrorMessage());
        return Stdio::exitError;
    }

    return runParallelFilesReplace (arguments, files, isExplicitFlags, pattern, replacementText, isRegex, lineRange);
}

static int runWithValidatedPattern (const Arguments& arguments,
                                    const jam::Strings& rawPaths,
                                    const Pattern& pattern,
                                    bool hasWithStdin,
                                    bool hasWithFile,
                                    bool isRegex,
                                    juce::Range<int> lineRange)
{
    const auto [replacementStatus, replacementText] { getValidatedReplacementText (arguments,
                                                                                   hasWithStdin,
                                                                                   hasWithFile,
                                                                                   isRegex,
                                                                                   pattern.getNumCaptureGroups()) };

    if (replacementStatus.failed())
    {
        Stdio::printError (replacementStatus.getErrorMessage());
        return Stdio::exitError;
    }

    if (rawPaths.size() == 0)
        return runStandardInput (arguments, pattern, replacementText, isRegex, lineRange);

    return runFiles (arguments, rawPaths, pattern, replacementText, isRegex, lineRange);
}

static int runWithValidatedInputs (const Arguments& arguments,
                                   const jam::Strings& rawPaths,
                                   bool hasWithStdin,
                                   bool hasWithFile)
{
    const auto [rangeStatus, lineRange] { arguments.getLineRangeFilter() };

    if (rangeStatus.failed())
    {
        Stdio::printError (rangeStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const bool isRegex { arguments.contains (Id::regexOption) };
    const auto [patternStatus, pattern] { Pattern::create (arguments.getPositionals().at (0),
                                                           isRegex,
                                                           arguments.contains (Id::ignoreCaseOption),
                                                           false) };

    if (patternStatus.failed())
    {
        Stdio::printError (patternStatus.getErrorMessage());
        return Stdio::exitError;
    }

    return runWithValidatedPattern (arguments, rawPaths, *pattern, hasWithStdin, hasWithFile, isRegex, lineRange);
}

static int getPathStartIndex (bool hasWithStdin, bool hasWithFile) noexcept
{
    if (hasWithStdin or hasWithFile)
        return 1;

    return 2;
}

static int runWithValidatedArguments (const Arguments& arguments)
{
    const bool hasWithStdin { arguments.contains (Id::withStdinOption) };
    const bool hasWithFile { arguments.contains (Id::withFileOption) };

    if (hasWithStdin and hasWithFile)
    {
        Stdio::printError (text::Diagnostics::failWithSourcesConflict);
        return Stdio::exitError;
    }

    const auto& positionals { arguments.getPositionals() };
    const int pathStartIndex { getPathStartIndex (hasWithStdin, hasWithFile) };

    if (positionals.size() < pathStartIndex)
    {
        Stdio::printError (text::Diagnostics::failReplaceRequiresArguments);
        return Stdio::exitError;
    }

    jam::Strings rawPaths;
    rawPaths.addArray (positionals, pathStartIndex, -1);

    if (rawPaths.size() == 0 and (hasWithStdin or Stdio::isTerminalInput()))
    {
        Stdio::printError (text::Diagnostics::failMissingPath);
        return Stdio::exitError;
    }

    return runWithValidatedInputs (arguments, rawPaths, hasWithStdin, hasWithFile);
}

int Replace::run (const jam::Strings& argumentList)
{
    const jam::Strings flagOptions
    {
        Id::regexOption, Id::ignoreCaseOption, Id::withStdinOption, Id::applyOption
    };
    const jam::Strings valueOptions
    {
        Id::linesOption, Id::withFileOption, Id::includeOption, Id::excludeOption, Id::expectOption
    };
    const Arguments arguments { argumentList, 1, flagOptions, valueOptions };

    if (arguments.getStatus().failed())
    {
        Stdio::printError (arguments.getStatus().getErrorMessage());
        return Stdio::exitError;
    }

    return runWithValidatedArguments (arguments);
}
