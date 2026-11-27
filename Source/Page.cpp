#include "Page.h"
#include "Arguments.h"
#include "IgnoreRules.h"
#include "Input.h"
#include "LineDiff.h"
#include "NonPrintable.h"
#include "PageConfiguration.h"
#include "PageOptions.h"
#include "PageRanges.h"
#include "Paths.h"
#include "StandardStream.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

using WriteFunction = int (*) (std::string_view, const jam::Document::Tokens&, const juce::StringPairArray&,
                               std::string_view, std::string_view, int, bool, juce::Span<const juce::Range<int>>,
                               juce::Span<const int>, juce::Span<const int>, juce::Span<const juce::Range<int>>,
                               juce::OutputStream&);

using RunFunction = int (*) (std::string_view, const jam::Document::Tokens&, const juce::StringPairArray&,
                             std::string_view, std::string_view, int, bool, juce::Span<const juce::Range<int>>,
                             juce::Span<const int>, juce::Span<const int>, juce::Span<const juce::Range<int>>, bool);

static constexpr char colonChar { ':' };
static constexpr int firstLine { 1 };
static constexpr int firstIndex { 0 };

static size_t getObjectIdLength (const juce::File& commonDirectory)
{
    const auto objectFormat { IgnoreRules::getConfigEntry (commonDirectory.getChildFile (Id::config.toString()),
                                                           Id::objectFormatSection,
                                                           Id::objectFormatKey) };

    if (std::string_view { objectFormat }.compare (Id::sha256Format) == 0)
        return jam::GitDocument::sha256IdLength;

    return jam::GitDocument::sha1IdLength;
}

static jam::Array<int> getMarks (const Input::Source& blobSource, const TextDocument& document)
{
    auto [status, blobInput] { Input::create (blobSource, Input::allLines) };
    jassert (status.wasOk());
    const auto blobDocument { TextDocument::fromInput (std::move (blobInput), {}) };
    const LineDiff diff { *blobDocument, document };
    return diff.getLineChanges (document.getNumLines());
}

static jam::Array<int> getLineChanges (const juce::File& file, const TextDocument& document)
{
    const auto workTree { IgnoreRules::getTopLevelDirectory (file.getParentDirectory()) };
    const auto gitDirectory { jam::GitDocument::getGitDirectory (workTree) };

    if (gitDirectory.isDirectory())
    {
        const auto commonDirectory { jam::GitDocument::getCommonDirectory (gitDirectory) };
        const auto gitDocument { jam::GitDocument::parse (file, workTree, gitDirectory, commonDirectory, getObjectIdLength (commonDirectory)) };

        if (gitDocument != nullptr)
        {
            const auto blob { gitDocument->getBlob() };

            if (blob.has_value())
            {
                const Input::Source blobSource { *blob };
                return getMarks (blobSource, document);
            }
        }
    }

    return {};
}

static jam::Array<int> getDocumentChanges (const juce::File& file, const TextDocument& document)
{
    if (file.existsAsFile())
        return getLineChanges (file, document);

    return {};
}

static jam::Array<juce::Result> getFailureList (const juce::Result& failure)
{
    jam::Array<juce::Result> failures;
    failures.add (failure);
    return failures;
}

static std::string_view getDisplayPath (std::string_view path)
{
    if (path.compare (Id::stdinArgument) == 0)
        return Id::stdinPath;

    return path;
}

static Stdio::Documents getDocumentList (std::unique_ptr<TextDocument> document)
{
    Stdio::Documents documents;
    documents.add (std::move (document));
    return documents;
}

static juce::Span<const std::string_view> getRawPaths (juce::Span<const std::string_view> positionals, const Input::Source& input)
{
    static constexpr std::array defaultPaths { Id::currentDirectory };
    static constexpr std::array stdinPaths { Id::stdinArgument };

    if (positionals.empty() and Input::hasData (input))
        return juce::Span<const std::string_view> { stdinPaths };

    if (positionals.empty())
        return juce::Span<const std::string_view> { defaultPaths };

    return positionals;
}

static juce::Result getDirectoryFailure (std::string_view path)
{
    return Stdio::fail (text::Diagnostics::failPathQuote, path, text::Diagnostics::failIsDirectory);
}

static std::pair<juce::Result, std::unique_ptr<Input>> getFileInput (std::string_view path)
{
    const auto file { Paths::getLinkedTarget (path) };

    if (file.isDirectory())
        return { getDirectoryFailure (path), nullptr };

    auto fileInput { Input::fromFile (file, Input::maxBufferedReadSize) };

    if (fileInput != nullptr)
        return { juce::Result::ok(), std::move (fileInput) };

    return { TextDocument::getReadFailure (file, path), nullptr };
}

static std::pair<juce::Result, std::unique_ptr<Input>> getPathInput (std::string_view path, const Input::Source& input)
{
    if (path.compare (Id::stdinArgument) == 0)
        return Input::create (input, Input::allLines);

    return getFileInput (path);
}

static bool isBinaryPage (std::string_view bytes, bool isBinaryText)
{
    return Stdio::isTerminalOutput() and TextDocument::isBinary (bytes) and not isBinaryText;
}

static juce::File getTokensLibraryFile()
{
#if JUCE_WINDOWS
    return Paths::getCarriedFile (files::tokensLibraryName, files::tokensLibraryWindows);
#else
    return Paths::getCarriedFile (files::tokensLibraryName, files::tokensLibrary);
#endif
}

template <typename FunctionType>
static FunctionType getEntryFunction (juce::DynamicLibrary& library, std::string_view entryName)
{
    return reinterpret_cast<FunctionType> (library.getFunction (jam::Strings::fromView (entryName)));
}

static bool isPageLibraryReady (juce::DynamicLibrary& pageLibrary)
{
    return getEntryFunction<WriteFunction> (pageLibrary, Id::writeEntryName) != nullptr
           and getEntryFunction<RunFunction> (pageLibrary, Id::runEntryName) != nullptr;
}

static bool hasComponent (const PageOptions& options, PageOptions::Component::value component)
{
    return (options.components & component) != 0;
}

static bool isColourOn (const PageOptions& options)
{
    const bool isAutomatic { options.colourMode == PageOptions::Choice::automatic };
    const bool hasNoColour { not jam::Strings::getView (juce::SystemStats::getEnvironmentVariable (jam::Strings::fromView (Id::noColourVariable), {})).empty() };
    return options.colourMode == PageOptions::Choice::always or (isAutomatic and Stdio::isTerminalOutput() and not hasNoColour);
}

static bool isPagingOn (const PageOptions& options)
{
    const bool isAutomatic { options.pagingMode == PageOptions::Choice::automatic };
    return options.pagingMode == PageOptions::Choice::always or (isAutomatic and Stdio::isTerminalOutput());
}

static bool isRenderedPage (const Input::Source& input, const PageOptions& options)
{
    const bool isCommandLine { std::holds_alternative<Input::StandardInput> (input) };
    const bool isShowingDecorations { options.isDecorated and options.components != 0 };
    return isCommandLine and (isShowingDecorations or isColourOn (options) or options.pagingMode == PageOptions::Choice::always);
}

static std::string_view getWithoutIgnoredSuffix (std::string_view name, juce::Span<const std::string_view> ignoredSuffixes)
{
    for (const auto suffix : ignoredSuffixes)
    {
        if (name.size() >= suffix.size() and name.substr (name.size() - suffix.size()).compare (suffix) == 0)
            return name.substr (0, name.size() - suffix.size());
    }

    return name;
}

static std::string_view getDetectionName (std::string_view displayPath, const PageOptions& options)
{
    const std::string_view name { options.fileName.empty() ? displayPath : options.fileName };
    return getWithoutIgnoredSuffix (name, options.ignoredSuffixes);
}

static bool isNoTheme (std::string_view themeName)
{
    return themeName.empty() or themeName.compare (Id::autoValue) == 0 or themeName.compare (Id::defaultValue) == 0;
}

static std::optional<std::string> getThemeSheet (const PageConfiguration& configuration, std::string_view themeName)
{
    if (isNoTheme (themeName))
        return std::nullopt;

    return configuration.getStyleSheet (themeName);
}

static bool isUnknownTheme (std::string_view themeName, const std::optional<std::string>& themeSheet)
{
    return not isNoTheme (themeName) and not themeSheet.has_value();
}

static bool hasTextTransform (const PageOptions& options)
{
    return options.isShowingAll or options.isStrippingAnsi or options.isSanitizing;
}

static std::string getTransformedText (std::string_view text, const PageOptions& options)
{
    jassert (hasTextTransform (options));
    std::string visible { options.isShowingAll ? NonPrintable::getVisible (text, options.notation) : std::string {} };
    const std::string_view visibleText { options.isShowingAll ? std::string_view { visible } : text };
    std::string stripped { options.isStrippingAnsi ? NonPrintable::getWithoutSequences (visibleText) : std::string {} };
    const std::string_view strippedText { options.isStrippingAnsi ? std::string_view { stripped } : visibleText };

    if (options.isSanitizing)
        return NonPrintable::getSanitized (strippedText);

    if (options.isStrippingAnsi)
        return stripped;

    return visible;
}

static bool hasTab (std::string_view text)
{
    static constexpr char tabChar { static_cast<char> (Chars::tab) };
    return jam::simd::findByte (text.data(), text.size(), 0, tabChar) < text.size();
}

static bool isTabExpanded (std::string_view text, const PageOptions& options)
{
    static constexpr size_t noTabWidth { 0 };
    return hasTab (text) and options.tabWidth > noTabWidth;
}

static std::string_view getWithoutByteOrderMark (std::string_view bytes)
{
    static constexpr std::string_view byteOrderMark { Chars::byteOrderMark };

    if (bytes.substr (0, byteOrderMark.size()).compare (byteOrderMark) == 0)
        return bytes.substr (byteOrderMark.size());

    return bytes;
}

static bool isRenderedTransformOn (std::string_view bytes, const PageOptions& options)
{
    const std::string_view text { getWithoutByteOrderMark (bytes) };
    return NonPrintable::isUtf16 (text) or hasTextTransform (options) or isTabExpanded (text, options);
}

static std::string getRenderedText (std::string_view bytes, const PageOptions& options)
{
    const std::string_view text { getWithoutByteOrderMark (bytes) };
    const bool isDecoded { NonPrintable::isUtf16 (text) };
    std::string decoded { isDecoded ? NonPrintable::getDecoded (text) : std::string {} };
    const std::string_view decodedText { isDecoded ? std::string_view { decoded } : text };
    const bool isTransformed { hasTextTransform (options) };
    std::string transformed { isTransformed ? getTransformedText (decodedText, options) : std::string {} };
    const std::string_view transformedText { isTransformed ? std::string_view { transformed } : decodedText };

    if (isTabExpanded (transformedText, options))
        return NonPrintable::getExpanded (transformedText, options.tabWidth);

    if (isTransformed)
        return transformed;

    return decoded;
}

static std::string getSelectedText (std::string_view bytes, const jam::Array<juce::Range<int>>& lineRanges, const PageOptions& options)
{
    std::string text { PageRanges::getJoinedBytes (bytes, lineRanges) };

    if (hasTextTransform (options))
        text = getTransformedText (text, options);

    if (options.isSqueezing)
        text = NonPrintable::getLinesText (text, NonPrintable::getSqueezedLines (text, options.squeezeLimit));

    return text;
}

static std::string_view getSingleRangeBytes (std::string_view bytes, const jam::Array<juce::Range<int>>& lineRanges)
{
    jassert (lineRanges.size() <= 1);

    if (lineRanges.isEmpty())
        return {};

    return PageRanges::getRangeBytes (bytes, lineRanges.at (firstIndex));
}

static std::unique_ptr<TextDocument> getSelectedDocument (std::unique_ptr<Input> pageInput, std::string_view displayPath,
                                                          const jam::Array<juce::Range<int>>& lineRanges, const PageOptions& options)
{
    const auto bytes { pageInput->getBytes() };

    if (hasTextTransform (options) or options.isSqueezing)
        return TextDocument::fromText (getSelectedText (bytes, lineRanges, options), displayPath);

    if (lineRanges.size() > 1)
        return TextDocument::fromText (PageRanges::getJoinedBytes (bytes, lineRanges), displayPath);

    return TextDocument::fromInput (std::move (pageInput), displayPath, getSingleRangeBytes (bytes, lineRanges));
}

static Stdio::Output getSelectedOutput (std::unique_ptr<Input> pageInput, std::string_view displayPath, const PageOptions& options)
{
    const auto bytes { pageInput->getBytes() };
    const auto [rangeStatus, lineRanges] { PageRanges::getRanges (options.lineRangeTexts, PageRanges::getLineCount (bytes)) };

    if (rangeStatus.failed())
        return Stdio::getOutput (Stdio::exitMismatch, {}, getFailureList (rangeStatus));

    return Stdio::getOutput (Stdio::exitSuccess, getDocumentList (getSelectedDocument (std::move (pageInput), displayPath, lineRanges, options)));
}

static Stdio::Output getPlainOutput (std::unique_ptr<Input> pageInput, std::string_view displayPath, const PageOptions& options)
{
    if (hasTextTransform (options) or options.isSqueezing or not options.lineRangeTexts.empty())
        return getSelectedOutput (std::move (pageInput), displayPath, options);

    const auto bytes { pageInput->getBytes() };
    return Stdio::getOutput (Stdio::exitSuccess, getDocumentList (TextDocument::fromInput (std::move (pageInput), displayPath, bytes)));
}

static size_t getLastColon (std::string_view text)
{
    size_t lastColon { text.size() };
    size_t colon { jam::simd::findByte (text.data(), text.size(), 0, colonChar) };

    while (colon < text.size())
    {
        lastColon = colon;
        colon = jam::simd::findByte (text.data(), text.size(), colon + 1, colonChar);
    }

    return lastColon;
}

static juce::File getDetectionFile (std::string_view detectionName)
{
    return juce::File::getCurrentWorkingDirectory().getChildFile (jam::Strings::fromView (detectionName));
}

static std::string_view getMappedName (std::string_view detectionName, juce::Span<const std::string_view> syntaxMappings)
{
    const auto baseName { getDetectionFile (detectionName).getFileName() };

    for (const auto mapping : syntaxMappings)
    {
        const auto colon { getLastColon (mapping) };

        if (colon < mapping.size() and Paths::isWildcardMatch (mapping.substr (0, colon), jam::Strings::getView (baseName)))
            return mapping.substr (colon + 1);
    }

    return {};
}

static std::string_view getLanguageName (std::string_view detectionName, const PageOptions& options)
{
    if (options.languageName.empty())
        return getMappedName (detectionName, options.syntaxMappings);

    return options.languageName;
}

static std::shared_ptr<const jam::LanguageLibrary> getFallbackLanguage (const juce::File& tokensFile, const PageOptions& options)
{
    if (options.fallbackLanguageName.empty())
        return jam::LanguageLibrary::getLanguageLibrary (tokensFile, Id::plainTextTitle, {});

    const auto fallbackLanguage { jam::LanguageLibrary::getLanguageLibrary (tokensFile, options.fallbackLanguageName, {}) };

    if (fallbackLanguage != nullptr)
        return fallbackLanguage;

    return jam::LanguageLibrary::getLanguageLibrary (tokensFile, Id::plainTextTitle, {});
}

static std::shared_ptr<const jam::LanguageLibrary> getLanguage (std::string_view detectionName, const PageOptions& options)
{
    const auto tokensFile { getTokensLibraryFile() };
    const auto language { jam::LanguageLibrary::getLanguageLibrary (tokensFile, getLanguageName (detectionName, options), getDetectionFile (detectionName)) };

    if (language != nullptr)
        return language;

    return getFallbackLanguage (tokensFile, options);
}

static std::unique_ptr<jam::LanguageDocument> getLanguageDocument (std::string_view text, std::string_view detectionName, const PageOptions& options)
{
    const auto language { getLanguage (detectionName, options) };

    if (language != nullptr)
        return jam::LanguageDocument::parse (text, language);

    return nullptr;
}

static void appendRowText (std::string& text, std::string_view title, std::string_view globs)
{
    static constexpr char commaChar { ',' };
    const auto globTokens { jam::Strings::fromTokens (jam::Strings::fromView (globs), juce::String::charToString (Chars::space), {}) };
    jam::Format::appendBytes (text, title);
    jam::Format::appendByte (text, colonChar);

    const auto extensionGlobPrefix { jam::Strings::getView (Id::asteriskDot.toString()) };

    for (int index { 0 }; index < globTokens.size(); ++index)
    {
        if (index > 0)
            jam::Format::appendByte (text, commaChar);

        const auto glob { globTokens.getView (index) };
        const bool isPrefixed { glob.substr (0, extensionGlobPrefix.size()).compare (extensionGlobPrefix) == 0 };
        jam::Format::appendBytes (text, isPrefixed ? glob.substr (extensionGlobPrefix.size()) : glob);
    }

    jam::Format::appendBytes (text, Stdio::newline);
}

static std::string getLanguageListText()
{
    std::string text;
    jam::LanguageLibrary::applyToLanguages (getTokensLibraryFile(), [&text] (std::string_view, std::string_view title, std::string_view globs)
    {
        appendRowText (text, title, globs);
    });

    return text;
}

static jam::Array<int> getLineNumbers (int lineCount)
{
    jam::Array<int> numbers;

    for (int line { firstLine }; line < lineCount + firstLine; ++line)
        numbers.add (line);

    return numbers;
}

static jam::Array<int> getOriginalNumbers (std::string_view text, const PageOptions& options)
{
    if (options.isSqueezing)
        return NonPrintable::getSqueezedLines (text, options.squeezeLimit);

    return getLineNumbers (PageRanges::getLineCount (text));
}

static std::string getSqueezedText (std::string_view text, const jam::Array<int>& squeezedNumbers, const PageOptions& options)
{
    if (options.isSqueezing)
        return NonPrintable::getLinesText (text, squeezedNumbers);

    return {};
}

static std::pair<juce::Result, jam::Array<juce::Range<int>>> getLineRanges (const PageOptions& options, const jam::Array<int>& shownChanges, int lineCount)
{
    if (options.isDiff)
        return { juce::Result::ok(), PageRanges::getChangedRanges (shownChanges, options.diffContext, lineCount) };

    return PageRanges::getRanges (options.lineRangeTexts, lineCount);
}

static std::pair<juce::Result, jam::Array<juce::Range<int>>> getHighlightRanges (juce::Span<const std::string_view> highlightTexts, int lineCount)
{
    if (highlightTexts.empty())
        return { juce::Result::ok(), jam::Array<juce::Range<int>> {} };

    return PageRanges::getRanges (highlightTexts, lineCount);
}

static std::string_view getShownName (std::string_view displayPath, std::string_view fileName, bool isStandardInput)
{
    if (isStandardInput)
        return text::Formats::stdinLabel;

    if (fileName.empty())
        return displayPath;

    return fileName;
}

static juce::StringPairArray getProperties (std::string_view displayPath, size_t byteCount, bool isStandardInput, bool isEmpty,
                                            const PageOptions& options)
{
    juce::StringPairArray properties;

    if (hasComponent (options, PageOptions::Component::headerFileName))
    {
        std::string name;
        jam::Format::appendBytes (name, getShownName (displayPath, options.fileName, isStandardInput));

        if (isEmpty)
            jam::Format::appendBytes (name, text::Formats::emptyMark);

        properties.set (jam::Strings::fromView (text::Formats::fileLabel), jam::Strings::fromView (name));
    }

    if (hasComponent (options, PageOptions::Component::headerFileSize) and not isStandardInput)
        properties.set (jam::Strings::fromView (text::Formats::sizeLabel), juce::File::descriptionOfSizeInBytes (static_cast<juce::int64> (byteCount)));

    return properties;
}

static Stdio::Output getPageOutput (std::string_view text, const jam::Document::Tokens& tokens, const juce::StringPairArray& properties,
                                    std::string_view themeSheet, std::string_view gridSheet, const jam::Array<juce::Range<int>>& lineRanges,
                                    const jam::Array<juce::Range<int>>& highlightRanges, const jam::Array<int>& numbers,
                                    juce::Span<const int> changes, const PageOptions& options, juce::DynamicLibrary& pageLibrary)
{
    static constexpr int pagerShown { 1 };
    const auto writeFunction { getEntryFunction<WriteFunction> (pageLibrary, Id::writeEntryName) };
    const auto runFunction { getEntryFunction<RunFunction> (pageLibrary, Id::runEntryName) };
    const auto invoke { [&text, &tokens, &properties, themeSheet, gridSheet, &lineRanges, &highlightRanges, &numbers, &changes, &options]
                        (auto function, auto&&... extra)
    {
        return function (text, tokens, properties, themeSheet, gridSheet, options.columns, options.isWrapping,
                         juce::Span<const juce::Range<int>> { lineRanges }, juce::Span<const int> { numbers },
                         changes, juce::Span<const juce::Range<int>> { highlightRanges }, extra...);
    } };

    if (isPagingOn (options) and invoke (runFunction, options.pagingMode == PageOptions::Choice::always) == pagerShown)
        return Stdio::getOutput (Stdio::exitSuccess);

    if (isColourOn (options))
    {
        StandardStream output { StandardStream::output };
        invoke (writeFunction, output);
        return Stdio::getOutput (Stdio::exitSuccess);
    }

    juce::MemoryOutputStream output;
    invoke (writeFunction, output);
    const std::string_view page { static_cast<const char*> (output.getData()), output.getDataSize() };
    return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (NonPrintable::getWithoutSequences (page)));
}

static Stdio::Output getLayoutOutput (std::string_view text, const jam::Document::Tokens& tokens, const TextDocument& document,
                                      const juce::File& file, std::string_view displayPath, const jam::Array<int>& originalNumbers,
                                      const PageOptions& options, const std::optional<std::string>& themeSheet,
                                      juce::DynamicLibrary& pageLibrary)
{
    const int lineCount { PageRanges::getLineCount (text) };
    const bool isChangesShown { hasComponent (options, PageOptions::Component::changes) };
    const jam::Array<int> documentChanges (isChangesShown or options.isDiff ? getDocumentChanges (file, document) : jam::Array<int> {});
    const jam::Array<int> shownChanges (PageRanges::getShownChanges (documentChanges, originalNumbers, lineCount));
    const auto [lineStatus, lineRanges] { getLineRanges (options, shownChanges, lineCount) };
    const auto [highlightStatus, highlightRanges] { getHighlightRanges (options.highlightRangeTexts, lineCount) };

    if (lineStatus.failed())
        return Stdio::getOutput (Stdio::exitMismatch, {}, getFailureList (lineStatus));

    if (highlightStatus.failed())
        return Stdio::getOutput (Stdio::exitMismatch, {}, getFailureList (highlightStatus));

    if ((options.isDiff and lineRanges.isEmpty()) or (text.empty() and options.isQuietEmpty))
        return Stdio::getOutput (Stdio::exitSuccess);

    const bool isNumbersShown { hasComponent (options, PageOptions::Component::numbers) };
    const jam::Array<int> numbers (isNumbersShown ? PageRanges::getShownNumbers (text, originalNumbers, options.isNumberingNonblank) : jam::Array<int> {});
    const juce::Span<const int> changes { isChangesShown ? juce::Span<const int> { shownChanges } : juce::Span<const int> {} };
    const bool isStandardInput { file.getFullPathName().isEmpty() };
    const auto properties (getProperties (displayPath, document.getBytes().size(), isStandardInput, text.empty(), options));

    return getPageOutput (text, tokens, properties, themeSheet.has_value() ? std::string_view { *themeSheet } : std::string_view {}, hasComponent (options, PageOptions::Component::grid) ? std::string_view {} : Id::noGridStyleSheet, lineRanges, highlightRanges, numbers, changes, options, pageLibrary);
}

static Stdio::Output getRenderedOutput (std::unique_ptr<TextDocument> document, const juce::File& file, std::string_view displayPath,
                                        const PageOptions& options, const std::optional<std::string>& themeSheet,
                                        juce::DynamicLibrary& pageLibrary)
{
    const auto documentBytes { document->getBytes() };
    const bool isTransformed { isRenderedTransformOn (documentBytes, options) };
    std::string rendered { isTransformed ? getRenderedText (documentBytes, options) : std::string {} };
    const std::string_view bytes { isTransformed ? std::string_view { rendered } : getWithoutByteOrderMark (documentBytes) };
    const jam::Array<int> originalNumbers (getOriginalNumbers (bytes, options));
    const auto squeezed { getSqueezedText (bytes, originalNumbers, options) };
    const std::string_view text { options.isSqueezing ? std::string_view { squeezed } : bytes };
    const auto languageDocument { getLanguageDocument (text, getDetectionName (displayPath, options), options) };
    const jam::Document::Tokens noTokens;
    const jam::Document::Tokens* tokens { &noTokens };
    std::string_view source { text };

    if (languageDocument != nullptr)
    {
        source = languageDocument->getSource();
        tokens = languageDocument->getRoot()->get<jam::Document::Tokens> (Id::tokens);
    }

    return getLayoutOutput (source, *tokens, *document, file, displayPath, originalNumbers, options, themeSheet, pageLibrary);
}

static Stdio::Output getPathOutput (std::string_view path, const Input::Source& input, const PageOptions& options,
                                    const std::optional<std::string>& themeSheet, juce::DynamicLibrary& pageLibrary)
{
    auto [inputStatus, pageInput] { getPathInput (path, input) };

    if (inputStatus.failed())
        return Stdio::getOutput (Stdio::exitMismatch, {}, getFailureList (inputStatus));

    if (isBinaryPage (pageInput->getBytes(), options.isBinaryText))
        return Stdio::getOutput (Stdio::exitSuccess, {}, getFailureList (TextDocument::getBinaryFailure (getDisplayPath (path))));

    const auto displayPath { getDisplayPath (path) };
    const bool isStandardInput { path.compare (Id::stdinArgument) == 0 };
    const juce::File file { isStandardInput ? juce::File {} : Paths::getLinkedTarget (path) };

    if (isRenderedPage (input, options) and isPageLibraryReady (pageLibrary))
        return getRenderedOutput (TextDocument::fromInput (std::move (pageInput), displayPath), file, displayPath, options, themeSheet, pageLibrary);

    return getPlainOutput (std::move (pageInput), displayPath, options);
}

static Stdio::Output getCombinedOutput (jam::Array<Stdio::Output> outputs)
{
    Stdio::Documents documents;
    Stdio::Documents errorDocuments;
    jam::Array<juce::Result> failures;
    int exitCode { Stdio::exitSuccess };

    for (auto& [outputExitCode, outputDocuments, outputFailures, outputErrorDocuments] : outputs)
    {
        exitCode = std::max (exitCode, outputExitCode);
        Stdio::addDocuments (documents, std::move (outputDocuments));
        Stdio::addDocuments (errorDocuments, std::move (outputErrorDocuments));

        for (const auto& failure : outputFailures)
            failures.add (failure);
    }

    return Stdio::getOutput (exitCode, std::move (documents), std::move (failures), std::move (errorDocuments));
}

static Stdio::Output getPagesOutput (const Arguments& arguments, const PageOptions& options, const Input::Source& input,
                                     const PageConfiguration& configuration)
{
    const auto rawPaths { getRawPaths (arguments.getPositionals (0), input) };
    const bool isRendering { isRenderedPage (input, options) };
    juce::DynamicLibrary pageLibrary;

    if (isRendering)
        pageLibrary.open (PageConfiguration::getPageLibraryFile().getFullPathName());

    const auto themeSheet { getThemeSheet (configuration, options.themeName) };
    jam::Array<Stdio::Output> outputs;

    if (isRendering and isPageLibraryReady (pageLibrary) and isUnknownTheme (options.themeName, themeSheet))
        outputs.add (Stdio::getOutput (Stdio::exitSuccess, {}, getFailureList (Stdio::fail (text::Diagnostics::failThemeUnknown, options.themeName))));

    for (const auto path : rawPaths)
    {
        outputs.add (getPathOutput (path, input, options, themeSheet, pageLibrary));
    }

    return getCombinedOutput (std::move (outputs));
}

static std::string getPathLineText (const juce::File& file)
{
    std::string text;
    jam::Format::appendBytes (text, jam::Strings::getView (file.getFullPathName()));
    jam::Format::appendBytes (text, Stdio::newline);
    return text;
}

static std::string getVersionText (const PageConfiguration&, const PageOptions&)
{
    return Stdio::getVersionLine();
}

static std::string getConfigFileText (const PageConfiguration& configuration, const PageOptions&)
{
    return getPathLineText (configuration.getConfigFile());
}

static std::string getConfigDirectoryText (const PageConfiguration& configuration, const PageOptions&)
{
    return getPathLineText (configuration.getConfigDirectory());
}

static std::string getThemeListText (const PageConfiguration& configuration, const PageOptions&)
{
    return configuration.getThemeText();
}

static std::string getAcknowledgementsText (const PageConfiguration&, const PageOptions&)
{
    return {};
}

using OneShotText = std::string (*) (const PageConfiguration&, const PageOptions&);

static constexpr std::array<std::pair<std::string_view, OneShotText>, 5> oneShotOptions {{
    { Id::versionOption, getVersionText },
    { Id::configFileOption, getConfigFileText },
    { Id::configDirOption, getConfigDirectoryText },
    { Id::listThemesOption, getThemeListText },
    { Id::acknowledgementsOption, getAcknowledgementsText }
}};

static std::optional<Stdio::Output> getOneShotOutput (const Arguments& arguments, const PageOptions& options,
                                                      const PageConfiguration& configuration)
{
    for (const auto& [option, getText] : oneShotOptions)
    {
        if (arguments.contains (option))
            return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (getText (configuration, options)));
    }

    if (arguments.contains (Id::listLanguagesOption))
        return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (getLanguageListText()));

    return std::nullopt;
}

Arguments Page::getArguments (juce::Span<const std::string_view> argumentList)
{
    return PageOptions::getArguments (argumentList);
}

Stdio::Output Page::run (const Arguments& arguments, const Input::Source& input)
{
    const PageConfiguration configuration;
    const PageOptions options { arguments, configuration.getArguments() };
    auto oneShotOutput { getOneShotOutput (arguments, options, configuration) };

    if (oneShotOutput.has_value())
        return std::move (*oneShotOutput);

    if (options.getStatus().failed())
        return Stdio::getFailure (options.getStatus());

    return getPagesOutput (arguments, options, input, configuration);
}
