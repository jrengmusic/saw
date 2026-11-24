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

static jam::Strings getRawPaths (const jam::Strings& positionals)
{
    jam::Strings rawPaths;
    rawPaths.addArray (positionals, 1, -1);
    return rawPaths;
}

static juce::Result getReadModeFlagStatus (const Arguments& arguments)
{
    const std::vector<std::string_view> mutateOnlyOptions { Id::stdinOption,
                                                            Id::verifyHashOption,
                                                            Id::applyOption,
                                                            Id::expectOption };

    for (const auto option : mutateOnlyOptions)
    {
        if (arguments.contains (option))
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (option).toRawUTF8()));
    }

    return juce::Result::ok();
}

static std::pair<juce::Result, juce::Range<int>> getValidatedLineRange (const Arguments& arguments)
{
    if (getMutateFlagCount (arguments) > 1)
        return { juce::Result::fail (text::Diagnostics::failMutateFlagsConflict), {} };

    const auto& positionals { arguments.getPositionals() };

    if (positionals.size() < 1)
        return { juce::Result::fail (text::Diagnostics::failLinesRequiresRange), {} };

    return Arguments::getLineRange (positionals.at (0), arguments.contains (Id::insertAfterOption));
}

static void appendHashedLine (std::string& output, int lineNumber, std::string_view line)
{
    const auto lineHash { TextDocument::getLineHash (line) };
    jam::Format::appendDecimal (output, lineNumber);
    output.push_back (static_cast<char> (Chars::colon));
    jam::Format::appendBytes (output, jam::Strings::getView (lineHash));
    output.push_back (static_cast<char> (Chars::pipe));
    jam::Format::appendBytes (output, line);
    output.push_back (static_cast<char> (Chars::newline));
}

static void appendNumberedLine (std::string& output, int lineNumber, std::string_view line)
{
    jam::Format::appendDecimal (output, lineNumber);
    output.push_back (static_cast<char> (Chars::tab));
    jam::Format::appendBytes (output, line);
    output.push_back (static_cast<char> (Chars::newline));
}

static void appendPlainLine (std::string& output, std::string_view line)
{
    jam::Format::appendBytes (output, line);
    output.push_back (static_cast<char> (Chars::newline));
}

static void appendFormattedLine (std::string& output,
                                 int lineNumber,
                                 std::string_view line,
                                 bool isShowingHash,
                                 bool isShowingLineNumber)
{
    if (isShowingHash)
        appendHashedLine (output, lineNumber, line);
    else if (isShowingLineNumber)
        appendNumberedLine (output, lineNumber, line);
    else
        appendPlainLine (output, line);
}

static std::pair<size_t, std::string> getFormattedLines (std::string_view bytes,
                                                         juce::Range<int> lineRange,
                                                         bool isShowingHash,
                                                         bool isShowingLineNumber)
{
    if (bytes.empty() or lineRange.getStart() >= lineRange.getEnd())
        return { 0, {} };

    std::string output;
    const char* data { bytes.data() };
    const size_t size { bytes.size() };
    size_t cursor { 0 };
    int lineNumber { 1 };

    while (cursor < size and lineNumber < lineRange.getEnd())
    {
        const auto newlineIndex { jam::simd::findByte (data, size, cursor, static_cast<char> (Chars::newline)) };

        if (lineNumber >= lineRange.getStart())
            appendFormattedLine (output, lineNumber, TextDocument::getLine (bytes, cursor, newlineIndex), isShowingHash, isShowingLineNumber);

        cursor = newlineIndex < size ? newlineIndex + 1 : size;
        ++lineNumber;
    }

    return { cursor, std::move (output) };
}

static std::pair<juce::Result, std::string> getBytesOutput (std::string_view bytes,
                                                            std::string_view displayPath,
                                                            bool isExplicit,
                                                            juce::Range<int> lineRange,
                                                            bool isShowingHash,
                                                            bool isShowingLineNumber)
{
    if (bytes.empty())
        return { juce::Result::ok(), {} };

    auto [readSize, output] { getFormattedLines (bytes, lineRange, isShowingHash, isShowingLineNumber) };

    if (TextDocument::isBinary (bytes.substr (0, readSize)))
    {
        if (isExplicit)
            return { TextDocument::getBinaryFailure (jam::Strings::fromView (displayPath)), {} };

        return { juce::Result::ok(), {} };
    }

    return { juce::Result::ok(), std::move (output) };
}

static std::pair<juce::Result, std::string> getFileOutput (const juce::File& file,
                                                           std::string_view displayPath,
                                                           bool isExplicit,
                                                           juce::Range<int> lineRange,
                                                           bool isShowingHash,
                                                           bool isShowingLineNumber)
{
    const auto [inputStatus, input] { TextDocument::getInput (file, jam::Strings::fromView (displayPath), Input::maxBufferedReadSize) };

    if (inputStatus.failed())
        return { inputStatus, {} };

    return getBytesOutput (input->getBytes(), displayPath, isExplicit, lineRange, isShowingHash, isShowingLineNumber);
}

static Stdio::Output getFileOutputs (const jam::Array<juce::Result>& statuses,
                                     const jam::Array<std::string>& outputs)
{
    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
            return Stdio::getFailure (statuses.at (index).getErrorMessage());
    }

    return { Stdio::exitSuccess, Stdio::getJoinedOutput (outputs), {} };
}

static Stdio::Output runStdin (juce::Range<int> lineRange,
                               bool isShowingHash,
                               bool isShowingLineNumber,
                               const std::optional<std::string>& input)
{
    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    const auto [inputStatus, standardInput] { Input::create (input, lineRange.getEnd() - 1) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus.getErrorMessage());

    const auto inputText { standardInput->getBytes() };

    if (inputText.empty())
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    auto [outputStatus, output] { getBytesOutput (inputText, Id::stdinPath, true, lineRange, isShowingHash, isShowingLineNumber) };

    if (outputStatus.failed())
        return Stdio::getFailure (outputStatus.getErrorMessage());

    return { Stdio::exitSuccess, std::move (output), {} };
}

static Stdio::Output runParallelFilesLines (const std::vector<std::pair<std::string, juce::File>>& files,
                                            const jam::Array<bool>& isExplicitFlags,
                                            juce::Range<int> lineRange,
                                            bool isShowingHash,
                                            bool isShowingLineNumber)
{
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), static_cast<int> (files.size()));
    jam::Array<std::string> outputs;
    outputs.resize (static_cast<int> (files.size()));

    Jobs::run (static_cast<int> (files.size()),
               [&files, &isExplicitFlags, &statuses, &outputs, lineRange, isShowingHash, isShowingLineNumber] (int index)
               {
                   const auto& [displayPath, targetFile] { files.at (static_cast<size_t> (index)) };
                   auto [fileStatus, fileOutput] { getFileOutput (targetFile, displayPath, isExplicitFlags.at (index), lineRange, isShowingHash, isShowingLineNumber) };
                   statuses.at (index) = std::move (fileStatus);
                   outputs.at (index) = std::move (fileOutput);
               });

    return getFileOutputs (statuses, outputs);
}

static Stdio::Output runFiles (const jam::Strings& rawPaths,
                               juce::Range<int> lineRange,
                               bool isShowingHash,
                               bool isShowingLineNumber)
{
    const auto [readStatus, files, isExplicitFlags] { Paths::getReadableFiles (rawPaths, {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus.getErrorMessage());

    return runParallelFilesLines (files, isExplicitFlags, lineRange, isShowingHash, isShowingLineNumber);
}

static Stdio::Output runReadMode (const Arguments& arguments, juce::Range<int> lineRange, const std::optional<std::string>& input)
{
    const auto flagStatus { getReadModeFlagStatus (arguments) };

    if (flagStatus.failed())
        return Stdio::getFailure (flagStatus.getErrorMessage());

    const auto isShowingHash { arguments.contains (Id::hashOption) };
    const auto isShowingLineNumber { arguments.contains (Id::lineNumberOption) };
    const auto rawPaths { getRawPaths (arguments.getPositionals()) };

    if (rawPaths.size() == 0)
        return runStdin (lineRange, isShowingHash, isShowingLineNumber, input);

    return runFiles (rawPaths, lineRange, isShowingHash, isShowingLineNumber);
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, LineDiff::Change (juce::Range<int>, std::string_view)>& getLineMutations()
{
    static const auto lineMutations (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, LineDiff::Change (juce::Range<int>, std::string_view)> mutations;
            mutations.add (juce::Identifier (Id::deleteOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view) { return LineDiff::Change { range.getStart() - 1, range.getLength(), {} }; });
            mutations.add (juce::Identifier (Id::replaceOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::Change { range.getStart() - 1, range.getLength(), LineDiff::getLines (text) }; });
            mutations.add (juce::Identifier (Id::insertBeforeOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::Change { range.getStart() - 1, 0, LineDiff::getLines (text) }; });
            mutations.add (juce::Identifier (Id::insertAfterOption.data()), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::Change { range.getStart(), 0, LineDiff::getLines (text) }; });
            return mutations;
        }());

    return lineMutations;
}

static juce::Identifier getActiveMutationOption (const Arguments& arguments)
{
    for (const auto option : mutateOptions)
    {
        if (arguments.contains (option))
            return juce::Identifier (option.data());
    }

    assert (false);
    return {};
}

static juce::Result getMutateModeFlagStatus (const Arguments& arguments,
                                           const juce::Identifier& activeMutation)
{
    const std::vector<std::string_view> readOnlyOptions { Id::lineNumberOption, Id::hashOption };

    for (const auto option : readOnlyOptions)
    {
        if (arguments.contains (option))
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (option).toRawUTF8()));
    }

    if (activeMutation == juce::Identifier (Id::deleteOption.data()) and arguments.contains (Id::stdinOption))
        return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failFlagUnknown, Arguments::getOptionName (Id::stdinOption).toRawUTF8()));

    return juce::Result::ok();
}

static juce::Result getMutateValidationStatus (const Arguments& arguments,
                                               const juce::Identifier& activeMutation)
{
    const auto flagStatus { getMutateModeFlagStatus (arguments, activeMutation) };

    if (flagStatus.failed())
        return flagStatus;

    const auto mutationValueView { arguments.getValueView (activeMutation.toString().toRawUTF8()) };

    if (arguments.contains (Id::stdinOption) and not mutationValueView.empty())
        return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failUnexpectedArgument,
                                                        jam::Strings::fromView (mutationValueView).toRawUTF8()));

    const auto& positionals { arguments.getPositionals() };
    const bool isInsert { activeMutation == juce::Identifier (Id::insertBeforeOption.data())
                          or activeMutation == juce::Identifier (Id::insertAfterOption.data()) };

    if (isInsert and positionals.at (0).containsChar (Chars::colon))
        return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failInsertSingleLine, positionals.at (0).toRawUTF8()));

    if (positionals.size() < 2)
        return juce::Result::fail (text::Diagnostics::failMissingPath);

    return juce::Result::ok();
}

static std::pair<juce::Result, juce::Range<int>> getEffectiveMutationRange (const TextDocument& document,
                                                                            const juce::Identifier& activeMutation,
                                                                            juce::Range<int> lineRange,
                                                                            const juce::String& rangeText,
                                                                            const juce::String& displayPath)
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
        return { juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failLineRangeBounds,
                                                          rangeText.toRawUTF8(),
                                                          displayPath.toRawUTF8(),
                                                          numLines)),
                 {} };

    return { juce::Result::ok(), juce::Range<int> (lineRange.getStart(), effectiveEnd) };
}

static juce::Result getVerifiedHashStatus (const TextDocument& document,
                                           const Arguments& arguments,
                                           const juce::String& displayPath,
                                           int startLine)
{
    if (arguments.contains (Id::verifyHashOption))
    {
        const auto expectedHash { arguments.getValue (Id::verifyHashOption) };
        const auto actualHash { startLine >= 1 ? document.getLineHash (startLine) : juce::String {} };

        if (startLine < 1 or actualHash.compare (expectedHash) != 0)
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failHashMismatch,
                                                            displayPath.toRawUTF8(),
                                                            startLine,
                                                            expectedHash.toRawUTF8(),
                                                            actualHash.toRawUTF8()));
    }

    return juce::Result::ok();
}

static juce::Result addFileMutation (Transaction& transaction,
                                     const Arguments& arguments,
                                     const juce::Identifier& activeMutation,
                                     juce::Range<int> lineRange,
                                     std::string_view payloadText,
                                     const juce::File& targetFile,
                                     const juce::String& displayPath)
{
    auto [documentStatus, document] { TextDocument::fromTextFile (targetFile, displayPath, Input::allBytes) };

    if (documentStatus.failed())
        return documentStatus;

    const auto [rangeStatus, effectiveRange] { getEffectiveMutationRange (*document, activeMutation, lineRange, arguments.getPositionals().at (0), displayPath) };

    if (rangeStatus.failed())
        return rangeStatus;

    const auto hashStatus { getVerifiedHashStatus (*document, arguments, displayPath, lineRange.getStart()) };

    if (hashStatus.failed())
        return hashStatus;

    jam::Array<LineDiff::Change> changes;
    changes.add (getLineMutations().get (activeMutation, effectiveRange, payloadText));
    return transaction.addFile (targetFile, std::move (document), LineDiff { std::move (changes) }, 1);
}

static Stdio::Output runTransaction (const Arguments& arguments,
                                     const juce::Identifier& activeMutation,
                                     juce::Range<int> lineRange,
                                     std::string_view payloadText,
                                     const std::vector<std::pair<std::string, juce::File>>& files)
{
    Transaction transaction { Id::lines };
    std::set<juce::File> targetFiles;

    for (const auto& [filePath, targetFile] : files)
    {
        const auto displayPath { jam::Strings::fromView (filePath) };
        const auto [targetEntry, isInserted] { targetFiles.insert (targetFile) };
        juce::ignoreUnused (targetEntry);

        if (isInserted)
        {
            const auto fileStatus { addFileMutation (transaction, arguments, activeMutation, lineRange, payloadText, targetFile, displayPath) };

            if (fileStatus.failed())
                return Stdio::getFailure (fileStatus.getErrorMessage());
        }
    }

    return transaction.run (arguments, false);
}

static Stdio::Output runPayload (const Arguments& arguments,
                                 const juce::Identifier& activeMutation,
                                 juce::Range<int> lineRange,
                                 std::string_view payloadText)
{
    const auto [readStatus, files, isExplicitFlags] { Paths::getReadableFiles (getRawPaths (arguments.getPositionals()), {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus.getErrorMessage());

    juce::ignoreUnused (isExplicitFlags);
    return runTransaction (arguments, activeMutation, lineRange, payloadText, files);
}

static Stdio::Output runMutation (const Arguments& arguments, juce::Range<int> lineRange, const std::optional<std::string>& input)
{
    const auto activeMutation { getActiveMutationOption (arguments) };
    const auto validationStatus { getMutateValidationStatus (arguments, activeMutation) };

    if (validationStatus.failed())
        return Stdio::getFailure (validationStatus.getErrorMessage());

    if (arguments.contains (Id::stdinOption))
    {
        const auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

        if (inputStatus.failed())
            return Stdio::getFailure (inputStatus.getErrorMessage());

        return runPayload (arguments, activeMutation, lineRange, standardInput->getBytes());
    }

    return runPayload (arguments, activeMutation, lineRange, arguments.getValueView (activeMutation.toString().toRawUTF8()));
}

Arguments Lines::getArguments (const jam::Strings& argumentList)
{
    const std::vector<std::string_view> flagOptions { Id::lineNumberOption, Id::hashOption, Id::deleteOption, Id::stdinOption, Id::applyOption };
    const std::vector<std::string_view> valueOptions { Id::verifyHashOption, Id::expectOption };
    const std::vector<std::string_view> optionalValueOptions { Id::replaceOption, Id::insertBeforeOption, Id::insertAfterOption };

    return Arguments { argumentList, flagOptions, valueOptions, optionalValueOptions };
}

Stdio::Output Lines::run (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus().getErrorMessage());

    const auto [rangeStatus, lineRange] { getValidatedLineRange (arguments) };

    if (rangeStatus.failed())
        return Stdio::getFailure (rangeStatus.getErrorMessage());

    if (getMutateFlagCount (arguments) == 0)
        return runReadMode (arguments, lineRange, input);

    return runMutation (arguments, lineRange, input);
}
