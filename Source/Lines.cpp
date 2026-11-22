#include "Lines.h"
#include "Arguments.h"
#include "Jobs.h"
#include "LineDiff.h"
#include "Paths.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Transaction.h"
#include "generated/Generated.h"

static jam::Strings getFlagOptions (bool hasStdinFlag)
{
    jam::Strings options { Id::lineNumberOption,
                           Id::hashOption,
                           Id::deleteOption,
                           Id::stdinOption,
                           Id::applyOption };

    if (hasStdinFlag)
    {
        options.add (Id::replaceOption);
        options.add (Id::insertBeforeOption);
        options.add (Id::insertAfterOption);
    }

    return options;
}

static jam::Strings getValueOptions (bool hasStdinFlag)
{
    jam::Strings options { Id::verifyHashOption, Id::expectOption };

    if (not hasStdinFlag)
    {
        options.add (Id::replaceOption);
        options.add (Id::insertBeforeOption);
        options.add (Id::insertAfterOption);
    }

    return options;
}

static int getMutateFlagCount (const Arguments& arguments)
{
    const jam::Strings mutateOptions { Id::deleteOption,
                                       Id::replaceOption,
                                       Id::insertBeforeOption,
                                       Id::insertAfterOption };
    int count { 0 };

    for (int index { 0 }; index < mutateOptions.size(); ++index)
    {
        if (arguments.contains (mutateOptions.at (index)))
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
    const jam::Strings mutateOnlyOptions { Id::stdinOption,
                                           Id::verifyHashOption,
                                           Id::applyOption,
                                           Id::expectOption };

    for (int index { 0 }; index < mutateOnlyOptions.size(); ++index)
    {
        const auto option { mutateOnlyOptions.at (index) };

        if (arguments.contains (option))
            return juce::Result::fail (juce::String::formatted (text::Diagnostics::failFlagUnknown, option.toRawUTF8()));
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
    const auto lineHash { juce::SHA256 (line.data(), line.size()).toHexString().substring (0, TextDocument::lineHashLength) };
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

static std::string getFormattedLines (std::string_view bytes,
                                      juce::Range<int> lineRange,
                                      bool isShowingHash,
                                      bool isShowingLineNumber)
{
    if (bytes.empty() or lineRange.getStart() >= lineRange.getEnd())
        return {};

    std::string output;
    const char* data { bytes.data() };
    const size_t size { bytes.size() };
    size_t cursor { 0 };
    int lineNumber { 1 };

    while (cursor < size)
    {
        const auto newlineIndex { jam::simd::findByte (data, size, cursor, static_cast<char> (Chars::newline)) };

        if (lineNumber >= lineRange.getStart() and lineNumber < lineRange.getEnd())
        {
            auto line { bytes.substr (cursor, newlineIndex - cursor) };

            if (newlineIndex < size and not line.empty() and line.back() == static_cast<char> (Chars::carriageReturn))
                line.remove_suffix (1);

            appendFormattedLine (output, lineNumber, line, isShowingHash, isShowingLineNumber);
        }

        if (lineNumber >= lineRange.getEnd() - 1)
            break;

        cursor = newlineIndex < size ? newlineIndex + 1 : size;
        ++lineNumber;
    }

    return output;
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

    if (jam::simd::findByte (bytes.data(), bytes.size(), 0, '\0') < bytes.size())
    {
        if (isExplicit)
            return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failBinaryFile, jam::Strings::fromView (displayPath).toRawUTF8())), {} };

        return { juce::Result::ok(), {} };
    }

    return { juce::Result::ok(), getFormattedLines (bytes, lineRange, isShowingHash, isShowingLineNumber) };
}

static std::pair<juce::Result, std::string> getFileOutput (const juce::File& file,
                                                           std::string_view displayPath,
                                                           bool isExplicit,
                                                           juce::Range<int> lineRange,
                                                           bool isShowingHash,
                                                           bool isShowingLineNumber)
{
    if (not file.existsAsFile())
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())), {} };

    if (file.getSize() == 0)
        return { juce::Result::ok(), {} };

    const juce::MemoryMappedFile mapped { file, juce::MemoryMappedFile::readOnly };

    if (mapped.getData() == nullptr)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())), {} };

    const std::string_view bytes { static_cast<const char*> (mapped.getData()), mapped.getSize() };
    return getBytesOutput (bytes, displayPath, isExplicit, lineRange, isShowingHash, isShowingLineNumber);
}

static int writeFileOutputs (const jam::Array<juce::Result>& statuses,
                             const jam::Array<std::string>& outputs)
{
    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
        {
            Stdio::printError (statuses.at (index).getErrorMessage());
            return Stdio::exitError;
        }
    }

    for (int index { 0 }; index < outputs.size(); ++index)
    {
        if (not outputs.at (index).empty())
            Stdio::write (outputs.at (index));
    }

    return Stdio::exitSuccess;
}

static int runStdin (juce::Range<int> lineRange,
                     bool isShowingHash,
                     bool isShowingLineNumber)
{
    if (Stdio::isTerminalInput())
    {
        Stdio::printError (text::Diagnostics::failMissingPath);
        return Stdio::exitError;
    }

    auto [inputStatus, inputText] { Stdio::getInput() };

    if (inputStatus.failed())
    {
        Stdio::printError (inputStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto [outputStatus, output] { getBytesOutput (inputText, jam::Strings::getView (Id::stdinPath), true, lineRange, isShowingHash, isShowingLineNumber) };

    if (outputStatus.failed())
    {
        Stdio::printError (outputStatus.getErrorMessage());
        return Stdio::exitError;
    }

    Stdio::write (output);
    return Stdio::exitSuccess;
}

static int runFiles (const jam::Strings& rawPaths,
                     juce::Range<int> lineRange,
                     bool isShowingHash,
                     bool isShowingLineNumber)
{
    const auto [filesStatus, filePaths, isExplicitFlags] { Paths::getFiles (rawPaths, {}, {}, false, false) };

    if (filesStatus.failed())
    {
        Stdio::printError (filesStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), static_cast<int> (filePaths.size()));
    jam::Array<std::string> outputs;
    outputs.resize (static_cast<int> (filePaths.size()));

    const auto& paths { filePaths };
    const auto& explicitFlags { isExplicitFlags };

    Jobs::run (static_cast<int> (paths.size()),
               [&paths, &explicitFlags, &workingDirectory, &statuses, &outputs, lineRange, isShowingHash, isShowingLineNumber] (int index)
               {
                   const auto displayPath { std::string_view { paths.at (static_cast<size_t> (index)) } };
                   const auto targetFile { workingDirectory.getChildFile (jam::Strings::fromView (displayPath)).getLinkedTarget() };
                   auto [fileStatus, fileOutput] { getFileOutput (targetFile, displayPath, explicitFlags.at (index), lineRange, isShowingHash, isShowingLineNumber) };
                   statuses.at (index) = std::move (fileStatus);
                   outputs.at (index) = std::move (fileOutput);
               });

    return writeFileOutputs (statuses, outputs);
}

static int runReadMode (const Arguments& arguments, juce::Range<int> lineRange)
{
    const auto flagStatus { getReadModeFlagStatus (arguments) };

    if (flagStatus.failed())
    {
        Stdio::printError (flagStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto isShowingHash { arguments.contains (Id::hashOption) };
    const auto isShowingLineNumber { arguments.contains (Id::lineNumberOption) };
    const auto rawPaths { getRawPaths (arguments.getPositionals()) };

    if (rawPaths.size() == 0)
        return runStdin (lineRange, isShowingHash, isShowingLineNumber);

    return runFiles (rawPaths, lineRange, isShowingHash, isShowingLineNumber);
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, LineDiff::Change (juce::Range<int>, std::string_view)>& getLineMutations()
{
    static const auto lineMutations (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, LineDiff::Change (juce::Range<int>, std::string_view)> mutations;
            mutations.add (juce::Identifier (Id::deleteOption), [] (const juce::Identifier&, juce::Range<int> range, std::string_view) { return LineDiff::Change { range.getStart() - 1, range.getLength(), {} }; });
            mutations.add (juce::Identifier (Id::replaceOption), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::Change { range.getStart() - 1, range.getLength(), LineDiff::getLines (text) }; });
            mutations.add (juce::Identifier (Id::insertBeforeOption), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::Change { range.getStart() - 1, 0, LineDiff::getLines (text) }; });
            mutations.add (juce::Identifier (Id::insertAfterOption), [] (const juce::Identifier&, juce::Range<int> range, std::string_view text) { return LineDiff::Change { range.getStart(), 0, LineDiff::getLines (text) }; });
            return mutations;
        }());

    return lineMutations;
}

static juce::Identifier getActiveMutationOption (const Arguments& arguments)
{
    const jam::Strings mutateOptions { Id::deleteOption,
                                       Id::replaceOption,
                                       Id::insertBeforeOption,
                                       Id::insertAfterOption };

    for (int index { 0 }; index < mutateOptions.size(); ++index)
    {
        if (arguments.contains (mutateOptions.at (index)))
            return juce::Identifier (mutateOptions.at (index));
    }

    assert (false);
    return {};
}

static juce::Result getMutateModeFlagStatus (const Arguments& arguments,
                                           const juce::Identifier& activeMutation)
{
    const jam::Strings readOnlyOptions { Id::lineNumberOption, Id::hashOption };

    for (int index { 0 }; index < readOnlyOptions.size(); ++index)
    {
        const auto option { readOnlyOptions.at (index) };

        if (arguments.contains (option))
            return juce::Result::fail (juce::String::formatted (text::Diagnostics::failFlagUnknown, option.toRawUTF8()));
    }

    if (activeMutation == juce::Identifier (Id::deleteOption) and arguments.contains (Id::stdinOption))
        return juce::Result::fail (juce::String::formatted (text::Diagnostics::failFlagUnknown, Id::stdinOption.toRawUTF8()));

    return juce::Result::ok();
}

static juce::Result getMutateValidationStatus (const Arguments& arguments,
                                               const juce::Identifier& activeMutation)
{
    const auto flagStatus { getMutateModeFlagStatus (arguments, activeMutation) };

    if (flagStatus.failed())
        return flagStatus;

    const auto& positionals { arguments.getPositionals() };
    const bool isInsert { activeMutation == juce::Identifier (Id::insertBeforeOption)
                          or activeMutation == juce::Identifier (Id::insertAfterOption) };

    if (isInsert and positionals.at (0).containsChar (Chars::colon))
        return juce::Result::fail (juce::String::formatted (text::Diagnostics::failInsertSingleLine, positionals.at (0).toRawUTF8()));

    if (positionals.size() < 2)
        return juce::Result::fail (text::Diagnostics::failMissingPath);

    return juce::Result::ok();
}

static std::pair<juce::Result, std::string> getMutationPayload (const Arguments& arguments,
                                                                const juce::Identifier& activeMutation)
{
    if (activeMutation == juce::Identifier (Id::deleteOption))
        return { juce::Result::ok(), {} };

    if (arguments.contains (Id::stdinOption))
        return Stdio::getInput();

    return { juce::Result::ok(), arguments.getValue (activeMutation.toString()).toStdString() };
}

static std::pair<juce::Result, juce::Range<int>> getEffectiveMutationRange (const TextDocument& document,
                                                                            const juce::Identifier& activeMutation,
                                                                            juce::Range<int> lineRange,
                                                                            const juce::String& rangeText,
                                                                            const juce::String& displayPath)
{
    const int numLines { document.getNumLines() };
    const int effectiveEnd { lineRange.getEnd() == Arguments::openRangeEnd ? numLines + 1 : lineRange.getEnd() };
    const bool isInsertAfter { activeMutation == juce::Identifier (Id::insertAfterOption) };
    const bool isInBounds { isInsertAfter
                                ? (lineRange.getStart() >= 0 and lineRange.getStart() <= numLines)
                                : (numLines > 0
                                   and lineRange.getStart() >= 1
                                   and effectiveEnd - 1 <= numLines
                                   and lineRange.getStart() <= effectiveEnd - 1) };

    if (not isInBounds)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failLineRangeBounds,
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
            return juce::Result::fail (juce::String::formatted (text::Diagnostics::failHashMismatch,
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
                                     const juce::String& displayPath)
{
    const auto targetFile { juce::File::getCurrentWorkingDirectory().getChildFile (displayPath).getLinkedTarget() };
    auto [documentStatus, document] { TextDocument::fromTextFile (targetFile, displayPath) };

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
    transaction.addFile (targetFile, std::move (document), LineDiff { std::move (changes) }, 1);
    return juce::Result::ok();
}

static int runTransaction (const Arguments& arguments,
                           const juce::Identifier& activeMutation,
                           juce::Range<int> lineRange,
                           std::string_view payloadText,
                           const std::vector<std::string>& filePaths)
{
    Transaction transaction { Id::lines.toString() };

    for (const auto& filePath : filePaths)
    {
        const auto fileStatus { addFileMutation (transaction, arguments, activeMutation, lineRange, payloadText, jam::Strings::fromView (filePath)) };

        if (fileStatus.failed())
        {
            Stdio::printError (fileStatus.getErrorMessage());
            return Stdio::exitError;
        }
    }

    return transaction.run (arguments, false);
}

static int runMutation (const Arguments& arguments, juce::Range<int> lineRange)
{
    const auto activeMutation { getActiveMutationOption (arguments) };
    const auto validationStatus { getMutateValidationStatus (arguments, activeMutation) };

    if (validationStatus.failed())
    {
        Stdio::printError (validationStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto [payloadStatus, payloadText] { getMutationPayload (arguments, activeMutation) };

    if (payloadStatus.failed())
    {
        Stdio::printError (payloadStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto [filesStatus, filePaths, isExplicitFlags] { Paths::getFiles (getRawPaths (arguments.getPositionals()), {}, {}, false, false) };

    if (filesStatus.failed())
    {
        Stdio::printError (filesStatus.getErrorMessage());
        return Stdio::exitError;
    }

    juce::ignoreUnused (isExplicitFlags);
    return runTransaction (arguments, activeMutation, lineRange, payloadText, filePaths);
}

int Lines::run (const jam::Strings& argumentList)
{
    const bool hasStdinFlag { Arguments::containsOptionBeforeTerminator (argumentList, 1, Id::stdinOption) };
    const Arguments arguments { argumentList, 1, getFlagOptions (hasStdinFlag), getValueOptions (hasStdinFlag) };

    if (arguments.getStatus().failed())
    {
        Stdio::printError (arguments.getStatus().getErrorMessage());
        return Stdio::exitError;
    }

    const auto [rangeStatus, lineRange] { getValidatedLineRange (arguments) };

    if (rangeStatus.failed())
    {
        Stdio::printError (rangeStatus.getErrorMessage());
        return Stdio::exitError;
    }

    if (getMutateFlagCount (arguments) == 0)
        return runReadMode (arguments, lineRange);

    return runMutation (arguments, lineRange);
}
