#include "Replace.h"
#include "Arguments.h"
#include "Input.h"
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

static constexpr int noCapture { -1 };

static std::pair<int, size_t> getBracedReference (std::string_view replacementText, size_t index)
{
    static constexpr size_t bracedOpenSpan { 2 };
    static constexpr size_t closeBraceSpan { 1 };
    const size_t digitsBegin { index + bracedOpenSpan };
    size_t digitsEnd { digitsBegin };

    while (digitsEnd < replacementText.size() and isAsciiDigit (replacementText.at (digitsEnd)))
        ++digitsEnd;

    if (digitsEnd > digitsBegin and digitsEnd < replacementText.size() and replacementText.at (digitsEnd) == static_cast<char> (Chars::closeBrace))
    {
        int parsedIndex { 0 };
        const auto parseResult { std::from_chars (replacementText.data() + digitsBegin, replacementText.data() + digitsEnd, parsedIndex) };
        const int groupIndex { parseResult.ec == std::errc {} ? parsedIndex : std::numeric_limits<int>::max() };
        return { groupIndex, digitsEnd + closeBraceSpan - index };
    }

    return { noCapture, 0 };
}

static std::pair<int, size_t> getCaptureReference (std::string_view replacementText, size_t index)
{
    static constexpr size_t dollarDigitSpan { 2 };

    if (index + 1 >= replacementText.size() or replacementText.at (index) != static_cast<char> (Chars::dollarSign))
        return { noCapture, 0 };

    const size_t nextIndex { index + 1 };

    if (isAsciiDigit (replacementText.at (nextIndex)))
        return { replacementText.at (nextIndex) - static_cast<char> (Chars::zero), dollarDigitSpan };

    if (replacementText.at (nextIndex) == static_cast<char> (Chars::openBrace))
        return getBracedReference (replacementText, index);

    return { noCapture, 0 };
}

static size_t getReplacementTokenSpan (std::string_view replacementText, size_t index)
{
    static constexpr size_t literalSpan { 1 };
    static constexpr size_t escapedDollarSpan { 2 };

    if (index + 1 >= replacementText.size() or replacementText.at (index) != static_cast<char> (Chars::dollarSign))
        return literalSpan;

    if (replacementText.at (index + 1) == static_cast<char> (Chars::dollarSign))
        return escapedDollarSpan;

    const auto [groupIndex, referenceSpan] { getCaptureReference (replacementText, index) };

    if (groupIndex != noCapture)
        return referenceSpan;

    return literalSpan;
}

static juce::Result getCaptureValidationStatus (std::string_view replacementText, int numCaptureGroups)
{
    size_t index { 0 };

    while (index < replacementText.size())
    {
        const auto [groupIndex, referenceSpan] { getCaptureReference (replacementText, index) };
        juce::ignoreUnused (referenceSpan);

        if (groupIndex > numCaptureGroups)
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failCaptureInvalid, groupIndex));

        index += getReplacementTokenSpan (replacementText, index);
    }

    return juce::Result::ok();
}

static size_t appendRegexToken (std::string& output,
                                std::string_view replacementText,
                                size_t index,
                                const jam::Array<std::optional<std::string_view>>& captures)
{
    const size_t tokenSpan { getReplacementTokenSpan (replacementText, index) };
    const auto [groupIndex, referenceSpan] { getCaptureReference (replacementText, index) };
    juce::ignoreUnused (referenceSpan);

    if (groupIndex == noCapture)
        output.push_back (static_cast<char> (Chars::dollarSign));
    else
        jam::Format::appendBytes (output, captures.at (groupIndex).value_or (std::string_view {}));

    return tokenSpan;
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
        size_t index { 0 };

        while (index < replacementText.size())
        {
            if (replacementText.at (index) == static_cast<char> (Chars::dollarSign) and index + 1 < replacementText.size())
            {
                index += appendRegexToken (output, replacementText, index, captures);
            }
            else
            {
                output.push_back (replacementText.at (index));
                ++index;
            }
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

static std::tuple<juce::Result, int, std::string> getReplacedLine (std::string_view line,
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
        auto [findStatus, captures] { pattern.findNext (line, offset) };

        if (findStatus.failed())
            return { findStatus, 0, std::string {} };

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
    return { juce::Result::ok(), matchCount, std::move (output) };
}

static std::tuple<juce::Result, LineDiff, int> getDocumentDiff (const TextDocument& document,
                                                                const Pattern& pattern,
                                                                std::string_view replacementText,
                                                                bool isRegex,
                                                                juce::Range<int> lineRange,
                                                                jam::RegExpAutomaton& automaton)
{
    jam::Array<LineDiff::Change> changes;
    int totalEdits { 0 };
    const auto activeRange { lineRange.getIntersectionWith (juce::Range<int> (1, document.getNumLines() + 1)) };
    const auto bytes { document.getBytes() };
    const size_t firstOffset { activeRange.isEmpty() ? bytes.size() : static_cast<size_t> (document.getLine (activeRange.getStart()).data() - bytes.data()) };
    size_t offset { firstOffset };

    while (offset < bytes.size())
    {
        const auto candidateOffset { pattern.findCandidate (bytes, offset, automaton) };

        if (candidateOffset >= bytes.size())
            break;

        const int lineNumber { document.getLineNumber (candidateOffset) };

        if (lineNumber >= activeRange.getEnd())
            break;

        auto [lineStatus, lineMatchCount, replacedLine] { getReplacedLine (document.getLine (lineNumber), pattern, replacementText, isRegex) };

        if (lineStatus.failed())
            return { lineStatus, LineDiff {}, 0 };

        if (lineMatchCount > 0)
        {
            LineDiff::Change change;
            change.start = lineNumber - 1;
            change.length = 1;
            change.insertedLines = LineDiff::getLinesPreservingEmpty (replacedLine);
            changes.add (std::move (change));
            totalEdits += lineMatchCount;
        }

        offset = lineNumber < document.getNumLines()
                     ? static_cast<size_t> (document.getLine (lineNumber + 1).data() - bytes.data())
                     : bytes.size();
    }

    return { juce::Result::ok(), LineDiff { std::move (changes) }, totalEdits };
}

static Stdio::Output runStandardInput (const Arguments& arguments,
                                       const Pattern& pattern,
                                       std::string_view replacementText,
                                       bool isRegex,
                                       juce::Range<int> lineRange,
                                       const std::optional<std::string>& input)
{
    if (arguments.contains (Id::applyOption))
        return Stdio::getFailure (text::Diagnostics::failApplyStandardInput);

    auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus.getErrorMessage());

    if (standardInput->getBytes().empty())
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    auto document { TextDocument::fromInput (std::move (standardInput), jam::Strings::fromView (Id::stdinPath)) };
    jam::RegExpAutomaton automaton;
    auto [diffStatus, diff, editCount] { getDocumentDiff (*document, pattern, replacementText, isRegex, lineRange, automaton) };

    if (diffStatus.failed())
        return Stdio::getFailure (diffStatus.getErrorMessage());

    Transaction transaction { Id::replace };
    const auto addStatus { transaction.addFile (juce::File {}, std::move (document), std::move (diff), editCount) };

    if (addStatus.failed())
        return Stdio::getFailure (addStatus.getErrorMessage());

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
    auto [loadStatus, document] { TextDocument::fromOwnedFile (targetFile, displayPath) };

    if (loadStatus.failed())
        return { loadStatus, std::unique_ptr<TextDocument> {}, LineDiff {}, 0 };

    if (document->isBinary())
    {
        if (isExplicit)
            return { TextDocument::getBinaryFailure (displayPath),
                     std::unique_ptr<TextDocument> {},
                     LineDiff {},
                     0 };

        return { juce::Result::ok(),
                 std::unique_ptr<TextDocument> {},
                 LineDiff {},
                 0 };
    }

    jam::RegExpAutomaton automaton;

    if (pattern.findCandidate (document->getBytes(), 0, automaton) >= document->getBytes().size())
        return { juce::Result::ok(),
                 std::unique_ptr<TextDocument> {},
                 LineDiff {},
                 0 };

    auto [diffStatus, diff, editCount] { getDocumentDiff (*document, pattern, replacementText, isRegex, lineRange, automaton) };

    if (diffStatus.failed())
        return { diffStatus, std::unique_ptr<TextDocument> {}, LineDiff {}, 0 };

    return { juce::Result::ok(), std::move (document), std::move (diff), editCount };
}

static Stdio::Output runTransactionForFiles (const Arguments& arguments,
                                             const std::vector<std::pair<std::string, juce::File>>& files,
                                             const jam::Array<juce::Result>& statuses,
                                             jam::Array<std::unique_ptr<TextDocument>>& documents,
                                             jam::Array<LineDiff>& diffs,
                                             const jam::Array<int>& editCounts)
{
    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
            return Stdio::getFailure (statuses.at (index).getErrorMessage());
    }

    Transaction transaction { Id::replace };
    std::set<juce::File> targetFiles;

    for (int index { 0 }; index < static_cast<int> (files.size()); ++index)
    {
        if (documents.at (index) != nullptr)
        {
            const auto& [filePath, targetFile] { files.at (static_cast<size_t> (index)) };
            juce::ignoreUnused (filePath);
            const auto [targetEntry, isInserted] { targetFiles.insert (targetFile) };
            juce::ignoreUnused (targetEntry);

            if (isInserted)
            {
                const auto addStatus { transaction.addFile (targetFile, std::move (documents.at (index)), std::move (diffs.at (index)), editCounts.at (index)) };

                if (addStatus.failed())
                    return Stdio::getFailure (addStatus.getErrorMessage());
            }
        }
    }

    return transaction.run (arguments, false);
}

static Stdio::Output runParallelFilesReplace (const Arguments& arguments,
                                              const std::vector<std::pair<std::string, juce::File>>& files,
                                              const jam::Array<bool>& isExplicitFlags,
                                              const Pattern& pattern,
                                              std::string_view replacementText,
                                              bool isRegex,
                                              juce::Range<int> lineRange)
{
    const auto fileCount { static_cast<int> (files.size()) };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount);
    jam::Array<LineDiff> diffs;
    diffs.resize (fileCount);
    jam::Array<std::unique_ptr<TextDocument>> documents;
    documents.resize (fileCount);
    jam::Array<int> editCounts;
    editCounts.resize (fileCount);

    Jobs::run (static_cast<int> (files.size()),
               [&files, &isExplicitFlags, &pattern, replacementText, isRegex, lineRange, &statuses, &documents, &diffs, &editCounts] (int index)
               {
                   const auto& [filePath, targetFile] { files.at (static_cast<size_t> (index)) };
                   const auto displayPath { jam::Strings::fromView (filePath) };
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

    return runTransactionForFiles (arguments, files, statuses, documents, diffs, editCounts);
}

static Stdio::Output runFiles (const Arguments& arguments,
                               const jam::Strings& rawPaths,
                               const Pattern& pattern,
                               std::string_view replacementText,
                               bool isRegex,
                               juce::Range<int> lineRange)
{
    const auto [readStatus, files, isExplicitFlags] { Paths::getReadableFiles (rawPaths,
                                                                               arguments.getValues (Id::includeOption),
                                                                               arguments.getValues (Id::excludeOption),
                                                                               arguments.contains (Id::hiddenOption),
                                                                               arguments.contains (Id::noIgnoreOption)) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus.getErrorMessage());

    return runParallelFilesReplace (arguments, files, isExplicitFlags, pattern, replacementText, isRegex, lineRange);
}

static Stdio::Output runReplacement (const Arguments& arguments,
                                     const jam::Strings& rawPaths,
                                     const Pattern& pattern,
                                     std::string_view replacementText,
                                     bool isRegex,
                                     juce::Range<int> lineRange,
                                     const std::optional<std::string>& input)
{
    if (isRegex)
    {
        const auto captureStatus { getCaptureValidationStatus (replacementText, pattern.getNumCaptureGroups()) };

        if (captureStatus.failed())
            return Stdio::getFailure (captureStatus.getErrorMessage());
    }

    if (rawPaths.size() == 0)
        return runStandardInput (arguments, pattern, replacementText, isRegex, lineRange, input);

    return runFiles (arguments, rawPaths, pattern, replacementText, isRegex, lineRange);
}

static Stdio::Output runWithValidatedPattern (const Arguments& arguments,
                                              const jam::Strings& rawPaths,
                                              const Pattern& pattern,
                                              bool hasWithStdin,
                                              bool hasWithFile,
                                              bool isRegex,
                                              juce::Range<int> lineRange,
                                              const std::optional<std::string>& input)
{
    if (hasWithStdin)
    {
        const auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

        if (inputStatus.failed())
            return Stdio::getFailure (inputStatus.getErrorMessage());

        return runReplacement (arguments, rawPaths, pattern, standardInput->getBytes(), isRegex, lineRange, input);
    }

    if (hasWithFile)
    {
        const auto filePath { arguments.getValue (Id::withFileOption) };
        const auto targetFile { Paths::getLinkedTarget (jam::Strings::getView (filePath)) };
        const auto [loadStatus, document] { TextDocument::fromTextFile (targetFile, filePath, Input::maxBufferedReadSize) };

        if (loadStatus.failed())
            return Stdio::getFailure (loadStatus.getErrorMessage());

        return runReplacement (arguments, rawPaths, pattern, document->getBytes(), isRegex, lineRange, input);
    }

    return runReplacement (arguments, rawPaths, pattern, arguments.getPositionals().getView (1), isRegex, lineRange, input);
}

static Stdio::Output runWithValidatedInputs (const Arguments& arguments,
                                             const jam::Strings& rawPaths,
                                             bool hasWithStdin,
                                             bool hasWithFile,
                                             const std::optional<std::string>& input)
{
    const auto [rangeStatus, lineRange] { arguments.getLineRangeFilter() };

    if (rangeStatus.failed())
        return Stdio::getFailure (rangeStatus.getErrorMessage());

    const bool isRegex { arguments.contains (Id::regexOption) };
    jam::Strings patternTexts;
    patternTexts.add (arguments.getPositionals().getView (0));
    const auto [patternStatus, pattern] { Pattern::create (patternTexts,
                                                           isRegex,
                                                           arguments.contains (Id::ignoreCaseOption),
                                                           false) };

    if (patternStatus.failed())
        return Stdio::getFailure (patternStatus.getErrorMessage());

    return runWithValidatedPattern (arguments, rawPaths, *pattern, hasWithStdin, hasWithFile, isRegex, lineRange, input);
}

static int getPathStartIndex (bool hasWithStdin, bool hasWithFile) noexcept
{
    if (hasWithStdin or hasWithFile)
        return 1;

    return 2;
}

static Stdio::Output runWithValidatedArguments (const Arguments& arguments, const std::optional<std::string>& input)
{
    const bool hasWithStdin { arguments.contains (Id::withStdinOption) };
    const bool hasWithFile { arguments.contains (Id::withFileOption) };

    if (hasWithStdin and hasWithFile)
        return Stdio::getFailure (text::Diagnostics::failWithSourcesConflict);

    const auto& positionals { arguments.getPositionals() };
    const int pathStartIndex { getPathStartIndex (hasWithStdin, hasWithFile) };

    if (positionals.size() < pathStartIndex)
        return Stdio::getFailure (text::Diagnostics::failReplaceRequiresArguments);

    jam::Strings rawPaths;
    rawPaths.addArray (positionals, pathStartIndex, -1);

    if (rawPaths.size() == 0 and (hasWithStdin or Stdio::isTerminalInput (input)))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    return runWithValidatedInputs (arguments, rawPaths, hasWithStdin, hasWithFile, input);
}

Arguments Replace::getArguments (const jam::Strings& argumentList)
{
    const std::vector<std::string_view> flagOptions
    {
        Id::regexOption, Id::ignoreCaseOption, Id::withStdinOption, Id::applyOption,
        Id::hiddenOption, Id::noIgnoreOption
    };
    const std::vector<std::string_view> valueOptions
    {
        Id::linesOption, Id::withFileOption, Id::includeOption, Id::excludeOption, Id::expectOption
    };

    return Arguments { argumentList, flagOptions, valueOptions, {} };
}

Stdio::Output Replace::run (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus().getErrorMessage());

    return runWithValidatedArguments (arguments, input);
}
