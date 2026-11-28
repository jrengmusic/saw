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

static constexpr int noCapture { -1 };

static std::pair<int, size_t> getBracedReference (std::string_view replacementText, size_t index)
{
    static constexpr size_t bracedOpenSpan { 2 };
    static constexpr size_t closeBraceSpan { 1 };
    const size_t digitsBegin { index + bracedOpenSpan };
    size_t digitsEnd { digitsBegin };

    while (digitsEnd < replacementText.size() and jam::Document::isDigit (static_cast<unsigned char> (replacementText.at (digitsEnd))))
        ++digitsEnd;

    if (digitsEnd > digitsBegin and digitsEnd < replacementText.size() and replacementText.at (digitsEnd) == static_cast<char> (Chars::closeBrace))
    {
        const int groupIndex { Arguments::getInteger (replacementText.substr (digitsBegin, digitsEnd - digitsBegin)).value_or (std::numeric_limits<int>::max()) };
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

    if (jam::Document::isDigit (static_cast<unsigned char> (replacementText.at (nextIndex))))
        return { replacementText.at (nextIndex) - static_cast<char> (Chars::zero), dollarDigitSpan };

    if (replacementText.at (nextIndex) == static_cast<char> (Chars::openBrace))
        return getBracedReference (replacementText, index);

    return { noCapture, 0 };
}

static jam::Array<std::pair<int, juce::Range<int>>> getReplacementTokens (std::string_view replacementText)
{
    static constexpr std::string_view escapedDollar { "$$" };
    jam::Array<std::pair<int, juce::Range<int>>> replacementTokens;
    size_t index { 0 };

    while (index < replacementText.size())
    {
        const auto [groupIndex, referenceSpan] { getCaptureReference (replacementText, index) };

        if (groupIndex != noCapture)
        {
            replacementTokens.add ({ groupIndex, juce::Range<int> (static_cast<int> (index), static_cast<int> (index + referenceSpan)) });
            index += referenceSpan;
        }
        else
        {
            const size_t literalSpan { replacementText.compare (index, escapedDollar.size(), escapedDollar) == 0 ? escapedDollar.size() : size_t { 1 } };
            const int literalStart { static_cast<int> (index + literalSpan - 1) };

            if (replacementTokens.isEmpty())
            {
                replacementTokens.add ({ noCapture, juce::Range<int> (literalStart, literalStart + 1) });
            }
            else
            {
                auto& [lastGroupIndex, lastRange] { replacementTokens.last() };

                if (lastGroupIndex == noCapture and lastRange.getEnd() == literalStart)
                    lastRange.setEnd (literalStart + 1);
                else
                    replacementTokens.add ({ noCapture, juce::Range<int> (literalStart, literalStart + 1) });
            }

            index += literalSpan;
        }
    }

    return replacementTokens;
}

static juce::Result getCaptureValidationStatus (const jam::Array<std::pair<int, juce::Range<int>>>& replacementTokens, int numCaptureGroups)
{
    for (const auto& [groupIndex, tokenRange] : replacementTokens)
    {
        if (groupIndex > numCaptureGroups)
            return Stdio::fail (text::Diagnostics::failCaptureInvalid, groupIndex, text::Diagnostics::inReplacement);
    }

    return juce::Result::ok();
}

static std::tuple<juce::Result, std::unique_ptr<TextDocument>, std::unique_ptr<Input>, std::string_view> getReplacementText (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.contains (Id::withStdinOption))
    {
        auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

        if (inputStatus.failed())
            return { inputStatus, nullptr, nullptr, std::string_view {} };

        const auto standardBytes { standardInput->getBytes() };
        return { juce::Result::ok(), nullptr, std::move (standardInput), standardBytes };
    }

    if (arguments.contains (Id::withFileOption))
    {
        const auto filePath { arguments.getValueView (Id::withFileOption) };
        const auto targetFile { Paths::getLinkedTarget (filePath) };
        auto [loadStatus, document] { TextDocument::fromTextFile (targetFile, filePath, Input::maxBufferedReadSize) };

        if (loadStatus.failed())
            return { loadStatus, nullptr, nullptr, std::string_view {} };

        const auto documentBytes { document->getBytes() };
        return { juce::Result::ok(), std::move (document), nullptr, documentBytes };
    }

    return { juce::Result::ok(), nullptr, nullptr, arguments.getPositionals().at (1) };
}

static int getPathStartIndex (bool hasWithStdin, bool hasWithFile) noexcept
{
    if (hasWithStdin or hasWithFile)
        return 1;

    return 2;
}

Replace::Replace (const Arguments& arguments, std::unique_ptr<Pattern> newPattern, std::unique_ptr<TextDocument> newReplacementDocument, std::unique_ptr<Input> newReplacementInput, std::string_view newReplacementText, jam::Array<std::pair<int, juce::Range<int>>> newReplacementTokens, juce::Range<int> newLineRange)
    : pattern (std::move (newPattern)),
      replacementDocument (std::move (newReplacementDocument)),
      replacementInput (std::move (newReplacementInput)),
      replacementText (newReplacementText),
      replacementTokens (std::move (newReplacementTokens)),
      lineRange (newLineRange),
      isRegex (arguments.contains (Id::regexOption))
{
}

std::pair<juce::Result, int> Replace::appendReplacedLine (std::string& output, std::string_view line) const
{
    size_t offset { 0 };
    size_t printedUpTo { 0 };
    int matchCount { 0 };

    while (offset <= line.size())
    {
        auto [findStatus, captures] { pattern->findNext (line, offset) };

        if (findStatus.failed())
            return { findStatus, 0 };

        if (captures.isEmpty())
            break;

        const auto match { *captures.at (0) };
        const size_t matchStart { static_cast<size_t> (match.data() - line.data()) };
        const size_t matchEnd { matchStart + match.size() };
        jam::Format::appendBytes (output, line.substr (printedUpTo, matchStart - printedUpTo));
        appendExpandedReplacement (output, captures);
        ++matchCount;
        printedUpTo = matchEnd;
        offset = match.empty() ? jam::RegExp::getNextPosition (line, matchEnd, true) : matchEnd;
    }

    jam::Format::appendBytes (output, line.substr (printedUpTo));
    return { juce::Result::ok(), matchCount };
}

void Replace::appendExpandedReplacement (std::string& output, const Pattern::Captures& captures) const
{
    if (not isRegex)
    {
        jam::Format::appendBytes (output, replacementText);
    }
    else
    {
        for (const auto& [groupIndex, tokenRange] : replacementTokens)
        {
            if (groupIndex == noCapture)
                jam::Format::appendBytes (output, replacementText.substr (static_cast<size_t> (tokenRange.getStart()), static_cast<size_t> (tokenRange.getLength())));
            else
                jam::Format::appendBytes (output, captures.at (groupIndex).value_or (std::string_view {}));
        }
    }
}

std::tuple<juce::Result, LineDiff, int> Replace::getDocumentDiff (const TextDocument& document, size_t firstCandidate, jam::RegExpAutomaton& automaton) const
{
    juce::Result status { juce::Result::ok() };
    int totalEdits { 0 };
    std::string replacedLine;
    const auto activeRange { lineRange.getIntersectionWith (juce::Range<int> (1, document.getNumLines() + 1)) };
    const auto bytes { document.getBytes() };
    const size_t firstOffset { activeRange.isEmpty() ? bytes.size() : static_cast<size_t> (document.getLine (activeRange.getStart()).data() - bytes.data()) };

    LineDiff diff (document, [this, &document, &automaton, &bytes, &activeRange, firstOffset, firstCandidate, &status, &totalEdits, &replacedLine] (const auto& addChange)
    {
        size_t candidateOffset { firstCandidate >= firstOffset ? firstCandidate : pattern->findCandidate (bytes, firstOffset, automaton) };

        while (candidateOffset < bytes.size() and status.wasOk())
        {
            const int lineNumber { document.getLineNumber (candidateOffset) };

            if (lineNumber >= activeRange.getEnd())
                break;

            replacedLine.clear();
            const auto [lineStatus, lineMatchCount] { appendReplacedLine (replacedLine, document.getLine (lineNumber)) };
            status = lineStatus;

            if (lineMatchCount > 0)
            {
                addChange (lineNumber - 1, 1, std::string_view { replacedLine });
                totalEdits += lineMatchCount;
            }

            candidateOffset = lineNumber < document.getNumLines()
                                  ? pattern->findCandidate (bytes, static_cast<size_t> (document.getLine (lineNumber + 1).data() - bytes.data()), automaton)
                                  : bytes.size();
        }
    });

    return { status, std::move (diff), totalEdits };
}

std::pair<juce::Result, std::optional<Replace::Replacement>> Replace::getReplacedFile (juce::File targetFile,
                                                                                       std::string_view displayPath,
                                                                                       bool isExplicit,
                                                                                       const Input::Source& source) const
{
    auto [inputStatus, input] { TextDocument::getInput (targetFile, displayPath, Input::allBytes, source) };

    if (inputStatus.failed())
        return { inputStatus, std::nullopt };

    auto document { TextDocument::fromInput (std::move (input), displayPath) };

    if (document->isBinary())
        return { isExplicit ? TextDocument::getBinaryFailure (displayPath) : juce::Result::ok(), std::nullopt };

    jam::RegExpAutomaton automaton;

    const size_t firstCandidate { pattern->findCandidate (document->getBytes(), 0, automaton) };

    if (firstCandidate >= document->getBytes().size())
        return { juce::Result::ok(), std::nullopt };

    auto [diffStatus, diff, editCount] { getDocumentDiff (*document, firstCandidate, automaton) };

    if (diffStatus.failed())
        return { diffStatus, std::nullopt };

    return { juce::Result::ok(), Replace::Replacement { std::move (targetFile), std::move (document), std::move (diff), editCount } };
}

Stdio::Output Replace::runStandardInput (const Arguments& arguments, const Input::Source& input) const
{
    if (arguments.contains (Id::applyOption))
        return Stdio::getFailure (text::Diagnostics::failApplyStandardInput);

    auto [inputStatus, standardInput] { TextDocument::getInput (input, Input::allLines) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    if (standardInput->getBytes().empty())
        return Stdio::getOutput (Stdio::exitSuccess);

    auto document { TextDocument::fromInput (std::move (standardInput), Id::stdinPath) };

    if (document->isBinary())
        return Stdio::getFailure (TextDocument::getBinaryFailure (document->getDisplayPath()));

    jam::RegExpAutomaton automaton;
    const size_t firstCandidate { pattern->findCandidate (document->getBytes(), 0, automaton) };
    auto [diffStatus, diff, editCount] { getDocumentDiff (*document, firstCandidate, automaton) };

    if (diffStatus.failed())
        return Stdio::getFailure (diffStatus);

    Transaction transaction { Id::replace };
    const auto addStatus { transaction.addFile (juce::File {}, std::move (document), std::move (diff), editCount) };

    if (addStatus.failed())
        return Stdio::getFailure (addStatus);

    return transaction.run (arguments);
}

Stdio::Output Replace::runFiles (const Arguments& arguments, juce::Span<const std::string_view> rawPaths, const Input::Source& source) const
{
    const auto [readStatus, files] { Paths::getReadableFiles (rawPaths,
                                                              arguments.getValues (Id::includeOption),
                                                              arguments.getValues (Id::excludeOption),
                                                              arguments.contains (Id::hiddenOption),
                                                              arguments.contains (Id::noIgnoreOption)) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    const int fileCount { files.size() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount);
    jam::Array<std::optional<Replacement>> replacements;
    replacements.resize (fileCount);

    const bool isStandardInputNamed { std::any_of (files.begin(), files.end(), [] (const Paths::Entry& entry)
    {
        return std::holds_alternative<Input::StandardInput> (entry);
    }) };

    if (isStandardInputNamed and arguments.contains (Id::applyOption))
        return Stdio::getFailure (text::Diagnostics::failApplyStandardInput);

    if (isStandardInputNamed and arguments.contains (Id::withStdinOption))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    Jobs::run (fileCount,
               [this, &pathEntries = files, &source, &statuses, &replacements] (int index)
               {
                   const auto& entry { pathEntries.at (index) };
                   auto [fileStatus, replacement] { getReplacedFile (Paths::getFile (entry), Paths::getDisplayPath (entry), Paths::isExplicit (entry), source) };
                   statuses.at (index) = std::move (fileStatus);
                   replacements.at (index) = std::move (replacement);
               });

    const juce::Result firstFailure { Stdio::getFirstFailure (statuses) };

    if (firstFailure.failed())
        return Stdio::getFailure (firstFailure);

    Transaction transaction { Id::replace };

    for (int index { 0 }; index < fileCount; ++index)
    {
        if (replacements.at (index).has_value())
        {
            auto& [targetFile, document, diff, editCount] { *replacements.at (index) };

            if (transaction.addTarget (targetFile))
            {
                const auto addStatus { transaction.addFile (targetFile, std::move (document), std::move (diff), editCount) };

                if (addStatus.failed())
                    return Stdio::getFailure (addStatus);
            }
        }
    }

    return transaction.run (arguments);
}

Arguments Replace::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList,
                       { Id::regexOption, Id::ignoreCaseOption, Id::withStdinOption, Id::applyOption, Id::hiddenOption, Id::noIgnoreOption },
                       { Id::linesOption, Id::withFileOption, Id::includeOption, Id::excludeOption, Id::expectOption },
                       {} };
}

Stdio::Output Replace::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    const bool hasWithStdin { arguments.contains (Id::withStdinOption) };
    const bool hasWithFile { arguments.contains (Id::withFileOption) };

    if (hasWithStdin and hasWithFile)
        return Stdio::getFailure (text::Diagnostics::failWithSourcesConflict);

    const int pathStartIndex { getPathStartIndex (hasWithStdin, hasWithFile) };

    if (arguments.getPositionals().size() < pathStartIndex)
        return Stdio::getFailure (text::Diagnostics::failReplaceRequiresArguments);

    const auto rawPaths { arguments.getPositionals (pathStartIndex) };

    if (rawPaths.empty() and (hasWithStdin or Stdio::isTerminalInput (input)))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    const auto [rangeStatus, lineRange] { arguments.getLineRangeFilter() };

    if (rangeStatus.failed())
        return Stdio::getFailure (rangeStatus);

    const bool isRegex { arguments.contains (Id::regexOption) };
    const std::string_view patternText { arguments.getPositionals().at (0) };
    auto [patternStatus, pattern] { Pattern::create (juce::Span<const std::string_view> { &patternText, 1 }, isRegex, arguments.contains (Id::ignoreCaseOption), false) };

    if (patternStatus.failed())
        return Stdio::getFailure (patternStatus);

    auto [replacementStatus, replacementDocument, replacementInput, replacementText] { getReplacementText (arguments, input) };

    if (replacementStatus.failed())
        return Stdio::getFailure (replacementStatus);

    auto replacementTokens (isRegex ? getReplacementTokens (replacementText) : jam::Array<std::pair<int, juce::Range<int>>> {});

    if (isRegex)
    {
        const auto captureStatus { getCaptureValidationStatus (replacementTokens, pattern->getNumCaptureGroups()) };

        if (captureStatus.failed())
            return Stdio::getFailure (captureStatus);
    }

    const Replace replace { arguments, std::move (pattern), std::move (replacementDocument), std::move (replacementInput), replacementText, std::move (replacementTokens), lineRange };

    if (rawPaths.empty())
        return replace.runStandardInput (arguments, input);

    return replace.runFiles (arguments, rawPaths, input);
}
