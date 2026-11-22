#include "Fields.h"
#include "Arguments.h"
#include "Jobs.h"
#include "Paths.h"
#include "Pattern.h"
#include "Stdio.h"
#include "generated/Generated.h"

static constexpr int firstOptionIndex { 1 };

static std::pair<juce::Result, int> getParsedFieldIndex (std::string_view tokenView, const juce::String& specText)
{
    if (tokenView.empty())
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failFieldSpecInvalid, specText.toRawUTF8())), 0 };

    int value { 0 };
    const auto [endPointer, errorCode] { std::from_chars (tokenView.data(), tokenView.data() + tokenView.size(), value) };

    if (errorCode != std::errc {} or endPointer != tokenView.data() + tokenView.size())
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failFieldSpecInvalid, specText.toRawUTF8())), 0 };

    if (value == 0)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failFieldSpecZero, specText.toRawUTF8())), 0 };

    return { juce::Result::ok(), value };
}

static std::pair<juce::Result, jam::Array<int>> getFieldIndices (const juce::String& specText)
{
    const std::string_view spec { specText.toRawUTF8() };
    constexpr auto comma { static_cast<char> (Chars::comma) };

    if (spec.empty() or spec.at (0) == comma or spec.at (spec.size() - 1) == comma)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failFieldSpecInvalid, specText.toRawUTF8())), jam::Array<int> {} };

    jam::Array<int> indices;
    size_t cursor { 0 };

    while (cursor < spec.size())
    {
        const auto commaIndex { spec.find (comma, cursor) };
        const auto tokenEnd { commaIndex == std::string_view::npos ? spec.size() : commaIndex };
        const auto [status, indexValue] { getParsedFieldIndex (spec.substr (cursor, tokenEnd - cursor), specText) };

        if (status.failed())
            return { status, jam::Array<int> {} };

        indices.add (indexValue);
        cursor = tokenEnd + 1;
    }

    return { juce::Result::ok(), std::move (indices) };
}

static std::string getOutputDelimiter (const Arguments& arguments, bool hasCustomDelimiter, const std::string& delimiter)
{
    if (arguments.contains (Id::outputDelimiterOption))
        return arguments.getValue (Id::outputDelimiterOption).toStdString();

    if (hasCustomDelimiter)
        return delimiter;

    return std::string { static_cast<char> (Chars::space) };
}

static std::tuple<juce::Result, jam::Array<int>, bool, std::string, std::string> getFieldSettings (const Arguments& arguments)
{
    const auto& positionals { arguments.getPositionals() };

    if (positionals.size() == 0)
        return { juce::Result::fail (text::Diagnostics::failFieldsRequiresSpec), jam::Array<int> {}, false, std::string {}, std::string {} };

    auto [indexStatus, fieldIndices] { getFieldIndices (positionals.at (0)) };

    if (indexStatus.failed())
        return { indexStatus, jam::Array<int> {}, false, std::string {}, std::string {} };

    const bool hasCustomDelimiter { arguments.contains (Id::delimiterOption) };
    const std::string delimiter { arguments.getValue (Id::delimiterOption).toStdString() };

    if (hasCustomDelimiter and delimiter.empty())
        return { juce::Result::fail (text::Diagnostics::failDelimiterEmpty), jam::Array<int> {}, false, std::string {}, std::string {} };

    return { juce::Result::ok(),
             std::move (fieldIndices),
             hasCustomDelimiter,
             delimiter,
             getOutputDelimiter (arguments, hasCustomDelimiter, delimiter) };
}

static std::pair<juce::Result, int> getAfterLine (const Arguments& arguments)
{
    if (arguments.contains (Id::afterOption))
        return arguments.getInteger (Id::afterOption, 0);

    return { juce::Result::ok(), 0 };
}

static std::pair<juce::Result, std::unique_ptr<Pattern>> getMatchPattern (const Arguments& arguments)
{
    if (arguments.contains (Id::matchOption))
        return Pattern::create (arguments.getValue (Id::matchOption), arguments.contains (Id::regexOption), false, false);

    return { juce::Result::ok(), nullptr };
}

static std::tuple<juce::Result, int, juce::Range<int>, std::unique_ptr<Pattern>> getFilterSettings (const Arguments& arguments)
{
    const auto [afterStatus, afterLine] { getAfterLine (arguments) };

    if (afterStatus.failed())
        return { afterStatus, 0, juce::Range<int> {}, std::unique_ptr<Pattern> {} };

    const auto [rangeStatus, lineRange] { arguments.getLineRangeFilter() };

    if (rangeStatus.failed())
        return { rangeStatus, 0, juce::Range<int> {}, std::unique_ptr<Pattern> {} };

    auto [patternStatus, matchPattern] { getMatchPattern (arguments) };

    if (patternStatus.failed())
        return { patternStatus, 0, juce::Range<int> {}, std::unique_ptr<Pattern> {} };

    return { juce::Result::ok(), afterLine, lineRange, std::move (matchPattern) };
}

static bool isFieldWhitespace (char byte) noexcept
{
    return byte == static_cast<char> (Chars::space) or byte == static_cast<char> (Chars::tab);
}

static void setWhitespaceTokens (jam::Array<std::string_view>& tokens, std::string_view line)
{
    size_t cursor { 0 };

    while (cursor < line.size())
    {
        while (cursor < line.size() and isFieldWhitespace (line.at (cursor)))
            ++cursor;

        if (cursor < line.size())
        {
            const size_t start { cursor };

            while (cursor < line.size() and not isFieldWhitespace (line.at (cursor)))
                ++cursor;

            tokens.add (line.substr (start, cursor - start));
        }
    }
}

static void setDelimitedTokens (jam::Array<std::string_view>& tokens, std::string_view line, std::string_view delimiter)
{
    assert (not line.empty() and not delimiter.empty());
    size_t cursor { 0 };

    if (delimiter.size() == 1)
    {
        const char* lineData { line.data() };
        const size_t lineSize { line.size() };
        const char delimiterByte { delimiter.at (0) };
        size_t matchIndex { jam::simd::findByte (lineData, lineSize, cursor, delimiterByte) };

        while (matchIndex < lineSize)
        {
            tokens.add (line.substr (cursor, matchIndex - cursor));
            cursor = matchIndex + 1;
            matchIndex = jam::simd::findByte (lineData, lineSize, cursor, delimiterByte);
        }

        tokens.add (line.substr (cursor));
    }
    else
    {
        size_t matchIndex { line.find (delimiter, cursor) };

        while (matchIndex != std::string_view::npos)
        {
            tokens.add (line.substr (cursor, matchIndex - cursor));
            cursor = matchIndex + delimiter.size();
            matchIndex = line.find (delimiter, cursor);
        }

        tokens.add (line.substr (cursor));
    }
}

static void setLineTokens (jam::Array<std::string_view>& tokens, std::string_view line, bool hasCustomDelimiter, std::string_view delimiter)
{
    tokens.clear();

    if (not line.empty())
    {
        if (hasCustomDelimiter)
            setDelimitedTokens (tokens, line, delimiter);
        else
            setWhitespaceTokens (tokens, line);
    }
}

static std::pair<juce::Result, std::string_view> getSelectedField (int requested,
                                                                   const jam::Array<std::string_view>& tokens,
                                                                   std::string_view displayPath,
                                                                   int lineNumber,
                                                                   bool isStrict)
{
    const int slotIndex { requested > 0 ? requested - 1 : tokens.size() + requested };

    if (slotIndex >= 0 and slotIndex < tokens.size())
        return { juce::Result::ok(), tokens.at (slotIndex) };

    if (isStrict)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failFieldBounds,
                                                              requested,
                                                              jam::Strings::fromView (displayPath).toRawUTF8(),
                                                              lineNumber)),
                 {} };

    return { juce::Result::ok(), {} };
}

static juce::Result appendFormattedLine (std::string& output,
                                         const jam::Array<int>& fieldIndices,
                                         const jam::Array<std::string_view>& tokens,
                                         std::string_view outputDelimiter,
                                         std::string_view displayPath,
                                         int lineNumber,
                                         bool isStrict)
{
    if (tokens.isEmpty() and not isStrict)
    {
        output.push_back (static_cast<char> (Chars::newline));
        return juce::Result::ok();
    }

    for (int index { 0 }; index < fieldIndices.size(); ++index)
    {
        if (index > 0)
            jam::Format::appendBytes (output, outputDelimiter);

        const auto [status, field] { getSelectedField (fieldIndices.at (index), tokens, displayPath, lineNumber, isStrict) };

        if (status.failed())
            return status;

        jam::Format::appendBytes (output, field);
    }

    output.push_back (static_cast<char> (Chars::newline));
    return juce::Result::ok();
}

static std::pair<juce::Result, bool> processLine (std::string& output,
                                                  jam::Array<std::string_view>& tokens,
                                                  std::string_view line,
                                                  const jam::Array<int>& fieldIndices,
                                                  bool hasCustomDelimiter,
                                                  std::string_view delimiter,
                                                  std::string_view outputDelimiter,
                                                  const Pattern* matchPattern,
                                                  std::string_view displayPath,
                                                  int lineNumber,
                                                  bool isStrict)
{
    if (matchPattern == nullptr or matchPattern->hasMatch (line))
    {
        setLineTokens (tokens, line, hasCustomDelimiter, delimiter);
        const auto status { appendFormattedLine (output, fieldIndices, tokens, outputDelimiter, displayPath, lineNumber, isStrict) };
        return { status, status.wasOk() };
    }

    return { juce::Result::ok(), false };
}

static std::tuple<juce::Result, bool, std::string> processBytes (std::string_view bytes,
                                                                 std::string_view displayPath,
                                                                 bool isExplicit,
                                                                 const jam::Array<int>& fieldIndices,
                                                                 bool hasCustomDelimiter,
                                                                 std::string_view delimiter,
                                                                 std::string_view outputDelimiter,
                                                                 const Pattern* matchPattern,
                                                                 juce::Range<int> lineRange,
                                                                 int afterLine,
                                                                 bool isFirstOnly,
                                                                 bool isStrict)
{
    static constexpr size_t maxInitialReserve { 65536 };

    if (bytes.empty() or lineRange.getStart() >= lineRange.getEnd())
        return { juce::Result::ok(), false, {} };

    if (jam::simd::findByte (bytes.data(), bytes.size(), 0, '\0') < bytes.size())
    {
        if (isExplicit)
            return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failBinaryFile, jam::Strings::fromView (displayPath).toRawUTF8())), false, {} };

        return { juce::Result::ok(), false, {} };
    }

    std::string output;
    output.reserve (std::min (bytes.size(), maxInitialReserve));
    jam::Array<std::string_view> tokens;
    bool anyLineMatched { false };
    const char* data { bytes.data() };
    const size_t size { bytes.size() };
    size_t cursor { 0 };
    int lineNumber { 1 };

    while (cursor < size)
    {
        const auto newlineIndex { jam::simd::findByte (data, size, cursor, static_cast<char> (Chars::newline)) };

        if (lineNumber > afterLine and lineNumber >= lineRange.getStart() and lineNumber < lineRange.getEnd())
        {
            auto line { bytes.substr (cursor, newlineIndex - cursor) };

            if (newlineIndex < size and not line.empty() and line.back() == static_cast<char> (Chars::carriageReturn))
                line.remove_suffix (1);

            const auto [status, matched] { processLine (output, tokens, line, fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern, displayPath, lineNumber, isStrict) };

            if (status.failed())
                return { status, false, {} };

            if (matched)
            {
                anyLineMatched = true;

                if (isFirstOnly)
                    break;
            }
        }

        if (lineRange.getEnd() != Arguments::openRangeEnd and lineNumber + 1 >= lineRange.getEnd())
            break;

        cursor = newlineIndex < size ? newlineIndex + 1 : size;
        ++lineNumber;
    }

    return { juce::Result::ok(), anyLineMatched, std::move (output) };
}

static int writeSingleOutput (const juce::Result& formatStatus, bool anyLineMatched, bool hasMatchFilter, std::string_view output)
{
    if (formatStatus.failed())
    {
        Stdio::printError (formatStatus.getErrorMessage());
        return Stdio::exitError;
    }

    if (hasMatchFilter and not anyLineMatched)
        return Stdio::exitMismatch;

    Stdio::write (output);
    return Stdio::exitSuccess;
}

static int runStandardInput (const jam::Array<int>& fieldIndices, bool hasCustomDelimiter, std::string_view delimiter,
                             std::string_view outputDelimiter, const Pattern* matchPattern, juce::Range<int> lineRange,
                             int afterLine, bool isFirstOnly, bool isStrict)
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

    const auto [formatStatus, anyLineMatched, output] { processBytes (inputText,
                                                                      jam::Strings::getView (Id::stdinPath),
                                                                      true,
                                                                      fieldIndices,
                                                                      hasCustomDelimiter,
                                                                      delimiter,
                                                                      outputDelimiter,
                                                                      matchPattern,
                                                                      lineRange,
                                                                      afterLine,
                                                                      isFirstOnly,
                                                                      isStrict) };
    return writeSingleOutput (formatStatus, anyLineMatched, matchPattern != nullptr, output);
}

static std::tuple<juce::Result, bool, std::string> getFormattedFileResult (const juce::File& resolved,
                                                                           std::string_view displayPath,
                                                                           bool isExplicit,
                                                                           const jam::Array<int>& fieldIndices,
                                                                           bool hasCustomDelimiter,
                                                                           std::string_view delimiter,
                                                                           std::string_view outputDelimiter,
                                                                           const Pattern* matchPattern,
                                                                           juce::Range<int> lineRange,
                                                                           int afterLine,
                                                                           bool isFirstOnly,
                                                                           bool isStrict)
{
    if (not resolved.existsAsFile())
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())), false, {} };

    if (resolved.getSize() == 0)
        return { juce::Result::ok(), false, {} };

    const juce::MemoryMappedFile mapped { resolved, juce::MemoryMappedFile::readOnly };

    if (mapped.getData() == nullptr)
        return { juce::Result::fail (juce::String::formatted (text::Diagnostics::failNotFound, jam::Strings::fromView (displayPath).toRawUTF8())), false, {} };

    const std::string_view bytes { static_cast<const char*> (mapped.getData()), mapped.getSize() };
    return processBytes (bytes, displayPath, isExplicit, fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern, lineRange, afterLine, isFirstOnly, isStrict);
}

static int writeFileOutputs (const jam::Array<juce::Result>& statuses, const jam::Array<bool>& matchedFlags,
                             const jam::Array<std::string>& outputs, bool hasMatchFilter)
{
    bool anyMatched { false };

    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
        {
            Stdio::printError (statuses.at (index).getErrorMessage());
            return Stdio::exitError;
        }

        anyMatched = anyMatched or matchedFlags.at (index);
    }

    if (hasMatchFilter and not anyMatched)
        return Stdio::exitMismatch;

    for (int index { 0 }; index < outputs.size(); ++index)
        Stdio::write (outputs.at (index));

    return Stdio::exitSuccess;
}

static int runFiles (const jam::Strings& rawPaths, const jam::Array<int>& fieldIndices, bool hasCustomDelimiter,
                     std::string_view delimiter, std::string_view outputDelimiter, const Pattern* matchPattern,
                     juce::Range<int> lineRange, int afterLine, bool isFirstOnly, bool isStrict)
{
    const auto [pathsStatus, files, isExplicitFlags] { Paths::getFiles (rawPaths, {}, {}, false, false) };

    if (pathsStatus.failed())
    {
        Stdio::printError (pathsStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto fileCount { static_cast<int> (files.size()) };
    const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount);
    jam::Array<bool> matchedFlags;
    matchedFlags.resize (fileCount);
    jam::Array<std::string> outputs;
    outputs.resize (fileCount);

    const auto& filePaths { files };
    const auto& explicitFlags { isExplicitFlags };

    Jobs::run (static_cast<int> (filePaths.size()), [&filePaths, &explicitFlags, &workingDirectory, &statuses, &matchedFlags, &outputs, &fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern, lineRange, afterLine, isFirstOnly, isStrict] (int index)
    {
        const auto displayPath { std::string_view { filePaths.at (static_cast<size_t> (index)) } };
        const auto resolved { workingDirectory.getChildFile (jam::Strings::fromView (displayPath)).getLinkedTarget() };
        auto [status, matched, output] { getFormattedFileResult (resolved, displayPath, explicitFlags.at (index), fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern, lineRange, afterLine, isFirstOnly, isStrict) };
        statuses.at (index) = std::move (status);
        matchedFlags.at (index) = matched;
        outputs.at (index) = std::move (output);
    });

    return writeFileOutputs (statuses, matchedFlags, outputs, matchPattern != nullptr);
}

static int runWithInputs (const Arguments& arguments, const jam::Array<int>& fieldIndices, bool hasCustomDelimiter,
                          std::string_view delimiter, std::string_view outputDelimiter, const Pattern* matchPattern,
                          juce::Range<int> lineRange, int afterLine)
{
    const auto& positionals { arguments.getPositionals() };
    jam::Strings rawPaths;
    rawPaths.addArray (positionals, 1, positionals.size() - 1);
    const bool isFirstOnly { arguments.contains (Id::firstOption) };
    const bool isStrict { arguments.contains (Id::strictOption) };

    if (rawPaths.size() == 0)
        return runStandardInput (fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern, lineRange, afterLine, isFirstOnly, isStrict);

    return runFiles (rawPaths, fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern, lineRange, afterLine, isFirstOnly, isStrict);
}

int Fields::run (const jam::Strings& argumentList)
{
    const jam::Strings flagOptions { Id::regexOption, Id::firstOption, Id::strictOption };
    const jam::Strings valueOptions { Id::delimiterOption, Id::outputDelimiterOption, Id::matchOption, Id::linesOption, Id::afterOption };
    const Arguments arguments { argumentList, firstOptionIndex, flagOptions, valueOptions };

    if (arguments.getStatus().failed())
    {
        Stdio::printError (arguments.getStatus().getErrorMessage());
        return Stdio::exitError;
    }

    const auto [fieldStatus, fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter] { getFieldSettings (arguments) };

    if (fieldStatus.failed())
    {
        Stdio::printError (fieldStatus.getErrorMessage());
        return Stdio::exitError;
    }

    const auto [filterStatus, afterLine, lineRange, matchPattern] { getFilterSettings (arguments) };

    if (filterStatus.failed())
    {
        Stdio::printError (filterStatus.getErrorMessage());
        return Stdio::exitError;
    }

    return runWithInputs (arguments, fieldIndices, hasCustomDelimiter, delimiter, outputDelimiter, matchPattern.get(), lineRange, afterLine);
}
