#include "Lines.h"
#include "Arguments.h"
#include "Input.h"
#include "Jobs.h"
#include "LineDiff.h"
#include "Paths.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Transaction.h"
#include "generated/Generated.h"

static constexpr std::array mutateOptions { Id::deleteOption, Id::replaceOption, Id::insertBeforeOption, Id::insertAfterOption };

static int getMutateFlagCount (const Arguments& arguments)
{
    int count { 0 };

    for (const auto option : mutateOptions)
    {
        if (arguments.contains (option))
            ++count;
    }

    return count;
}

static juce::Result getReadModeFlagStatus (const Arguments& arguments)
{
    static constexpr std::array mutateOnlyOptions { Id::stdinOption, Id::verifyHashOption, Id::applyOption, Id::expectOption };

    for (const auto option : mutateOnlyOptions)
    {
        if (arguments.contains (option))
            return Stdio::fail (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (option));
    }

    return juce::Result::ok();
}

static std::pair<juce::Result, juce::Range<int>> getValidatedLineRange (const Arguments& arguments)
{
    if (getMutateFlagCount (arguments) > 1)
        return { Stdio::fail (text::Diagnostics::failMutateFlagsConflict), {} };

    const auto& positionals { arguments.getPositionals() };

    if (positionals.size() < 1)
        return { Stdio::fail (text::Diagnostics::failLinesRequiresRange), {} };

    return Arguments::getLineRange (positionals.at (0), arguments.contains (Id::insertAfterOption));
}

void Lines::appendFormattedLine (std::string& output, int lineNumber, std::string_view line, std::string_view ending) const
{
    TextDocument::appendLinePrefix (output, lineNumber, line, static_cast<char> (isShowingHash ? Chars::colon : Chars::tab), isShowingLineNumber, isShowingHash);
    jam::Format::appendBytes (output, std::string_view { line.data(), line.size() + ending.size() });
}

std::pair<juce::Result, Stdio::Documents> Lines::getOutputDocuments (std::unique_ptr<Input> input,
                                                                     std::string_view displayPath,
                                                                     bool isExplicit) const
{
    const std::string_view bytes { input->getBytes() };

    if (bytes.empty())
        return { juce::Result::ok(), Stdio::Documents {} };

    if (not isShowingLineNumber and not isShowingHash)
    {
        const size_t firstOffset { TextDocument::getLeadingLines (bytes, lineRange.getStart() - 1).size() };
        const std::string_view rest { bytes.substr (firstOffset) };
        const std::string_view rangeBytes { lineRange.getEnd() == Arguments::openRangeEnd
                                                ? rest
                                                : TextDocument::getLeadingLines (rest, lineRange.getLength()) };

        if (not isText and TextDocument::isBinary (bytes.substr (0, firstOffset + rangeBytes.size())))
            return { TextDocument::getBinaryStatus (displayPath, isExplicit), Stdio::Documents {} };

        Stdio::Documents documents;

        if (not rangeBytes.empty())
            documents.add (TextDocument::fromInput (std::move (input), {}, rangeBytes));

        return { juce::Result::ok(), std::move (documents) };
    }

    std::string output;
    const size_t readSize { TextDocument::applyToLines (bytes,
                                                        lineRange,
                                                        [this, bytes, &output] (int lineNumber, std::string_view line)
                                                        {
                                                            appendFormattedLine (output, lineNumber, line, TextDocument::getLineEnding (bytes, line));
                                                            return true;
                                                        }) };

    if (not isText and TextDocument::isBinary (bytes.substr (0, readSize)))
        return { TextDocument::getBinaryStatus (displayPath, isExplicit), Stdio::Documents {} };

    return { juce::Result::ok(), Stdio::getDocuments (std::move (output)) };
}

std::pair<juce::Result, Stdio::Documents> Lines::getFileOutput (const juce::File& file,
                                                                std::string_view displayPath,
                                                                bool isExplicit,
                                                                const Input::Source& source) const
{
    auto [inputStatus, input] { TextDocument::getInput (file, displayPath, Input::maxBufferedReadSize, source) };

    if (inputStatus.failed())
        return { inputStatus, Stdio::Documents {} };

    return getOutputDocuments (std::move (input), displayPath, isExplicit);
}

Stdio::Output Lines::runStandardInput (const Input::Source& input) const
{
    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    auto [inputStatus, standardInput] { TextDocument::getInput (input, lineRange.getEnd() - 1) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    auto [outputStatus, documents] { getOutputDocuments (std::move (standardInput), Id::stdinPath, true) };

    if (outputStatus.failed())
        return Stdio::getFailure (outputStatus);

    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

Stdio::Output Lines::runFiles (juce::Span<const std::string_view> rawPaths, const Input::Source& source) const
{
    const auto [readStatus, files] { Paths::getReadableFiles (rawPaths, {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    const int fileCount { files.size() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount);
    jam::Array<Stdio::Documents> results;
    results.resize (fileCount);

    Jobs::run (fileCount,
               [this, &pathEntries = files, &source, &statuses, &results] (int index)
               {
                   const auto& entry { pathEntries.at (index) };
                   auto [fileStatus, fileDocuments] { getFileOutput (Paths::getFile (entry), Paths::getDisplayPath (entry), Paths::isExplicit (entry), source) };
                   statuses.at (index) = std::move (fileStatus);
                   results.at (index) = std::move (fileDocuments);
               });

    const juce::Result firstFailure { Stdio::getFirstFailure (statuses) };

    if (firstFailure.failed())
        return Stdio::getFailure (firstFailure);

    Stdio::Documents documents;

    for (auto& fileDocuments : results)
        Stdio::addDocuments (documents, std::move (fileDocuments));

    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

static juce::Identifier getActiveMutationOption (const Arguments& arguments)
{
    for (const auto option : mutateOptions)
    {
        if (arguments.contains (option))
            return juce::Identifier (option.data());
    }

    return {};
}

Lines::Lines (const Arguments& arguments, juce::Range<int> newLineRange, std::unique_ptr<Input> newPayloadInput, std::string_view newPayloadText)
    : lineRange (newLineRange),
      rangeText (arguments.getPositionals().at (0)),
      activeMutation (getActiveMutationOption (arguments)),
      expectedHash (arguments.contains (Id::verifyHashOption) ? std::optional<std::string_view> { arguments.getValueView (Id::verifyHashOption) } : std::nullopt),
      payloadInput (std::move (newPayloadInput)),
      payloadText (newPayloadText),
      isShowingHash (arguments.contains (Id::hashOption)),
      isShowingLineNumber (arguments.contains (Id::lineNumberOption)),
      isText (arguments.contains (Id::textOption))
{
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, std::tuple<int, int, std::optional<std::string_view>> (juce::Range<int>, std::string_view)>& getLineMutations()
{
    static const auto lineMutations (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, std::tuple<int, int, std::optional<std::string_view>> (juce::Range<int>, std::string_view)> mutations;
            mutations.add (juce::Identifier (Id::deleteOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view) { return LineDiff::getDeletion (range); });
            mutations.add (juce::Identifier (Id::replaceOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::getReplacement (range, text); });
            mutations.add (juce::Identifier (Id::insertBeforeOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::getInsertion (range.getStart() - 1, text); });
            mutations.add (juce::Identifier (Id::insertAfterOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::getInsertion (range.getStart(), text); });
            return mutations;
        }());

    return lineMutations;
}

static juce::Result getMutateModeFlagStatus (const Arguments& arguments,
                                           const juce::Identifier& activeMutation)
{
    static constexpr std::array readOnlyOptions { Id::lineNumberOption, Id::hashOption, Id::textOption };

    for (const auto option : readOnlyOptions)
    {
        if (arguments.contains (option))
            return Stdio::fail (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (option));
    }

    if (activeMutation == juce::Identifier (Id::deleteOption.data()) and arguments.contains (Id::stdinOption))
        return Stdio::fail (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (Id::stdinOption));

    return juce::Result::ok();
}

static juce::Result getMutateValidationStatus (const Arguments& arguments,
                                               const juce::Identifier& activeMutation)
{
    const auto flagStatus { getMutateModeFlagStatus (arguments, activeMutation) };

    if (flagStatus.failed())
        return flagStatus;

    const auto mutationValueView { arguments.getValueView (jam::Strings::getView (activeMutation.toString())) };

    if (arguments.contains (Id::stdinOption) and not mutationValueView.empty())
        return Stdio::fail (text::Diagnostics::failUnexpectedArgument, mutationValueView);

    const auto& positionals { arguments.getPositionals() };
    const bool isInsert { activeMutation == juce::Identifier (Id::insertBeforeOption.data())
                          or activeMutation == juce::Identifier (Id::insertAfterOption.data()) };

    if (isInsert and positionals.at (0).find (static_cast<char> (Chars::colon)) != std::string_view::npos)
        return Stdio::fail (text::Diagnostics::failInsertSingleLine, positionals.at (0));

    if (positionals.size() < 2)
        return Stdio::fail (text::Diagnostics::failMissingPath);

    return juce::Result::ok();
}

std::pair<juce::Result, juce::Range<int>> Lines::getEffectiveMutationRange (const TextDocument& document, std::string_view displayPath) const
{
    const int numLines { document.getNumLines() };
    const int effectiveEnd { lineRange.getEnd() == Arguments::openRangeEnd ? numLines + 1 : lineRange.getEnd() };
    const bool isInsertAfter { activeMutation == juce::Identifier (Id::insertAfterOption.data()) };
    const bool isInBounds { isInsertAfter
                                ? (lineRange.getStart() >= 0 and lineRange.getStart() <= numLines)
                                : (numLines > 0
                                   and lineRange.getStart() >= 1
                                   and effectiveEnd - 1 <= numLines
                                   and lineRange.getStart() <= effectiveEnd - 1) };

    if (not isInBounds)
        return { Stdio::fail (text::Diagnostics::failLineRangeBounds,
                              rangeText,
                              text::Diagnostics::outOfBoundsFor,
                              displayPath,
                              text::Diagnostics::linesOpen,
                              numLines,
                              text::Diagnostics::linesClose),
                 {} };

    return { juce::Result::ok(), juce::Range<int> (lineRange.getStart(), effectiveEnd) };
}

juce::Result Lines::getVerifiedHashStatus (const TextDocument& document) const
{
    if (expectedHash.has_value())
        return document.getLineHashStatus (lineRange.getStart(), *expectedHash);

    return juce::Result::ok();
}

juce::Result Lines::addFileMutation (Transaction& transaction, const juce::File& targetFile, std::string_view displayPath) const
{
    auto [documentStatus, document] { TextDocument::fromTextFile (targetFile, displayPath, Input::allBytes) };

    if (documentStatus.failed())
        return documentStatus;

    const auto [rangeStatus, effectiveRange] { getEffectiveMutationRange (*document, displayPath) };

    if (rangeStatus.failed())
        return rangeStatus;

    const auto hashStatus { getVerifiedHashStatus (*document) };

    if (hashStatus.failed())
        return hashStatus;

    const auto mutation { getLineMutations().get (activeMutation, effectiveRange, payloadText) };
    LineDiff diff (*document, [&mutation] (const auto& addChange) { std::apply (addChange, mutation); }, payloadInput != nullptr);
    return transaction.addFile (targetFile, std::move (document), std::move (diff), 1);
}

Stdio::Output Lines::runMutation (const Arguments& arguments) const
{
    const auto [readStatus, files] { Paths::getReadableFiles (arguments.getPositionals (1), {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    Transaction transaction { Id::lines };

    for (const auto& entry : files)
    {
        if (std::holds_alternative<Input::StandardInput> (entry))
            return Stdio::getFailure (text::Diagnostics::failMissingPath);

        const auto targetFile { Paths::getFile (entry) };

        if (transaction.addTarget (targetFile))
        {
            const auto fileStatus { addFileMutation (transaction, targetFile, Paths::getDisplayPath (entry)) };

            if (fileStatus.failed())
                return Stdio::getFailure (fileStatus);
        }
    }

    return transaction.run (arguments);
}

static std::tuple<juce::Result, std::unique_ptr<Input>, std::string_view> getPayloadText (const Arguments& arguments,
                                                                                         const juce::Identifier& activeMutation,
                                                                                          const Input::Source& input)
{
    if (arguments.contains (Id::stdinOption))
    {
        auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

        if (inputStatus.failed())
            return { inputStatus, nullptr, std::string_view {} };

        const std::string_view standardBytes { standardInput->getBytes() };
        return { juce::Result::ok(), std::move (standardInput), standardBytes };
    }

    return { juce::Result::ok(), nullptr, LineDiff::getTextWithoutSingleTrailingNewline (arguments.getValueView (jam::Strings::getView (activeMutation.toString()))) };
}

Arguments Lines::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList,
                       { Id::lineNumberOption, Id::hashOption, Id::deleteOption, Id::stdinOption, Id::applyOption, Id::textOption },
                       { Id::verifyHashOption, Id::expectOption },
                       { Id::replaceOption, Id::insertBeforeOption, Id::insertAfterOption } };
}

Stdio::Output Lines::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    const auto [rangeStatus, lineRange] { getValidatedLineRange (arguments) };

    if (rangeStatus.failed())
        return Stdio::getFailure (rangeStatus);

    if (getMutateFlagCount (arguments) == 0)
    {
        const auto flagStatus { getReadModeFlagStatus (arguments) };

        if (flagStatus.failed())
            return Stdio::getFailure (flagStatus);

        const Lines lines { arguments, lineRange, {}, {} };
        const auto rawPaths { arguments.getPositionals (1) };

        if (rawPaths.empty())
            return lines.runStandardInput (input);

        return lines.runFiles (rawPaths, input);
    }

    const auto activeMutation { getActiveMutationOption (arguments) };
    const auto validationStatus { getMutateValidationStatus (arguments, activeMutation) };

    if (validationStatus.failed())
        return Stdio::getFailure (validationStatus);

    auto [payloadStatus, payloadInput, payloadText] { getPayloadText (arguments, activeMutation, input) };

    if (payloadStatus.failed())
        return Stdio::getFailure (payloadStatus);

    const Lines lines { arguments, lineRange, std::move (payloadInput), payloadText };
    return lines.runMutation (arguments);
}
