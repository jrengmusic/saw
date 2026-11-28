#include "Json.h"
#include "JsonOptions.h"
#include "Paths.h"
#include "TextDocument.h"
#include "generated/Generated.h"

#if JUCE_WINDOWS
#include <stdlib.h>
#else
extern char** environ;
#endif

static constexpr char recordSeparator { '\x1e' };
static constexpr char nullByte { static_cast<char> (Chars::nullCharacter) };
static constexpr bool isKeepingNumbers { true };
static constexpr int firstLine { 1 };
static constexpr int firstColumn { 1 };
static constexpr int streamLineCount { 1 };
static constexpr size_t numPairValues { 2 };

static std::string getLine (std::string_view line)
{
    std::string text;
    jam::Format::appendBytes (text, line);
    jam::Format::appendUtf8 (text, Chars::newline);
    return text;
}

static Stdio::Output getInformation (const JsonOptions& options)
{
    if (options.isVersion)
        return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (getLine (text::Diagnostics::jsonVersion)));

    return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (getLine (text::Diagnostics::jsonBuildConfiguration)));
}

static std::pair<juce::Result, std::unique_ptr<Input>> getFileInput (std::string_view path)
{
    const auto file { Paths::getLinkedTarget (path) };
    auto fileInput { Input::fromFile (file, Input::allBytes) };

    if (fileInput != nullptr)
        return { juce::Result::ok(), std::move (fileInput) };

    return { TextDocument::getReadFailure (file, path), nullptr };
}

static std::pair<juce::Result, std::unique_ptr<Input>> getSourceInput (std::string_view name, const Input::Source& source)
{
    if (name.empty() or name.compare (Id::stdinArgument) == 0)
        return Input::create (source, Input::allLines);

    return getFileInput (name);
}

static std::pair<juce::Result, std::string_view> getFilterText (const JsonOptions& options, jam::Array<std::unique_ptr<Input>>& inputs)
{
    if (options.filterArgument.empty())
        return { Stdio::fail (text::Diagnostics::failJsonFilterRequired), {} };

    if (not options.isFromFile)
        return { juce::Result::ok(), options.filterArgument };

    auto [inputStatus, input] { getFileInput (options.filterArgument) };

    if (inputStatus.failed())
        return { inputStatus, {} };

    const auto text { input->getBytes() };
    inputs.add (std::move (input));
    return { juce::Result::ok(), text };
}

static Stdio::Output getCompileFailure (const juce::Result& result)
{
    jam::Array<juce::Result> failures;
    failures.add (result);
    return Stdio::getOutput (JsonOptions::exitCompile, {}, std::move (failures));
}

static std::optional<juce::File> getModuleCandidate (const juce::File& directory, const juce::String& relative, const juce::String& name, const juce::String& suffix)
{
    const auto candidate { directory.getChildFile (relative + suffix) };
    const auto folderCandidate { directory.getChildFile (relative).getChildFile (name + suffix) };
    const auto packageCandidate { directory.getChildFile (relative).getChildFile (jam::Strings::fromView (Id::jsonPackageFolder)).getChildFile (jam::Strings::fromView (Id::jsonPackageMain) + suffix) };

    if (candidate.existsAsFile())
        return candidate;

    if (folderCandidate.existsAsFile())
        return folderCandidate;

    if (packageCandidate.existsAsFile())
        return packageCandidate;

    return std::nullopt;
}

static std::optional<juce::File> getModuleFile (const JsonOptions& options, std::string_view relativePath, std::string_view extension, const juce::Array<juce::File>& searchDirectories)
{
    const auto relative { jam::Strings::fromView (relativePath) };
    const auto suffix { jam::Strings::fromView (extension) };
    const size_t separator { relativePath.rfind (static_cast<char> (Chars::slash)) };
    const auto name { jam::Strings::fromView (separator == std::string_view::npos ? relativePath : relativePath.substr (separator + 1)) };

    for (const auto& directory : searchDirectories)
        if (const auto file { getModuleCandidate (directory, relative, name, suffix) }; file.has_value()) return file;

    for (const auto directoryPath : options.libraryPaths)
        if (const auto file { getModuleCandidate (juce::File::getCurrentWorkingDirectory().getChildFile (jam::Strings::fromView (directoryPath)), relative, name, suffix) }; file.has_value()) return file;

    return std::nullopt;
}

static jam::JsonDocument::ModuleFile getModuleFileFunction (const JsonOptions& options)
{
    return [&options] (std::string_view relativePath, std::string_view extension, const juce::Array<juce::File>& searchDirectories)
    {
        return getModuleFile (options, relativePath, extension, searchDirectories);
    };
}

static jam::Array<std::string_view> getLines (std::string_view bytes)
{
    jam::Array<std::string_view> lines;
    size_t cursor { 0 };

    while (cursor < bytes.size())
    {
        const size_t newline { jam::simd::findByte (bytes.data(), bytes.size(), cursor, static_cast<char> (Chars::newline)) };
        lines.add (bytes.substr (cursor, newline - cursor));
        cursor = newline + 1;
    }

    return lines;
}

static juce::Result getParseFailure (const juce::Result& parseStatus)
{
    const auto& message { parseStatus.getErrorMessage() };
    return Stdio::fail (text::Diagnostics::failJsonParse, jam::Strings::getView (message));
}

static std::pair<juce::Result, int> getReadResult (const juce::Result& status)
{
    if (status.failed())
        return { getParseFailure (status), JsonOptions::exitRuntime };

    return { juce::Result::ok(), Stdio::exitSuccess };
}

static jam::Document::Element& getVariableObject (jam::JsonDocument& variables)
{
    auto& holder { *variables.getRoot()->firstChild };
    return *variables.getChildByToken (holder, holder.get<jam::Document::Tokens> (Id::tokens)->at (0));
}

static std::unique_ptr<jam::JsonDocument> getEmptyDocument()
{
    return jam::JsonDocument::readValues (std::string_view {}, true, firstLine, firstColumn, false);
}

static void addRawText (jam::JsonDocument& document, std::string_view bytes)
{
    if (jam::Utf8::isValid (bytes))
    {
        document.addValue (*document.getRoot(), bytes);
    }
    else
    {
        std::string text;
        jam::Format::appendValidUtf8 (text, bytes);
        document.addValue (*document.getRoot(), std::string_view { text });
    }
}

static std::unique_ptr<jam::JsonDocument> getRawDocument (std::string_view bytes, bool isSlurp)
{
    auto document { getEmptyDocument() };

    if (isSlurp)
    {
        addRawText (*document, bytes);
    }
    else
    {
        for (const auto line : getLines (bytes))
            addRawText (*document, line);
    }

    return document;
}

static std::unique_ptr<jam::JsonDocument> getSourceDocument (const JsonOptions& options, std::string_view bytes)
{
    if (options.isRawInput)
        return getRawDocument (bytes, options.isSlurp);

    return jam::JsonDocument::readValues (bytes, true, firstLine, firstColumn, options.isStream and not options.isSlurp);
}

static void addDocumentValues (jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::JsonDocument& document, std::string_view name)
{
    document.applyToValues ([&values, &names, &document, name] (jam::Document::Element& valueContainer, const jam::Document::Token& value)
    {
        values.add ({ &document, &valueContainer, &value });
        names.add (name);
    });
}

static std::optional<juce::Result> getRecordFailure (const jam::JsonDocument& document, std::string_view record, int line)
{
    const auto status { document.getValueResult() };

    if (status.failed())
        return Stdio::fail (text::Diagnostics::jsonRecordSkipped, jam::Strings::getView (status.getErrorMessage()));

    const auto& tokens { *document.getRoot()->get<jam::Document::Tokens> (Id::tokens) };

    if (tokens.size() > 1)
        return Stdio::fail (text::Diagnostics::jsonRecordSkipped);

    if (tokens.size() == 1
        and not jam::JsonDocument::isContainer (tokens.at (0))
        and tokens.at (0).type != map::JsonTokenType::string
        and not jam::Format::trim (record.substr (record.size() - 1)).empty())
        return Stdio::fail (text::Diagnostics::jsonRecordTruncated, line);

    return std::nullopt;
}

static void addRecordDocument (std::string_view record, int line, int column, std::string_view name, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<juce::Result>& failures)
{
    auto document { jam::JsonDocument::readValues (record, true, line, column, false) };

    if (const auto failure { getRecordFailure (*document, record, line) }; failure.has_value())
        failures.add (*failure);
    else if (document->getRoot()->get<jam::Document::Tokens> (Id::tokens)->size() == 1)
    {
        addDocumentValues (values, names, *document, name);
        documents.add (std::move (document));
    }
}

static void addRecordDocuments (std::string_view bytes, std::string_view name, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<juce::Result>& failures)
{
    const size_t firstSeparator { jam::simd::findByte (bytes.data(), bytes.size(), 0, recordSeparator) };
    const auto outside { bytes.substr (0, firstSeparator) };

    if (not jam::Format::trim (outside).empty())
        failures.add (Stdio::fail (text::Diagnostics::jsonRecordOutside, firstLine));

    size_t lineStart { 0 };
    size_t newline { jam::simd::findByte (bytes.data(), bytes.size(), 0, static_cast<char> (Chars::newline)) };
    int line { firstLine };

    for (size_t separator { firstSeparator }; separator < bytes.size();)
    {
        const size_t recordStart { separator + 1 };
        const size_t nextSeparator { jam::simd::findByte (bytes.data(), bytes.size(), recordStart, recordSeparator) };

        while (newline < recordStart)
        {
            lineStart = newline + 1;
            newline = jam::simd::findByte (bytes.data(), bytes.size(), lineStart, static_cast<char> (Chars::newline));
            ++line;
        }

        const int column { firstColumn + static_cast<int> (recordStart - lineStart) };
        addRecordDocument (bytes.substr (recordStart, nextSeparator - recordStart), line, column, name, documents, values, names, failures);
        separator = nextSeparator;
    }
}

static juce::Result addStreamDocument (const JsonOptions& options, std::string& tail, bool isFinal, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<jam::JsonDocument*>& pendingErrors)
{
    auto document { jam::JsonDocument::readValues (std::move (tail), isFinal, firstLine, firstColumn, options.isStream and not options.isSlurp) };
    tail.assign (document->getSource().substr (document->getConsumedLength()));
    const auto status { document->getValueResult() };
    addDocumentValues (values, names, *document, std::string_view {});

    if (options.isStreamErrors and status.failed())
        pendingErrors.add (document.get());

    documents.add (std::move (document));
    return status;
}

static std::pair<juce::Result, int> readStreamSource (const JsonOptions& options, const Input::Source& source, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<jam::JsonDocument*>& pendingErrors)
{
    std::string tail;
    auto [chunkStatus, chunk] { Input::create (source, streamLineCount) };

    while (chunkStatus.wasOk() and not chunk->getBytes().empty())
    {
        jam::Format::appendBytes (tail, chunk->getBytes());
        const auto status { addStreamDocument (options, tail, false, documents, values, names, pendingErrors) };

        if (status.failed())
            return options.isStreamErrors ? std::pair<juce::Result, int> { juce::Result::ok(), Stdio::exitSuccess } : getReadResult (status);

        auto [nextStatus, nextChunk] { Input::create (source, streamLineCount) };
        chunkStatus = nextStatus;
        chunk = std::move (nextChunk);
    }

    if (chunkStatus.failed())
        return { chunkStatus, JsonOptions::exitInputOpen };

    const auto status { addStreamDocument (options, tail, true, documents, values, names, pendingErrors) };

    if (options.isStreamErrors and status.failed())
        return { juce::Result::ok(), Stdio::exitSuccess };

    return getReadResult (status);
}

static std::pair<juce::Result, int> readSource (const JsonOptions& options, std::string_view name, const Input::Source& source, jam::Array<std::unique_ptr<Input>>& inputs, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<juce::Result>& failures, jam::Array<jam::JsonDocument*>& pendingErrors)
{
    if (options.isStream and not options.isRawInput and name.empty() and Input::isStream (source))
        return readStreamSource (options, source, documents, values, names, pendingErrors);

    auto [inputStatus, input] { getSourceInput (name, source) };

    if (inputStatus.failed())
        return { inputStatus, JsonOptions::exitInputOpen };

    const auto bytes { input->getBytes() };

    if (options.isSeq and not options.isRawInput)
    {
        addRecordDocuments (bytes, name, documents, values, names, failures);
        inputs.add (std::move (input));
        return { juce::Result::ok(), Stdio::exitSuccess };
    }

    auto document { getSourceDocument (options, bytes) };
    inputs.add (std::move (input));
    addDocumentValues (values, names, *document, name);
    const auto valueStatus { document->getValueResult() };

    if (options.isStreamErrors and valueStatus.failed())
        pendingErrors.add (document.get());

    const auto status { (options.isStreamErrors and valueStatus.failed()) ? std::pair<juce::Result, int> { juce::Result::ok(), Stdio::exitSuccess } : getReadResult (valueStatus) };
    documents.add (std::move (document));
    return status;
}

static std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*> getSlurpValue (const JsonOptions& options, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, const jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values)
{
    documents.add (getEmptyDocument());
    auto& slurp { *documents.last() };
    auto& root { *slurp.getRoot() };

    if (options.isRawInput)
    {
        std::string text;

        for (const auto& [valueDocument, valueContainer, value] : values)
            jam::Format::appendBytes (text, valueDocument->getString (*value));

        const auto [list, token] { slurp.addValue (root, std::string_view { text }) };
        return { &slurp, list, token };
    }

    const auto [list, token] { slurp.addValue (root, Id::array) };
    auto& array { *slurp.getChildByToken (*list, *token) };

    for (const auto& [valueDocument, valueContainer, value] : values)
        slurp.addValue (array, *valueContainer, *value);

    return { &slurp, list, token };
}

static juce::Result applyToPairs (juce::Span<const std::string_view> namedValues, const std::function<juce::Result (std::string_view, std::string_view)>& function)
{
    jam::Array<juce::Result> statuses;

    for (size_t index { 0 }; index + numPairValues <= namedValues.size(); index += numPairValues)
        statuses.add (function (namedValues[index], namedValues[index + 1]));

    return Stdio::getFirstFailure (statuses);
}

static std::tuple<juce::Result, jam::Document::Element*, const jam::Document::Token*> parseJsonText (jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, std::string_view text, std::string_view failureText)
{
    auto valuesDocument { jam::JsonDocument::readValues (text, true, firstLine, firstColumn, false) };

    if (valuesDocument->getValueResult().failed() or valuesDocument->getRoot()->get<jam::Document::Tokens> (Id::tokens)->size() != 1)
        return { Stdio::fail (failureText), nullptr, nullptr };

    auto* root { valuesDocument->getRoot() };
    const auto* value { &root->get<jam::Document::Tokens> (Id::tokens)->at (0) };
    documents.add (std::move (valuesDocument));
    return { juce::Result::ok(), root, value };
}

static juce::Result addTextVariable (jam::JsonDocument& variables, std::string_view name, std::string_view text)
{
    auto& variablesObject { getVariableObject (variables) };
    variables.addKey (variablesObject, name);
    variables.addValue (variablesObject, text);
    return juce::Result::ok();
}

static juce::Result addJsonVariable (jam::JsonDocument& variables, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, std::string_view name, std::string_view text)
{
    auto [parseStatus, valueContainer, value] { parseJsonText (documents, text, text::Diagnostics::failJsonArgumentJson) };

    if (parseStatus.wasOk())
    {
        auto& variablesObject { getVariableObject (variables) };
        variables.addKey (variablesObject, name);
        variables.addValue (variablesObject, *valueContainer, *value);
    }

    return parseStatus;
}

static juce::Result addSlurpVariable (jam::JsonDocument& variables, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::unique_ptr<Input>>& inputs, std::string_view name, std::string_view path)
{
    auto [inputStatus, input] { getFileInput (path) };

    if (inputStatus.failed())
        return inputStatus;

    const auto bytes { input->getBytes() };
    inputs.add (std::move (input));
    auto valuesDocument { jam::JsonDocument::readValues (bytes, true, firstLine, firstColumn, false) };

    if (valuesDocument->getValueResult().failed())
        return Stdio::fail (text::Diagnostics::failJsonSlurpBad, name, text::Diagnostics::slurpNameSeparator, path, text::Diagnostics::slurpPathSeparator, jam::Strings::getView (valuesDocument->getValueResult().getErrorMessage()));

    auto& variablesObject { getVariableObject (variables) };
    variables.addKey (variablesObject, name);
    const auto [list, token] { variables.addValue (variablesObject, Id::array) };
    auto& array { *variables.getChildByToken (*list, *token) };
    auto& valuesRoot { *valuesDocument->getRoot() };

    for (const auto& item : *valuesRoot.get<jam::Document::Tokens> (Id::tokens))
        variables.addValue (array, valuesRoot, item);

    documents.add (std::move (valuesDocument));
    return juce::Result::ok();
}

static juce::Result addRawVariable (jam::JsonDocument& variables, jam::Array<std::unique_ptr<Input>>& inputs, std::string_view name, std::string_view path)
{
    auto [inputStatus, input] { getFileInput (path) };

    if (inputStatus.failed())
        return inputStatus;

    auto& variablesObject { getVariableObject (variables) };
    variables.addKey (variablesObject, name);
    variables.addValue (variablesObject, input->getBytes());
    inputs.add (std::move (input));
    return juce::Result::ok();
}

static juce::Result addPositionalArguments (const JsonOptions& options, jam::JsonDocument& variables, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Document::Element& positionals)
{
    jam::Array<juce::Result> statuses;

    for (const auto text : options.stringArguments)
        variables.addValue (positionals, text);

    for (const auto text : options.jsonArguments)
    {
        auto [parseStatus, valueContainer, value] { parseJsonText (documents, text, text::Diagnostics::failJsonArgumentsJson) };
        statuses.add (parseStatus);

        if (parseStatus.wasOk())
            variables.addValue (positionals, *valueContainer, *value);
    }

    return Stdio::getFirstFailure (statuses);
}

static juce::Result addProgramArguments (const JsonOptions& options, jam::JsonDocument& variables, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents)
{
    auto& variablesObject { getVariableObject (variables) };
    const int argumentsKey { variablesObject.get<jam::Document::Tokens> (Id::tokens)->size() };
    variables.addKey (variablesObject, Id::jsonArgumentsVariable);
    auto [list, token] { variables.addValue (variablesObject, Id::object) };
    auto& arguments { *variables.getChildByToken (*list, *token) };
    variables.addKey (arguments, Id::jsonPositionalKey);
    std::tie (list, token) = variables.addValue (arguments, Id::array);
    auto& positionals { *variables.getChildByToken (*list, *token) };
    variables.addKey (arguments, Id::jsonNamedKey);
    std::tie (list, token) = variables.addValue (arguments, Id::object);
    auto& named { *variables.getChildByToken (*list, *token) };

    for (int index { 0 }; index < argumentsKey; index += jam::JsonDocument::numMemberTokens)
    {
        variables.addKey (named, variables.getString (variablesObject.get<jam::Document::Tokens> (Id::tokens)->at (index)));
        const auto [itemContainer, item] { variables.getValue (variablesObject, variablesObject.get<jam::Document::Tokens> (Id::tokens)->at (index + 1)) };
        variables.addValue (named, *itemContainer, *item);
    }

    variables.setKeyOrder (named);
    variables.setKeyOrder (arguments);
    return addPositionalArguments (options, variables, documents, positionals);
}

static void addEnvironmentEntry (jam::JsonDocument& variables, jam::Document::Element& environment, std::string_view entry)
{
    const size_t separator { jam::simd::findByte (entry.data(), entry.size(), 0, static_cast<char> (Chars::equals)) };

    if (separator < entry.size())
    {
        variables.addKey (environment, entry.substr (0, separator));
        variables.addValue (environment, entry.substr (separator + 1));
    }
}

static void addEnvironmentVariables (jam::JsonDocument& variables)
{
#if JUCE_WINDOWS
    char** entries { _environ };
#else
    char** entries { environ };
#endif
    auto& variablesObject { getVariableObject (variables) };
    variables.addKey (variablesObject, Id::jsonEnvironmentVariable);
    const auto [list, token] { variables.addValue (variablesObject, Id::object) };
    auto& environment { *variables.getChildByToken (*list, *token) };

    for (char* const* entry { entries }; *entry != nullptr; ++entry)
        addEnvironmentEntry (variables, environment, std::string_view { *entry });

    variables.setKeyOrder (environment);
}

static void addOriginVariables (const JsonOptions& options, jam::JsonDocument& variables)
{
    auto& variablesObject { getVariableObject (variables) };
    variables.addKey (variablesObject, Id::jsonSearchListVariable);
    const auto [searchList, searchToken] { variables.addValue (variablesObject, Id::array) };
    auto& searchArray { *variables.getChildByToken (*searchList, *searchToken) };

    for (const auto path : options.libraryPaths)
        variables.addValue (searchArray, path);

    variables.addKey (variablesObject, Id::jsonProgOriginVariable);

    if (options.isFromFile)
    {
        const auto programDirectory { Paths::getLinkedTarget (options.filterArgument).getParentDirectory().getFullPathName() };
        variables.addValue (variablesObject, jam::Strings::getView (programDirectory));
    }
    else
    {
        variables.addValue (variablesObject, std::string_view {});
    }

    const auto executableDirectory { juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory().getFullPathName() };
    variables.addKey (variablesObject, Id::jsonJqOriginVariable);
    variables.addValue (variablesObject, jam::Strings::getView (executableDirectory));
}

static juce::Result addVariables (const JsonOptions& options, jam::JsonDocument& variables, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::unique_ptr<Input>>& inputs)
{
    jam::Array<juce::Result> statuses;
    statuses.add (applyToPairs (options.argNamedValues, [&variables] (std::string_view name, std::string_view text) { return addTextVariable (variables, name, text); }));
    statuses.add (applyToPairs (options.argJsonNamedValues, [&variables, &documents] (std::string_view name, std::string_view text) { return addJsonVariable (variables, documents, name, text); }));
    statuses.add (applyToPairs (options.slurpFileNamedValues, [&variables, &documents, &inputs] (std::string_view name, std::string_view path) { return addSlurpVariable (variables, documents, inputs, name, path); }));
    statuses.add (applyToPairs (options.rawFileNamedValues, [&variables, &inputs] (std::string_view name, std::string_view path) { return addRawVariable (variables, inputs, name, path); }));
    statuses.add (addProgramArguments (options, variables, documents));
    addEnvironmentVariables (variables);
    auto& variablesObject { getVariableObject (variables) };
    variables.addKey (variablesObject, Id::jsonBuildConfigurationVariable);
    variables.addValue (variablesObject, std::string_view { text::Diagnostics::jsonBuildConfiguration });
    addOriginVariables (options, variables);
    variables.setKeyOrder (variablesObject);
    return Stdio::getFirstFailure (statuses);
}

static jam::Strings getVariableNames (jam::JsonDocument& variables)
{
    jam::Strings names;

    variables.applyToMembers (getVariableObject (variables), [&names] (std::string_view name, jam::Document::Element&, const jam::Document::Token&)
    {
        names.add (jam::JsonDocument::getVariableName (name));
    });

    return names;
}

static bool isTruthy (const jam::JsonDocument& document, const jam::Document::Token& value)
{
    if (value.type == map::JsonTokenType::boolean)
        return document.getBoolean (value);

    return value.type != map::JsonTokenType::null;
}

static jam::Array<std::string_view> getColourPieces (std::string_view spec)
{
    jam::Array<std::string_view> pieces;
    size_t cursor { 0 };

    while (cursor <= spec.size())
    {
        const size_t separator { jam::simd::findByte (spec.data(), spec.size(), cursor, static_cast<char> (Chars::colon)) };
        pieces.add (spec.substr (cursor, separator - cursor));
        cursor = separator + 1;
    }

    return pieces;
}

static std::string_view getColourPiece (const jam::Array<std::string_view>& pieces, const jam::Array<std::string_view>& defaultPieces, int index)
{
    if (index < pieces.size() and not pieces.at (index).empty())
        return pieces.at (index);

    return defaultPieces.at (index);
}

static bool isColourEnabled (const JsonOptions& options)
{
    if (options.isColourRequested)
        return true;

    return not options.isMonochrome and Stdio::isTerminalOutput() and std::getenv (Id::noColourVariable.data()) == nullptr;
}

static std::array<std::string_view, jam::JsonWriter::numColours> getColourViews()
{
    std::array<std::string_view, jam::JsonWriter::numColours> colourViews;
    const char* spec { std::getenv (Id::jqColoursVariable.data()) };
    const auto pieces { getColourPieces (spec == nullptr ? std::string_view {} : std::string_view { spec }) };
    const auto defaultPieces { getColourPieces (Id::jsonDefaultColours) };

    for (int index { 0 }; index < jam::JsonWriter::numColours; ++index)
        colourViews.at (static_cast<size_t> (index)) = getColourPiece (pieces, defaultPieces, index);

    return colourViews;
}

static jam::JsonWriter getWriter (const JsonOptions& options, const std::array<std::string_view, jam::JsonWriter::numColours>& colourViews)
{
    const auto formatOptions { juce::JSON::FormatOptions {}
                                   .withSpacing (options.isCompact ? juce::JSON::Spacing::none : juce::JSON::Spacing::multiLine)
                                   .withEncoding (options.isAscii ? juce::JSON::Encoding::ascii : juce::JSON::Encoding::utf8) };
    const juce::Span<const std::string_view> colours { colourViews.data(), isColourEnabled (options) ? colourViews.size() : size_t { 0 } };
    return jam::JsonWriter { formatOptions, options.indent, options.isTab, options.isSortedKeys, isKeepingNumbers, Stdio::newline, colours };
}

static void appendResult (juce::OutputStream& output, const JsonOptions& options, const jam::JsonWriter& writer, jam::JsonDocument::Documents documents, const jam::Document::Element& valueContainer, const jam::Document::Token& value)
{
    if (options.isSeq)
        output.writeByte (recordSeparator);

    if (options.isRawOutput and value.type == map::JsonTokenType::string)
        jam::Format::appendBytes (output, jam::JsonDocument::getDocument (valueContainer, documents).getString (value));
    else
        writer.appendValue (output, documents, valueContainer, value);

    if (options.isRawOutput0)
        output.writeByte (nullByte);
    else if (not options.isJoinOutput)
        jam::Format::appendUtf8 (output, Chars::newline);
}

static juce::Result getRuntimeFailure (const juce::Result& failure, std::string_view location)
{
    return Stdio::fail (text::Diagnostics::failJsonErrorPrefix,
                        location,
                        text::Diagnostics::failJsonErrorSeparator,
                        jam::Strings::getView (failure.getErrorMessage()));
}

static std::string_view getLocation (std::optional<std::string_view> fileName)
{
    if (fileName.has_value())
        return *fileName;

    return text::Diagnostics::failJsonStandardInputName;
}

static std::string getValueLocation (jam::JsonDocument& document, const jam::Document::Token& value, std::optional<std::string_view> fileName)
{
    const auto offset { value.offset() };
    std::string location;
    jam::Format::appendBytes (location, getLocation (fileName));
    jam::Format::appendUtf8 (location, Chars::colon);
    jam::Format::appendDecimal (location, static_cast<int> (document.getLineNumber (offset)));
    return location;
}

static int getLastValueExitCode (const std::optional<bool>& isLastValueTruthy)
{
    if (not isLastValueTruthy.has_value())
        return JsonOptions::exitNoOutput;

    if (not *isLastValueTruthy)
        return JsonOptions::exitFalseOrNull;

    return Stdio::exitSuccess;
}

static int getExitCode (const JsonOptions& options, std::optional<int> haltCode, int inputCode, int runtimeFailureCount, const std::optional<bool>& isLastValueTruthy)
{
    if (haltCode.has_value())
        return *haltCode;

    if (inputCode != Stdio::exitSuccess)
        return inputCode;

    if (runtimeFailureCount > 0)
        return JsonOptions::exitRuntime;

    if (options.isExitStatus)
        return getLastValueExitCode (isLastValueTruthy);

    return Stdio::exitSuccess;
}

static std::pair<juce::Result, int> loadSourceAt (const JsonOptions& options, int index, const Input::Source& source, jam::Array<std::unique_ptr<Input>>& inputs, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<juce::Result>& failures, jam::Array<jam::JsonDocument*>& pendingErrors)
{
    const auto name { options.inputFiles.isEmpty() ? std::string_view {} : options.inputFiles.at (index) };
    return readSource (options, name, source, inputs, documents, values, names, failures, pendingErrors);
}

static jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>> getSlurpedValues (const JsonOptions& options, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, const jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values)
{
    jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>> slurped;
    slurped.add (getSlurpValue (options, documents, values));
    return slurped;
}

static int getSourceCount (const JsonOptions& options)
{
    if (options.inputFiles.isEmpty())
        return 1;

    return options.inputFiles.size();
}

static void addSourceDocuments (const jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::JsonDocument::Registry& registry, int firstIndex)
{
    for (int index { firstIndex }; index < documents.size(); ++index)
        registry.try_emplace (documents.at (index)->getRoot(), documents.at (index).get());
}

static jam::JsonDocument& addEmptyDocument (jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::JsonDocument::Registry& registry)
{
    documents.add (getEmptyDocument());
    addSourceDocuments (documents, registry, documents.size() - 1);
    return *documents.at (documents.size() - 1);
}

static void replaceRunDocument (std::unique_ptr<jam::JsonDocument>& run, jam::JsonDocument::Registry& registry)
{
    registry.erase (run->getRoot());

    run = getEmptyDocument();
    registry.try_emplace (run->getRoot(), run.get());
}

static void loadNextSource (const JsonOptions& options, const Input::Source& source, int& nextSource, jam::Array<std::unique_ptr<Input>>& inputs, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::JsonDocument::Registry& registry, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<juce::Result>& failures, jam::Array<jam::JsonDocument*>& pendingErrors, int& inputCode)
{
    const int firstDocument { documents.size() };
    const auto [status, code] { loadSourceAt (options, nextSource, source, inputs, documents, values, names, failures, pendingErrors) };
    ++nextSource;
    addSourceDocuments (documents, registry, firstDocument);

    if (status.failed())
    {
        failures.add (status);
        inputCode = code;
    }
}

static jam::JsonDocument::Input getInputFunction (const JsonOptions& options, const Input::Source& source, jam::Array<std::unique_ptr<Input>>& inputs, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::JsonDocument::Registry& registry, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<juce::Result>& failures, jam::Array<jam::JsonDocument*>& pendingErrors, int& inputCode, int& nextSource, int& nextValue)
{
    return [&options, &source, &inputs, &documents, &registry, &values, &names, &failures, &pendingErrors, &inputCode, &nextSource, &nextValue] () -> std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>
    {
        while (nextValue == values.size() and nextSource < getSourceCount (options) and inputCode == Stdio::exitSuccess)
            loadNextSource (options, source, nextSource, inputs, documents, registry, values, names, failures, pendingErrors, inputCode);

        if (nextValue < values.size())
        {
            ++nextValue;
            return values.at (nextValue - 1);
        }

        return { nullptr, nullptr, nullptr };
    };
}

static jam::JsonDocument::FileName getFileNameFunction (const jam::Array<std::string_view>& names, const int& nextValue)
{
    return [&names, &nextValue] () -> std::optional<std::string_view>
    {
        if (nextValue == 0 or names.isEmpty() or names.at (nextValue - 1).empty())
            return std::nullopt;

        return names.at (nextValue - 1);
    };
}

static jam::JsonDocument::Message getMessageFunction (std::string& errorText)
{
    return [&errorText] (std::string_view message)
    {
        jam::Format::appendBytes (errorText, message);
    };
}

static jam::JsonDocument::Emit getEmitFunction (const JsonOptions& options, const jam::JsonDocument::Registry& registry, const jam::JsonWriter& writer, juce::MemoryOutputStream& buffer, std::optional<bool>& isLastValueTruthy)
{
    return [&options, &registry, &writer, &buffer, &isLastValueTruthy] (jam::Document::Element& valueContainer, const jam::Document::Token& value) -> bool
    {
        const jam::JsonDocument::Documents view { registry };
        const auto [targetContainer, target] { jam::JsonDocument::getDocument (valueContainer, view).getValue (valueContainer, value) };
        isLastValueTruthy = isTruthy (jam::JsonDocument::getDocument (*targetContainer, view), *target);
        appendResult (buffer, options, writer, view, valueContainer, value);
        return true;
    };
}

static std::optional<int> applyProgram (jam::JsonDocument& target, jam::JsonDocument& program, jam::Document::Element& valueContainer, const jam::Document::Token& value, std::string_view location, const jam::JsonDocument::Emit& writeValue, const jam::JsonDocument::Input& getInput, const jam::JsonDocument::Message& addMessage, const jam::JsonDocument::FileName& getFileName, const jam::JsonDocument::ModuleFile& getModuleFile, jam::JsonDocument& variables, jam::JsonDocument::Registry& registry, jam::Array<juce::Result>& failures, int& runtimeFailureCount)
{
    const auto [result, code] { target.applyToResults (program, valueContainer, value, registry, variables, getInput, addMessage, getFileName, getModuleFile, writeValue) };

    if (result.failed())
    {
        ++runtimeFailureCount;
        failures.add (getRuntimeFailure (result, location));
    }

    return code;
}

static void addStreamErrorEvent (jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& events, jam::JsonDocument& eventDocument, const jam::JsonDocument& errorDocument)
{
    const auto [errorList, errorValue] { eventDocument.addValue (*eventDocument.getRoot(), Id::array) };
    auto& errorArray { *eventDocument.getChildByToken (*errorList, *errorValue) };
    const auto errorResult { errorDocument.getValueResult() };
    eventDocument.addValue (errorArray, jam::Strings::getView (errorResult.getErrorMessage()));
    const auto [pathList, pathValue] { eventDocument.addValue (errorArray, Id::array) };
    auto& pathArray { *eventDocument.getChildByToken (*pathList, *pathValue) };

    for (const auto& segment : errorDocument.getFailurePath())
    {
        if (std::holds_alternative<int> (segment))
            eventDocument.addValue (pathArray, static_cast<double> (std::get<int> (segment)));
        else
            eventDocument.addValue (pathArray, std::string_view { std::get<std::string> (segment) });
    }

    events.add ({ &eventDocument, errorList, errorValue });
}

static jam::JsonDocument::Input getStreamInputFunction (const jam::JsonDocument::Input& rawInput, jam::JsonDocument& streamFilter, jam::JsonDocument& variables, const jam::JsonDocument::FileName& getFileName, const jam::JsonDocument::ModuleFile& getModuleFile, const jam::JsonDocument::Message& addMessage, jam::Array<juce::Result>& failures, int& runtimeFailureCount, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::JsonDocument::Registry& registry, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& events, jam::Array<jam::JsonDocument*>& pendingErrors, int& nextEvent)
{
    return [&rawInput, &streamFilter, &variables, &getFileName, &getModuleFile, &addMessage, &failures, &runtimeFailureCount, &documents, &registry, &events, &pendingErrors, &nextEvent] () -> std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>
    {
        while (nextEvent == events.size())
        {
            events = jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>> {};
            nextEvent = 0;
            const auto [inputDocument, inputContainer, token] { rawInput() };

            if (token != nullptr)
            {
                auto& eventDocument { addEmptyDocument (documents, registry) };
                int drained { 0 };

                while (drained < pendingErrors.size() and pendingErrors.at (drained) != inputDocument)
                {
                    addStreamErrorEvent (events, eventDocument, *pendingErrors.at (drained));
                    ++drained;
                }

                if (drained > 0)
                    pendingErrors.removeRange (0, drained);

                const auto location { getValueLocation (*inputDocument, *token, getFileName()) };
                const auto collect { [&events, &eventDocument] (jam::Document::Element& valueContainer, const jam::Document::Token& value) -> bool
                {
                    events.add ({ &eventDocument, &valueContainer, &value });
                    return true;
                } };
                applyProgram (eventDocument, streamFilter, *inputContainer, *token, location, collect, [] () -> std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*> { return { nullptr, nullptr, nullptr }; }, addMessage, getFileName, getModuleFile, variables, registry, failures, runtimeFailureCount);
            }
            else if (not pendingErrors.isEmpty())
            {
                auto& eventDocument { addEmptyDocument (documents, registry) };

                for (auto* errorDocument : pendingErrors)
                    addStreamErrorEvent (events, eventDocument, *errorDocument);

                pendingErrors.clear();
            }
            else
            {
                break;
            }
        }

        if (nextEvent < events.size())
        {
            ++nextEvent;
            return events.at (nextEvent - 1);
        }

        return { nullptr, nullptr, nullptr };
    };
}

static std::optional<int> runInputValue (jam::JsonDocument& filter, std::unique_ptr<jam::JsonDocument>& run, jam::JsonDocument::Registry& registry, std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*> input, const jam::JsonDocument::Input& getInput, const jam::JsonDocument::Emit& emit, const jam::JsonDocument::Message& addMessage, const jam::JsonDocument::FileName& getFileName, const jam::JsonDocument::ModuleFile& getModuleFile, jam::JsonDocument& variables, jam::Array<juce::Result>& failures, int& runtimeFailureCount)
{
    const auto [inputDocument, inputContainer, token] { input };
    const auto location { getValueLocation (*inputDocument, *token, getFileName()) };
    replaceRunDocument (run, registry);
    return applyProgram (*run, filter, *inputContainer, *token, location, emit, getInput, addMessage, getFileName, getModuleFile, variables, registry, failures, runtimeFailureCount);
}

static void loadSlurpedInput (const JsonOptions& options, const Input::Source& source, jam::Array<std::unique_ptr<Input>>& inputs, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::JsonDocument::Registry& registry, jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>>& values, jam::Array<std::string_view>& names, jam::Array<juce::Result>& failures, jam::Array<jam::JsonDocument*>& pendingErrors, int& inputCode, int& nextSource)
{
    while (nextSource < getSourceCount (options) and inputCode == Stdio::exitSuccess)
        loadNextSource (options, source, nextSource, inputs, documents, registry, values, names, failures, pendingErrors, inputCode);

    const int firstSlurpDocument { documents.size() };
    values = getSlurpedValues (options, documents, values);
    addSourceDocuments (documents, registry, firstSlurpDocument);
    names = jam::Array<std::string_view> {};
}

static Stdio::Output runCompiled (const JsonOptions& options, const Input::Source& source, jam::JsonDocument& filter, jam::JsonDocument& streamFilter, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, jam::Array<std::unique_ptr<Input>>& inputs, const jam::JsonDocument::ModuleFile& getModuleFile)
{
    jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>> values;
    jam::Array<std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*>> events;
    jam::Array<jam::JsonDocument*> pendingErrors;
    jam::Array<std::string_view> names;
    jam::Array<juce::Result> failures;
    jam::JsonDocument::Registry registry;
    juce::MemoryBlock output;
    juce::MemoryOutputStream buffer { output, false };
    std::string errorText;
    std::unique_ptr<jam::JsonDocument> run { getEmptyDocument() };
    std::optional<bool> isLastValueTruthy;
    std::optional<int> haltCode;
    int inputCode { Stdio::exitSuccess };
    int runtimeFailureCount { 0 };
    int nextSource { 0 };
    int nextValue { 0 };
    int nextEvent { 0 };
    auto& variables { *documents.at (0) };

    addSourceDocuments (documents, registry, 0);
    registry.try_emplace (run->getRoot(), run.get());
    jam::JsonDocument::addRunDocuments (filter, variables, registry);
    jam::JsonDocument::addRunDocuments (streamFilter, variables, registry);

    const auto colourViews { getColourViews() };
    const auto writer { getWriter (options, colourViews) };
    const auto rawInput { getInputFunction (options, source, inputs, documents, registry, values, names, failures, pendingErrors, inputCode, nextSource, nextValue) };
    const auto getFileName { getFileNameFunction (names, nextValue) };
    const auto addMessage { getMessageFunction (errorText) };
    const auto getInput { options.isStream ? getStreamInputFunction (rawInput, streamFilter, variables, getFileName, getModuleFile, addMessage, failures, runtimeFailureCount, documents, registry, events, pendingErrors, nextEvent) : rawInput };
    const auto emit { getEmitFunction (options, registry, writer, buffer, isLastValueTruthy) };

    if (options.isSlurp)
        loadSlurpedInput (options, source, inputs, documents, registry, values, names, failures, pendingErrors, inputCode, nextSource);

    if (options.isNullInput)
    {
        auto& nullDocument { addEmptyDocument (documents, registry) };
        const auto [nullList, nullValue] { nullDocument.addValue (*nullDocument.getRoot(), nullptr) };
        haltCode = runInputValue (filter, run, registry, { &nullDocument, nullList, nullValue }, getInput, emit, addMessage, getFileName, getModuleFile, variables, failures, runtimeFailureCount);
    }
    else
    {
        auto [inputDocument, inputContainer, token] { getInput() };

        while (token != nullptr and not haltCode.has_value())
        {
            haltCode = runInputValue (filter, run, registry, { inputDocument, inputContainer, token }, getInput, emit, addMessage, getFileName, getModuleFile, variables, failures, runtimeFailureCount);

            if (not haltCode.has_value())
                std::tie (inputDocument, inputContainer, token) = getInput();
        }
    }

    const int exitCode { getExitCode (options, haltCode, inputCode, runtimeFailureCount, isLastValueTruthy) };
    buffer.flush();
    auto outputDocuments (Stdio::getDocuments (std::move (output)));
    buffer.reset();
    return Stdio::getOutput (exitCode, std::move (outputDocuments), std::move (failures), Stdio::getDocuments (std::move (errorText)));
}

static Stdio::Output runFilter (const JsonOptions& options, const Input::Source& input)
{
    jam::SharedDocuments sharedDocuments;
    jam::Array<std::unique_ptr<Input>> inputs;
    auto [filterStatus, filterText] { getFilterText (options, inputs) };

    if (filterStatus.failed())
        return Stdio::getFailure (filterStatus);

    jam::Array<std::unique_ptr<jam::JsonDocument>> documents;
    documents.add (getEmptyDocument());
    auto& variables { *documents.at (0) };
    variables.addValue (*variables.getRoot(), Id::object);
    const auto variablesStatus { addVariables (options, variables, documents, inputs) };

    if (variablesStatus.failed())
        return Stdio::getFailure (variablesStatus);

    const auto getModuleFile { getModuleFileFunction (options) };
    auto [compileResult, filter] (jam::JsonDocument::readFilter (filterText, getVariableNames (variables), getModuleFile));
    auto [streamResult, streamFilter] (jam::JsonDocument::readFilter (Id::jsonTostreamFilter, jam::Strings {}, getModuleFile));

    if (compileResult.failed())
        return getCompileFailure (compileResult);

    if (streamResult.failed())
        return getCompileFailure (streamResult);

    return runCompiled (options, input, *filter, *streamFilter, documents, inputs, getModuleFile);
}

static std::string_view getLineAt (const jam::Array<std::string_view>& lines, int index)
{
    if (index < lines.size())
        return lines.at (index);

    return {};
}

static bool isBlank (std::string_view line)
{
    return jam::Format::trim (line).empty();
}

static bool isComment (std::string_view line)
{
    return not line.empty() and line.front() == static_cast<char> (Chars::hash);
}

static bool isFailMarker (std::string_view line)
{
    const std::string_view marker { text::Diagnostics::jsonTestFailMarker };
    return line.substr (0, marker.size()).compare (marker) == 0;
}

static int getBlockEnd (const jam::Array<std::string_view>& lines, int from)
{
    int end { from };

    while (end < lines.size() and not isBlank (lines.at (end)))
        ++end;

    return end;
}

static void appendLine (std::string& report, const juce::Result& message)
{
    jam::Format::appendBytes (report, jam::Strings::getView (message.getErrorMessage()));
    jam::Format::appendUtf8 (report, Chars::newline);
}

static void appendTestHeader (std::string& report, int testIndex, std::string_view program, int lineNumber)
{
    jam::Format::appendBytes (report, text::Diagnostics::jsonTestHeader);
    jam::Format::appendDecimal (report, testIndex);
    jam::Format::appendBytes (report, text::Diagnostics::jsonTestProgramOpen);
    jam::Format::appendBytes (report, program);
    jam::Format::appendBytes (report, text::Diagnostics::jsonTestProgramClose);
    jam::Format::appendDecimal (report, lineNumber);
    jam::Format::appendUtf8 (report, Chars::newline);
}

static void appendBlockHeader (std::string& report, int testIndex, const jam::Array<std::string_view>& lines, int index)
{
    const int programIndex { isFailMarker (lines.at (index)) ? index + 1 : index };
    appendTestHeader (report, testIndex, getLineAt (lines, programIndex), programIndex + 1);
}

static bool isTestSelected (const JsonOptions& options, int testIndex)
{
    return testIndex > options.skipCount and testIndex - options.skipCount <= options.takeCount;
}

static std::pair<int, int> runFailCase (const JsonOptions& options, const jam::Array<std::string_view>& lines, int programIndex, std::string& report)
{
    const auto program { getLineAt (lines, programIndex) };
    auto [compileResult, filter] (jam::JsonDocument::readFilter (program, jam::Strings {}, getModuleFileFunction (options)));

    if (compileResult.wasOk())
    {
        appendLine (report, Stdio::fail (text::Diagnostics::jsonTestUnexpectedCompile, programIndex + 1, text::Diagnostics::jsonTestProgramSeparator, program));
        return { 1, 0 };
    }

    return { 0, 0 };
}

static std::pair<int, int> addExpectedDocuments (const jam::Array<std::string_view>& lines, int programIndex, int end, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, std::string& report)
{
    int messages { 0 };
    int malformed { 0 };

    for (int lineIndex { programIndex + 2 }; lineIndex < end; ++lineIndex)
    {
        if (not isComment (getLineAt (lines, lineIndex)))
        {
            auto expectedDocument { jam::JsonDocument::readValues (getLineAt (lines, lineIndex), true, firstLine, firstColumn, false) };

            if (expectedDocument->getValueResult().failed() or expectedDocument->getRoot()->get<jam::Document::Tokens> (Id::tokens)->size() != 1)
            {
                appendLine (report, Stdio::fail (text::Diagnostics::jsonTestExpectedInvalid, lineIndex + 1, text::Diagnostics::jsonTestProgramSeparator, getLineAt (lines, lineIndex)));
                ++messages;
                ++malformed;
            }

            documents.add (std::move (expectedDocument));
        }
    }

    return { messages, malformed };
}

static int compareExpected (const jam::JsonDocument::Documents& registryView, const jam::Document::Element& outputContainer, const jam::Document::Token& output, const jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, int expectedFirst, int outputIndex, std::string_view program, int programIndex, std::string& report)
{
    const auto& expectedRoot { *documents.at (expectedFirst + outputIndex)->getRoot() };
    const auto& expectedToken { expectedRoot.get<jam::Document::Tokens> (Id::tokens)->at (0) };

    if (jam::JsonDocument::compare (registryView, outputContainer, output, expectedRoot, expectedToken) == 0)
        return 0;

    appendLine (report, Stdio::fail (text::Diagnostics::jsonTestWrong, programIndex + 3 + outputIndex, text::Diagnostics::jsonTestProgramSeparator, program));
    return 1;
}

static jam::JsonDocument::Emit getExpectedEmit (const jam::JsonDocument::Documents& registryView, const jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, int expectedFirst, int expectedCount, std::string_view program, int programIndex, int& outputIndex, int& messages, std::string& report)
{
    return [&registryView, &documents, expectedFirst, expectedCount, program, programIndex, &outputIndex, &messages, &report] (jam::Document::Element& valueContainer, const jam::Document::Token& value) -> bool
    {
        if (outputIndex >= expectedCount)
        {
            appendLine (report, Stdio::fail (text::Diagnostics::jsonTestExtra, programIndex + 1, text::Diagnostics::jsonTestProgramSeparator, program));
            ++messages;
            return true;
        }

        messages += compareExpected (registryView, valueContainer, value, documents, expectedFirst, outputIndex, program, programIndex, report);
        ++outputIndex;
        return true;
    };
}

static int getRunMessages (std::string& report, int outputIndex, int expectedCount, int programIndex, std::string_view program)
{
    if (outputIndex < expectedCount)
    {
        appendLine (report, Stdio::fail (text::Diagnostics::jsonTestInsufficient, programIndex + 3 + outputIndex, text::Diagnostics::jsonTestProgramSeparator, program));
        return 1;
    }

    return 0;
}

static std::pair<int, int> runExpected (const jam::Array<std::string_view>& lines, int programIndex, jam::JsonDocument& filter, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, int expectedFirst, jam::Document::Element& inputContainer, const jam::Document::Token& token, std::string& report, const jam::JsonDocument::ModuleFile& getModuleFile)
{
    const auto program { getLineAt (lines, programIndex) };
    const int expectedCount { documents.size() - expectedFirst };
    auto run { getEmptyDocument() };
    jam::JsonDocument::Registry registry;
    addSourceDocuments (documents, registry, 0);
    registry.try_emplace (run->getRoot(), run.get());
    const jam::JsonDocument::Documents registryView { registry };
    auto& variables { *documents.at (0) };
    jam::JsonDocument::addRunDocuments (filter, variables, registry);
    int outputIndex { 0 };
    int messages { 0 };
    const auto emit { getExpectedEmit (registryView, documents, expectedFirst, expectedCount, program, programIndex, outputIndex, messages, report) };

    const auto addMessage { [&report] (std::string_view message)
    {
        jam::Format::appendBytes (report, message);
        jam::Format::appendUtf8 (report, Chars::newline);
    } };

    const auto [result, code] { run->applyToResults (filter, inputContainer, token, registry, variables, [] () -> std::tuple<jam::JsonDocument*, jam::Document::Element*, const jam::Document::Token*> { return { nullptr, nullptr, nullptr }; }, addMessage, [] () -> std::optional<std::string_view> { return std::nullopt; }, getModuleFile, emit) };
    juce::ignoreUnused (result, code);

    messages += getRunMessages (report, outputIndex, expectedCount, programIndex, program);
    return { messages, 0 };
}

static std::pair<int, int> runProgram (const jam::Array<std::string_view>& lines, int programIndex, int end, jam::JsonDocument& filter, jam::Array<std::unique_ptr<jam::JsonDocument>>& documents, std::string& report, const jam::JsonDocument::ModuleFile& getModuleFile)
{
    const auto inputText { getLineAt (lines, programIndex + 1) };
    auto inputDocument { jam::JsonDocument::readValues (inputText, true, firstLine, firstColumn, false) };

    if (inputDocument->getValueResult().failed() or inputDocument->getRoot()->get<jam::Document::Tokens> (Id::tokens)->size() != 1)
    {
        appendLine (report, Stdio::fail (text::Diagnostics::jsonTestInputInvalid, programIndex + 2, text::Diagnostics::jsonTestProgramSeparator, inputText));
        return { 1, 1 };
    }

    auto& inputContainer { *inputDocument->getRoot() };
    const auto& token { inputContainer.get<jam::Document::Tokens> (Id::tokens)->at (0) };
    documents.add (std::move (inputDocument));
    const int expectedFirst { documents.size() };
    const auto [expectedMessages, expectedMalformed] { addExpectedDocuments (lines, programIndex, end, documents, report) };

    if (expectedMalformed > 0)
        return { expectedMessages, expectedMalformed };

    return runExpected (lines, programIndex, filter, documents, expectedFirst, inputContainer, token, report, getModuleFile);
}

static std::pair<int, int> runCase (const JsonOptions& options, const jam::Array<std::string_view>& lines, int programIndex, int end, std::string& report)
{
    const auto program { getLineAt (lines, programIndex) };
    jam::Array<std::unique_ptr<jam::JsonDocument>> documents;
    documents.add (getEmptyDocument());
    auto& variables { *documents.at (0) };
    variables.addValue (*variables.getRoot(), Id::object);
    addEnvironmentVariables (variables);
    addOriginVariables (options, variables);
    variables.setKeyOrder (getVariableObject (variables));
    const auto getModuleFile { getModuleFileFunction (options) };
    auto [compileResult, filter] (jam::JsonDocument::readFilter (program, getVariableNames (variables), getModuleFile));

    if (compileResult.failed())
    {
        appendLine (report, Stdio::fail (text::Diagnostics::jsonTestErroneous, jam::Strings::getView (compileResult.getErrorMessage()), text::Diagnostics::jsonTestErroneousEnd, programIndex + 2, text::Diagnostics::jsonTestProgramSeparator, program));
        return { 1, 1 };
    }

    return runProgram (lines, programIndex, end, *filter, documents, report, getModuleFile);
}

static std::pair<int, int> runTestBlock (const JsonOptions& options, const jam::Array<std::string_view>& lines, int index, std::string& report)
{
    jam::SharedDocuments sharedDocuments;

    if (isFailMarker (lines.at (index)))
        return runFailCase (options, lines, index + 1, report);

    return runCase (options, lines, index, getBlockEnd (lines, index + 2), report);
}

static Stdio::Output getTestSummary (std::string& report, int passedCount, int runCount, int malformedCount, int skippedCount)
{
    jam::Format::appendUtf8 (report, Chars::newline);
    jam::Format::appendDecimal (report, passedCount);
    jam::Format::appendBytes (report, text::Diagnostics::jsonTestOf);
    jam::Format::appendDecimal (report, runCount);
    jam::Format::appendBytes (report, text::Diagnostics::jsonTestSummary);
    jam::Format::appendDecimal (report, malformedCount);
    jam::Format::appendBytes (report, text::Diagnostics::jsonTestMalformed);
    jam::Format::appendDecimal (report, skippedCount);
    jam::Format::appendBytes (report, text::Diagnostics::jsonTestSkipped);
    jam::Format::appendUtf8 (report, Chars::newline);
    const int exitCode { passedCount == runCount ? Stdio::exitSuccess : Stdio::exitMismatch };
    return Stdio::getOutput (exitCode, Stdio::getDocuments (std::move (report)));
}

static Stdio::Output runTestLines (const JsonOptions& options, const jam::Array<std::string_view>& lines)
{
    std::string report;
    int testIndex { 0 };
    int runCount { 0 };
    int passedCount { 0 };
    int malformedCount { 0 };
    int skippedCount { 0 };
    int index { 0 };

    while (index < lines.size())
    {
        if (isBlank (lines.at (index)) or isComment (lines.at (index)))
        {
            ++index;
        }
        else
        {
            ++testIndex;

            if (isTestSelected (options, testIndex))
            {
                ++runCount;
                appendBlockHeader (report, testIndex, lines, index);
                const auto [testMessages, testMalformed] { runTestBlock (options, lines, index, report) };
                passedCount += testMessages == 0 ? 1 : 0;
                malformedCount += testMalformed;
            }
            else
            {
                ++skippedCount;
            }

            index = getBlockEnd (lines, index + 2) + 1;
        }
    }

    return getTestSummary (report, passedCount, runCount, malformedCount, skippedCount);
}

static Stdio::Output runTests (const JsonOptions& options, const Input::Source& input)
{
    auto [inputStatus, testInput] { options.filterArgument.empty() ? getSourceInput (std::string_view {}, input) : getFileInput (options.filterArgument) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    return runTestLines (options, getLines (testInput->getBytes()));
}

Arguments Json::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments {
        argumentList,
        {
            { Id::jsonNullInputOption, Arguments::OptionType::flag },
            { Id::jsonRawInputOption, Arguments::OptionType::flag },
            { Id::jsonSlurpOption, Arguments::OptionType::flag },
            { Id::jsonCompactOption, Arguments::OptionType::flag },
            { Id::jsonRawOutputOption, Arguments::OptionType::flag },
            { Id::jsonRawOutput0Option, Arguments::OptionType::flag },
            { Id::jsonJoinOutputOption, Arguments::OptionType::flag },
            { Id::jsonAsciiOption, Arguments::OptionType::flag },
            { Id::jsonSortKeysOption, Arguments::OptionType::flag },
            { Id::jsonColourOption, Arguments::OptionType::flag },
            { Id::jsonMonochromeOption, Arguments::OptionType::flag },
            { Id::jsonTabOption, Arguments::OptionType::flag },
            { Id::jsonIndentOption, Arguments::OptionType::value },
            { Id::jsonUnbufferedOption, Arguments::OptionType::flag },
            { Id::jsonStreamOption, Arguments::OptionType::flag },
            { Id::jsonStreamErrorsOption, Arguments::OptionType::flag },
            { Id::jsonSeqOption, Arguments::OptionType::flag },
            { Id::jsonFromFileOption, Arguments::OptionType::flag },
            { Id::jsonLibraryPathOption, Arguments::OptionType::value },
            { Id::jsonArgNamedOption, Arguments::OptionType::namedValue },
            { Id::jsonArgJsonOption, Arguments::OptionType::namedValue },
            { Id::jsonSlurpFileOption, Arguments::OptionType::namedValue },
            { Id::jsonRawFileOption, Arguments::OptionType::namedValue },
            { Id::jsonArgsOption, Arguments::OptionType::mode },
            { Id::jsonJsonArgsOption, Arguments::OptionType::mode },
            { Id::jsonExitStatusOption, Arguments::OptionType::flag },
            { Id::jsonBinaryOption, Arguments::OptionType::flag },
            { Id::versionOption, Arguments::OptionType::flag },
            { Id::jsonBuildConfigurationOption, Arguments::OptionType::flag },
            { Id::jsonRunTestsOption, Arguments::OptionType::flag },
            { Id::jsonSkipOption, Arguments::OptionType::value },
            { Id::jsonTakeOption, Arguments::OptionType::value }
        },
        Arguments::Grammar::separated
    };
}

Stdio::Output Json::run (const Arguments& arguments, const Input::Source& input)
{
    const JsonOptions options { arguments };

    if (options.getStatus().failed())
        return Stdio::getFailure (options.getStatus());

    if (options.isRunTests)
        return runTests (options, input);

    if (options.isVersion or options.isBuildConfiguration)
        return getInformation (options);

    return runFilter (options, input);
}
