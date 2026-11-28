#include "Fields.h"
#include "Arguments.h"
#include "Input.h"
#include "Jobs.h"
#include "Paths.h"
#include "Pattern.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static std::pair<juce::Result, int> getParsedFieldIndex (std::string_view tokenView, std::string_view specText)
{
    if (tokenView.empty())
        return { Stdio::fail (text::Diagnostics::failFieldSpecInvalid, specText), 0 };

    const auto value { Arguments::getInteger (tokenView) };

    if (not value.has_value())
        return { Stdio::fail (text::Diagnostics::failFieldSpecInvalid, specText), 0 };

    if (*value == 0)
        return { Stdio::fail (text::Diagnostics::failFieldSpecInvalid, specText, text::Diagnostics::fieldSpecZero), 0 };

    return { juce::Result::ok(), *value };
}

static std::pair<juce::Result, jam::Union<int, int>> getParsedFieldRange (std::string_view tokenView, std::string_view specText)
{
    constexpr auto dash { static_cast<char> (Chars::dash) };
    const auto dashIndex { tokenView.find (dash, 1) };

    if (dashIndex == std::string_view::npos)
    {
        const auto [status, indexValue] { getParsedFieldIndex (tokenView, specText) };
        return { status, jam::Union<int, int>::pack (indexValue, indexValue) };
    }

    const auto [firstStatus, firstIndex] { getParsedFieldIndex (tokenView.substr (0, dashIndex), specText) };

    if (firstStatus.failed() or firstIndex < 0)
        return { firstStatus.failed() ? firstStatus : Stdio::fail (text::Diagnostics::failFieldSpecInvalid, specText), jam::Union<int, int> {} };

    const auto tailView { tokenView.substr (dashIndex + 1) };

    if (tailView.empty())
        return { juce::Result::ok(), jam::Union<int, int>::pack (firstIndex, Arguments::openRangeEnd) };

    const auto [lastStatus, lastIndex] { getParsedFieldIndex (tailView, specText) };

    if (lastStatus.failed() or lastIndex < 0)
        return { lastStatus.failed() ? lastStatus : Stdio::fail (text::Diagnostics::failFieldSpecInvalid, specText), jam::Union<int, int> {} };

    return { juce::Result::ok(), jam::Union<int, int>::pack (firstIndex, lastIndex) };
}

static std::pair<juce::Result, jam::Array<jam::Union<int, int>>> getFieldRanges (std::string_view specText)
{
    constexpr auto comma { static_cast<char> (Chars::comma) };

    if (specText.empty() or specText.at (0) == comma or specText.at (specText.size() - 1) == comma)
        return { Stdio::fail (text::Diagnostics::failFieldSpecInvalid, specText), jam::Array<jam::Union<int, int>> {} };

    jam::Array<jam::Union<int, int>> ranges;
    size_t cursor { 0 };

    while (cursor < specText.size())
    {
        const auto commaIndex { specText.find (comma, cursor) };
        const auto tokenEnd { commaIndex == std::string_view::npos ? specText.size() : commaIndex };
        const auto [status, rangeValue] { getParsedFieldRange (specText.substr (cursor, tokenEnd - cursor), specText) };

        if (status.failed())
            return { status, jam::Array<jam::Union<int, int>> {} };

        ranges.add (rangeValue);
        cursor = tokenEnd + 1;
    }

    return { juce::Result::ok(), std::move (ranges) };
}

static std::string_view getOutputDelimiter (const Arguments& arguments)
{
    static constexpr std::string_view singleSpace { " " };

    if (arguments.contains (Id::outputDelimiterOption))
        return arguments.getValueView (Id::outputDelimiterOption);

    if (arguments.contains (Id::delimiterOption))
        return arguments.getValueView (Id::delimiterOption);

    return singleSpace;
}

static std::pair<juce::Result, std::unique_ptr<Pattern>> getMatchPattern (const Arguments& arguments)
{
    if (arguments.contains (Id::matchOption))
    {
        const std::string_view patternText { arguments.getValueView (Id::matchOption) };
        return Pattern::create (juce::Span<const std::string_view> { &patternText, 1 }, arguments.contains (Id::regexOption), false, false);
    }

    return { juce::Result::ok(), nullptr };
}

static std::tuple<juce::Result, int, juce::Range<int>, std::unique_ptr<Pattern>> getFilterSettings (const Arguments& arguments)
{
    const auto [afterStatus, afterLine] { arguments.getInteger (Id::afterOption, 0, 0) };

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

static void setWhitespaceTokens (jam::Array<std::string_view>& tokens, std::string_view line)
{
    size_t cursor { 0 };

    while (cursor < line.size())
    {
        while (cursor < line.size() and jam::Document::isWhitespace (static_cast<unsigned char> (line.at (cursor))))
            ++cursor;

        if (cursor < line.size())
        {
            const size_t start { cursor };

            while (cursor < line.size() and not jam::Document::isWhitespace (static_cast<unsigned char> (line.at (cursor))))
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

static Stdio::Output getDocumentsOutput (const juce::Result& formatStatus, bool anyLineMatched, bool hasMatchFilter, Stdio::Documents documents)
{
    if (formatStatus.failed())
        return Stdio::getFailure (formatStatus);

    if (hasMatchFilter and not anyLineMatched)
        return Stdio::getOutput (Stdio::exitMismatch);

    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

void Fields::setLineTokens (jam::Array<std::string_view>& tokens, std::string_view line) const
{
    tokens.clear();

    if (not line.empty())
    {
        if (delimiter.has_value())
            setDelimitedTokens (tokens, line, *delimiter);
        else
            setWhitespaceTokens (tokens, line);
    }
}

std::optional<std::string_view> Fields::getSelectedField (int requested,
                                                          const jam::Array<std::string_view>& tokens) const
{
    const int slotIndex { requested > 0 ? requested - 1 : tokens.size() + requested };

    if (slotIndex >= 0 and slotIndex < tokens.size())
        return tokens.at (slotIndex);

    if (isStrict)
        return std::nullopt;

    return std::string_view {};
}

juce::Result Fields::appendFormattedLine (std::string& output,
                                          const jam::Array<std::string_view>& tokens,
                                          std::string_view displayPath,
                                          int lineNumber,
                                          std::string_view ending) const
{
    if (tokens.isEmpty() and not isStrict)
    {
        jam::Format::appendBytes (output, ending);
        return juce::Result::ok();
    }

    int numWrittenFields { 0 };

    for (const auto& fieldRange : fieldRanges)
    {
        const auto [firstIndex, lastIndex] { fieldRange };
        const int step { lastIndex < firstIndex ? -1 : 1 };
        const int endIndex { lastIndex == Arguments::openRangeEnd ? tokens.size() : lastIndex };
        const int numRangeFields { (endIndex - firstIndex) * step + 1 };

        for (int offset { 0 }; offset < numRangeFields; ++offset)
        {
            const int fieldIndex { firstIndex + offset * step };

            if (numWrittenFields > 0)
                jam::Format::appendBytes (output, outputDelimiter);

            const auto field { getSelectedField (fieldIndex, tokens) };

            if (not field.has_value())
                return Stdio::fail (text::Diagnostics::failFieldBounds,
                                    fieldIndex,
                                    text::Diagnostics::outOfRangeAt,
                                    displayPath,
                                    Chars::colon,
                                    lineNumber);

            jam::Format::appendBytes (output, *field);
            ++numWrittenFields;
        }
    }

    jam::Format::appendBytes (output, ending);
    return juce::Result::ok();
}

std::pair<juce::Result, bool> Fields::appendFields (std::string& output,
                                                    jam::Array<std::string_view>& tokens,
                                                    std::string_view bytes,
                                                    std::string_view line,
                                                    int lineNumber,
                                                    std::string_view displayPath,
                                                    jam::RegExpAutomaton& automaton) const
{
    const auto [matchStatus, isMatched] { matchPattern == nullptr
                                              ? std::pair<juce::Result, bool> { juce::Result::ok(), true }
                                              : matchPattern->hasMatch (line, automaton) };

    if (matchStatus.failed())
        return { matchStatus, false };

    if (isMatched)
    {
        setLineTokens (tokens, line);
        const std::string_view lineEnding { TextDocument::getLineEnding (bytes, line) };
        const auto status { appendFormattedLine (output, tokens, displayPath, lineNumber, lineEnding.empty() ? Stdio::newline : lineEnding) };
        return { status, status.wasOk() };
    }

    return { juce::Result::ok(), false };
}

std::tuple<juce::Result, bool, Stdio::Documents> Fields::getBytesOutput (std::string_view bytes,
                                                                         std::string_view displayPath,
                                                                         bool isExplicit) const
{
    static constexpr size_t maxInitialReserve { 65536 };

    std::string output;
    output.reserve (std::min (bytes.size(), maxInitialReserve));
    jam::Array<std::string_view> tokens;
    jam::RegExpAutomaton automaton;
    bool anyLineMatched { false };
    juce::Result status { juce::Result::ok() };

    const size_t readSize { TextDocument::applyToLines (bytes,
                                                        lineRange,
                                                        [this, displayPath, bytes, &output, &tokens, &automaton, &anyLineMatched, &status] (int lineNumber, std::string_view line)
                                                        {
                                                            if (lineNumber > afterLine)
                                                            {
                                                                const auto [lineStatus, isMatched] { appendFields (output, tokens, bytes, line, lineNumber, displayPath, automaton) };
                                                                status = lineStatus;
                                                                anyLineMatched = anyLineMatched or isMatched;
                                                                return lineStatus.wasOk() and not (isMatched and isFirstOnly);
                                                            }

                                                            return true;
                                                        }) };

    if (not isText and TextDocument::isBinary (bytes.substr (0, readSize)))
        return { TextDocument::getBinaryStatus (displayPath, isExplicit), false, Stdio::Documents {} };

    return { std::move (status), anyLineMatched, Stdio::getDocuments (std::move (output)) };
}

std::tuple<juce::Result, bool, Stdio::Documents> Fields::getFileOutput (const juce::File& file,
                                                                        std::string_view displayPath,
                                                                        bool isExplicit,
                                                                        const Input::Source& source) const
{
    const auto [inputStatus, input] { TextDocument::getInput (file, displayPath, Input::maxBufferedReadSize, source) };

    if (inputStatus.failed())
        return { inputStatus, false, Stdio::Documents {} };

    if (input->getBytes().empty())
        return { juce::Result::ok(), false, Stdio::Documents {} };

    return getBytesOutput (input->getBytes(), displayPath, isExplicit);
}

Stdio::Output Fields::runStandardInput (const Input::Source& input) const
{
    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    const auto [inputStatus, standardInput] { TextDocument::getInput (input, lineRange.getEnd() - 1) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    const auto inputText { standardInput->getBytes() };

    if (inputText.empty())
        return getDocumentsOutput (juce::Result::ok(), false, matchPattern != nullptr, Stdio::Documents {});

    auto [formatStatus, anyLineMatched, documents] { getBytesOutput (inputText, Id::stdinPath, true) };
    return getDocumentsOutput (formatStatus, anyLineMatched, matchPattern != nullptr, std::move (documents));
}

Stdio::Output Fields::runFiles (juce::Span<const std::string_view> rawPaths, const Input::Source& source) const
{
    const auto [readStatus, files] { Paths::getReadableFiles (rawPaths, {}, {}, false, false) };

    if (readStatus.failed())
        return Stdio::getFailure (readStatus);

    const auto fileCount { files.size() };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), fileCount);
    jam::Array<bool> matchedFlags;
    matchedFlags.insertMultiple (0, false, fileCount);
    jam::Array<Stdio::Documents> results;
    results.resize (fileCount);

    Jobs::run (fileCount,
               [this, &pathEntries = files, &source, &statuses, &matchedFlags, &results] (int index)
               {
                   const auto& entry { pathEntries.at (index) };
                   auto [fileStatus, isMatched, fileDocuments] { getFileOutput (Paths::getFile (entry), Paths::getDisplayPath (entry), Paths::isExplicit (entry), source) };
                   statuses.at (index) = std::move (fileStatus);
                   matchedFlags.at (index) = isMatched;
                   results.at (index) = std::move (fileDocuments);
               });

    Stdio::Documents documents;

    for (auto& fileDocuments : results)
        Stdio::addDocuments (documents, std::move (fileDocuments));

    return getDocumentsOutput (Stdio::getFirstFailure (statuses),
                               std::find (matchedFlags.begin(), matchedFlags.end(), true) != matchedFlags.end(),
                               matchPattern != nullptr,
                               std::move (documents));
}

Fields::Fields (const Arguments& arguments,
                jam::Array<jam::Union<int, int>> newFieldRanges,
                juce::Range<int> newLineRange,
                int newAfterLine,
                std::unique_ptr<Pattern> newMatchPattern)
    : fieldRanges (std::move (newFieldRanges)),
      delimiter (arguments.contains (Id::delimiterOption) ? std::optional<std::string_view> { arguments.getValueView (Id::delimiterOption) } : std::nullopt),
      outputDelimiter (getOutputDelimiter (arguments)),
      lineRange (newLineRange),
      afterLine (newAfterLine),
      matchPattern (std::move (newMatchPattern)),
      isFirstOnly (arguments.contains (Id::firstOption)),
      isStrict (arguments.contains (Id::strictOption)),
      isText (arguments.contains (Id::textOption))
{
}

Arguments Fields::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList,
                       { Id::regexOption, Id::firstOption, Id::strictOption, Id::textOption },
                       { Id::delimiterOption, Id::outputDelimiterOption, Id::matchOption, Id::linesOption, Id::afterOption },
                       {} };
}

Stdio::Output Fields::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    const auto& positionals { arguments.getPositionals() };

    if (positionals.isEmpty())
        return Stdio::getFailure (text::Diagnostics::failFieldsRequiresSpec);

    auto [rangeStatus, fieldRanges] { getFieldRanges (positionals.at (0)) };

    if (rangeStatus.failed())
        return Stdio::getFailure (rangeStatus);

    if (arguments.contains (Id::delimiterOption) and arguments.getValueView (Id::delimiterOption).empty())
        return Stdio::getFailure (text::Diagnostics::failDelimiterEmpty);

    auto [filterStatus, afterLine, lineRange, matchPattern] { getFilterSettings (arguments) };

    if (filterStatus.failed())
        return Stdio::getFailure (filterStatus);

    const Fields fields { arguments, std::move (fieldRanges), lineRange, afterLine, std::move (matchPattern) };

    if (arguments.getPositionals (1).empty())
        return fields.runStandardInput (input);

    return fields.runFiles (arguments.getPositionals (1), input);
}
