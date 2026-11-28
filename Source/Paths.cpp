#include "Paths.h"
#include "Jobs.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static std::string getNormalizedPath (std::string_view rawPath)
{
    std::string normalized { Paths::getDisplayPath (rawPath) };

    while (normalized.size() > 1 and normalized.back() == static_cast<char> (Chars::slash))
        normalized.pop_back();

    return normalized;
}

static std::string getNormalizedDisplayPrefix (std::string_view rawPath)
{
    auto normalized { getNormalizedPath (rawPath) };

    if (normalized.empty() or normalized.back() != static_cast<char> (Chars::slash))
        jam::Format::appendUtf8 (normalized, Chars::slash);

    return normalized;
}

bool Paths::isWildcardMatch (std::string_view wildcard, std::string_view text) noexcept
{
    size_t wildIndex { 0 };
    size_t textIndex { 0 };
    size_t starIndex { std::string_view::npos };
    size_t matchIndex { 0 };

    while (textIndex < text.size())
    {
        const bool hasWild { wildIndex < wildcard.size() };

        if (hasWild and wildcard.at (wildIndex) == static_cast<char> (Chars::asterisk))
        {
            starIndex = wildIndex;
            ++wildIndex;
            matchIndex = textIndex;
        }
        else if (hasWild
                 and (wildcard.at (wildIndex) == static_cast<char> (Chars::question)
                      or wildcard.at (wildIndex) == text.at (textIndex)))
        {
            ++wildIndex;
            ++textIndex;
        }
        else
        {
            if (starIndex == std::string_view::npos)
                return false;

            wildIndex = starIndex + 1;
            ++matchIndex;
            textIndex = matchIndex;
        }
    }

    while (wildIndex < wildcard.size() and wildcard.at (wildIndex) == static_cast<char> (Chars::asterisk))
        ++wildIndex;

    return wildIndex == wildcard.size();
}

static bool matchesAnyGlob (std::string_view fileName,
                            std::string_view displayPath,
                            std::string_view relativePath,
                            juce::Span<const std::string_view> globs)
{
    for (const auto& glob : globs)
    {
        if (Paths::isWildcardMatch (glob, fileName) or Paths::isWildcardMatch (glob, displayPath) or Paths::isWildcardMatch (glob, relativePath))
            return true;
    }

    return false;
}

static bool shouldIncludeFile (std::string_view fileName,
                               std::string_view displayPath,
                               std::string_view relativePath,
                               juce::Span<const std::string_view> includeGlobs,
                               juce::Span<const std::string_view> excludeGlobs)
{
    if (not includeGlobs.empty() and not matchesAnyGlob (fileName, displayPath, relativePath, includeGlobs))
        return false;

    if (not excludeGlobs.empty() and matchesAnyGlob (fileName, displayPath, relativePath, excludeGlobs))
        return false;

    return true;
}

static void appendPathSegment (std::string& path, std::string_view segment)
{
    if (path.empty() or path.back() != static_cast<char> (Chars::slash))
        jam::Format::appendUtf8 (path, Chars::slash);

    jam::Format::appendBytes (path, segment);
}

static bool shouldVisitEntry (std::string_view name,
                              std::string_view gitDirectoryName,
                              bool isIncludingHidden) noexcept
{
    if (name.compare (gitDirectoryName) == 0)
        return false;

    if (not isIncludingHidden and name.at (0) == static_cast<char> (Chars::dot))
        return false;

    return true;
}

static constexpr int entriesPerItem { 64 };

using FolderRange = std::pair<std::shared_ptr<const Paths::ListedFolder>, juce::Range<int>>;
using WalkItem = std::variant<Paths::Folder, FolderRange>;

#if JUCE_WINDOWS
static std::optional<std::string> getEntryName (const std::filesystem::directory_entry& entry)
{
    try
    {
        return entry.path().filename().u8string();
    }
    catch (const std::system_error&)
    {
        return std::nullopt;
    }
}
#else
static std::optional<std::string_view> getEntryName (const std::filesystem::directory_entry& entry)
{
    const std::string_view path { entry.path().native() };
    return path.substr (path.rfind (static_cast<char> (Chars::slash)) + 1);
}
#endif

static std::optional<Paths::EntryType> getEntryType (const std::filesystem::directory_entry& entry)
{
    std::error_code entryError;

    if (entry.is_symlink (entryError))
        return Paths::EntryType::symlink;

    if (entry.is_directory (entryError) and std::filesystem::is_directory (entry.symlink_status (entryError)))
        return Paths::EntryType::directory;

    if (entry.is_regular_file (entryError))
        return Paths::EntryType::file;

    return std::nullopt;
}

static bool isListedEntry (std::optional<Paths::EntryType> entryType, bool isListingEntries)
{
    return entryType.has_value() and (*entryType != Paths::EntryType::symlink or isListingEntries);
}

static bool isCountedEntry (std::optional<Paths::EntryType> entryType)
{
    return entryType == Paths::EntryType::file or entryType == Paths::EntryType::directory;
}

static juce::int64 getUnixMilliseconds (std::filesystem::file_time_type fileTime)
{
#if JUCE_WINDOWS
    static constexpr juce::int64 windowsEpochOffsetMilliseconds { 11644473600000 };
    return juce::Time (fileTime).toMilliseconds() - windowsEpochOffsetMilliseconds;
#else
    return juce::Time (fileTime).toMilliseconds();
#endif
}

static std::optional<Paths::Metadata> getLinkMetadata (const std::filesystem::directory_entry& entry)
{
    std::error_code linkError;
    std::error_code targetError;
    std::error_code timeError;
    const int permissions { static_cast<int> (entry.symlink_status (linkError).permissions()) };
    const bool hasTarget { std::filesystem::exists (entry.status (targetError)) };
    const juce::int64 modifiedMilliseconds { hasTarget ? getUnixMilliseconds (entry.last_write_time (timeError)) : juce::int64 { 0 } };

    if (linkError.value() != 0 or (hasTarget and timeError.value() != 0))
        return std::nullopt;

    return Paths::Metadata { juce::int64 { 0 }, modifiedMilliseconds, permissions };
}

static std::optional<Paths::Metadata> getMetadata (const std::filesystem::directory_entry& entry, Paths::EntryType entryType)
{
    if (entryType == Paths::EntryType::symlink)
        return getLinkMetadata (entry);

    std::error_code sizeError;
    std::error_code timeError;
    std::error_code modeError;
    const juce::int64 size { entryType == Paths::EntryType::file ? static_cast<juce::int64> (entry.file_size (sizeError)) : juce::int64 { 0 } };
    const juce::int64 modifiedMilliseconds { getUnixMilliseconds (entry.last_write_time (timeError)) };
    const int permissions { static_cast<int> (entry.status (modeError).permissions()) };

    if (sizeError.value() != 0 or timeError.value() != 0 or modeError.value() != 0)
        return std::nullopt;

    return Paths::Metadata { size, modifiedMilliseconds, permissions };
}

static std::optional<Paths::Metadata> getListedMetadata (const std::filesystem::directory_entry& entry, Paths::EntryType entryType, bool isListingEntries)
{
    if (isListingEntries)
        return getMetadata (entry, entryType);

    return Paths::Metadata {};
}

static void addListedEntry (std::string& displayPaths,
                            jam::Array<Paths::ListedEntry>& entries,
                            std::string_view displayPrefix,
                            std::string_view name,
                            Paths::EntryType entryType,
                            const Paths::Metadata& metadata)
{
    entries.add (Paths::ListedEntry { displayPaths.size(), displayPrefix.size() + name.size(), entryType, metadata });
    jam::Format::appendBytes (displayPaths, displayPrefix);
    jam::Format::appendBytes (displayPaths, name);
}

static bool isRecursable (std::string_view displayView, size_t displayPrefixSize, std::optional<int> maxDepth)
{
    const auto relativePath { displayView.substr (displayPrefixSize) };
    const int depth { static_cast<int> (std::count (relativePath.begin(), relativePath.end(), static_cast<char> (Chars::slash))) + 1 };
    return not maxDepth.has_value() or depth < *maxDepth;
}

static std::optional<Paths::Listing> getDirectoryListing (const std::string& fullPath,
                                                          std::string_view displayPrefix,
                                                          std::string_view gitDirectoryName,
                                                          std::string_view gitignoreName,
                                                          bool isIncludingHidden,
                                                          bool isListingEntries,
                                                          const std::atomic<bool>& shouldStop)
{
    std::error_code listingError;
    std::filesystem::directory_iterator entry { std::filesystem::u8path (fullPath), listingError };
    std::string displayPaths;
    jam::Array<Paths::ListedEntry> entries;
    bool hasGitignore { false };
    int skippedCount { 0 };

    for (; listingError.value() == 0 and entry != std::filesystem::directory_iterator {} and not shouldStop.load (std::memory_order_relaxed); entry.increment (listingError))
    {
        const auto entryType { getEntryType (*entry) };
        const auto name { getEntryName (*entry) };

        if (not name.has_value())
            return std::nullopt;

        hasGitignore = hasGitignore or (entryType == Paths::EntryType::file and name->compare (gitignoreName) == 0);

        if (isListedEntry (entryType, isListingEntries) and shouldVisitEntry (*name, gitDirectoryName, isIncludingHidden))
        {
            const auto metadata { getListedMetadata (*entry, *entryType, isListingEntries) };

            if (not metadata.has_value())
                return std::nullopt;

            addListedEntry (displayPaths, entries, displayPrefix, *name, *entryType, *metadata);
        }
        else if (isCountedEntry (entryType) and name->compare (gitDirectoryName) != 0)
        {
            ++skippedCount;
        }
    }

    if (listingError.value() != 0)
        return std::nullopt;

    return Paths::Listing { std::move (displayPaths), std::move (entries), hasGitignore, skippedCount };
}

static std::shared_ptr<const Paths::ListedFolder> getListedFolder (Paths::Folder folder, Paths::Listing listing)
{
    auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, folderRules] { folder };
    const bool isWalkRoot { folderDisplay.size() == displayPrefixSize };
    const auto& [displayPaths, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (displayPaths, entries, skippedCount);

    if (folderRules != nullptr and hasGitignore and not isWalkRoot)
        folderRules = IgnoreRules::getChildRules (folderRules,
                                                  juce::File (jam::Strings::fromView (folderFull)),
                                                  folderRel);

    return std::make_shared<Paths::ListedFolder> (std::move (folder), std::move (listing));
}

template <typename AddItem>
static void addFolderRanges (const std::shared_ptr<const Paths::ListedFolder>& listedFolder, const AddItem& addItem)
{
    const auto& [folder, listing] { *listedFolder };
    const auto& [displayPaths, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (folder, displayPaths, hasGitignore, skippedCount);
    const int numEntries { entries.size() };

    for (int entryStart { entriesPerItem }; entryStart < numEntries; entryStart += entriesPerItem)
        addItem (WalkItem { FolderRange { listedFolder, juce::Range<int> { entryStart, std::min (entryStart + entriesPerItem, numEntries) } } });
}

template <typename AddItem>
static void applyToFolderEntry (std::string_view displayView,
                               size_t displayPrefixSize,
                               std::string_view name,
                               const std::shared_ptr<const IgnoreRules>& childRules,
                               std::optional<int> maxDepth,
                               std::string& fullPath,
                               std::string& relBuffer,
                               const AddItem& addItem)
{
    const auto savedFullSize { fullPath.size() };
    appendPathSegment (fullPath, name);
    std::string childDisplay { displayView };
    jam::Format::appendUtf8 (childDisplay, Chars::slash);

    if (childRules != nullptr)
        jam::Format::appendUtf8 (relBuffer, Chars::slash);

    if (isRecursable (displayView, displayPrefixSize, maxDepth))
        addItem (WalkItem { Paths::Folder { fullPath, childDisplay, relBuffer, displayPrefixSize, childRules } });

    fullPath.resize (savedFullSize);
}

template <typename OnFile>
static void reportEntry (int workerIndex, Paths::ListedFile& listedFile, int index, const OnFile& onFile)
{
    auto& [sharedFolder, slotIndex] { listedFile };
    juce::ignoreUnused (sharedFolder);
    slotIndex = index;
    onFile (workerIndex, listedFile);
}

template <typename AddItem, typename OnFile>
static int applyToListing (int workerIndex,
                           const std::shared_ptr<const Paths::ListedFolder>& listedFolder,
                           juce::Range<int> entryRange,
                           const std::atomic<bool>& shouldStop,
                           juce::Span<const std::string_view> includeGlobs,
                           juce::Span<const std::string_view> excludeGlobs,
                           std::optional<int> maxDepth,
                           bool isListingEntries,
                           const AddItem& addItem,
                           const OnFile& onFile)
{
    const auto& [folder, listing] { *listedFolder };
    const auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, childRules] { folder };
    const auto& [displayPaths, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (hasGitignore, skippedCount);
    int ignoredCount { 0 };
    const int entryEnd { std::min (entryRange.getEnd(), entries.size()) };
    Paths::ListedFile listedFile { listedFolder, entryRange.getStart() };
    std::string fullPath { folderFull };
    std::string relBuffer { folderRel };

    for (int index { entryRange.getStart() }; index < entryEnd; ++index)
    {
        if (shouldStop.load (std::memory_order_relaxed))
            break;

        const auto& [start, length, entryType, metadata] { entries.at (index) };
        juce::ignoreUnused (metadata);
        const std::string_view displayView { std::string_view { displayPaths }.substr (start, length) };
        const std::string_view name { displayView.substr (folderDisplay.size()) };
        const auto savedRelSize { relBuffer.size() };

        if (childRules != nullptr)
            jam::Format::appendBytes (relBuffer, name);

        const bool isDirectory { entryType == Paths::EntryType::directory };
        const bool isIncluded { childRules == nullptr or not childRules->isIgnored (relBuffer, isDirectory) };

        if (not isIncluded) ++ignoredCount;

        if (isIncluded and isDirectory)
            applyToFolderEntry (displayView, displayPrefixSize, name, childRules, maxDepth, fullPath, relBuffer, addItem);

        const bool isReported { isListingEntries or entryType == Paths::EntryType::file };

        if (isIncluded and isReported and shouldIncludeFile (name, displayView, displayView.substr (displayPrefixSize), includeGlobs, excludeGlobs))
            reportEntry (workerIndex, listedFile, index, onFile);

        relBuffer.resize (savedRelSize);
    }

    return ignoredCount;
}

static std::string_view getDirectoryDisplayPath (std::string_view displayPrefix)
{
    const bool isTrailingSlash { displayPrefix.size() > 1 and displayPrefix.back() == static_cast<char> (Chars::slash) };
    return isTrailingSlash ? displayPrefix.substr (0, displayPrefix.size() - 1) : displayPrefix;
}

void Paths::addFailures (Paths::Failures& failures, Paths::Failures&& addedFailures)
{
    auto& [failurePaths, failureResults] { failures };
    const auto& [addedPaths, addedResults] { addedFailures };

    if (failureResults.isEmpty())
    {
        failures = std::move (addedFailures);
    }
    else
    {
        failurePaths.addArray (addedPaths, 0, -1);
        failureResults.insertArray (failureResults.size(), addedResults.data(), addedResults.size());
    }
}

jam::Array<juce::Result> Paths::getFailureResults (const Failures& failures)
{
    const auto& [failurePaths, failureResults] { failures };
    jam::Array<int> failureOrder;
    failureOrder.ensureStorageAllocated (failurePaths.size());

    for (int index { 0 }; index < failurePaths.size(); ++index)
        failureOrder.add (index);

    const auto getMessageOrder { [&results = failureResults] (int firstIndex, int secondIndex)
    {
        return jam::Strings::getView (results.at (firstIndex).getErrorMessage()).compare (jam::Strings::getView (results.at (secondIndex).getErrorMessage()));
    } };
    const auto getPathOrder { [&paths = failurePaths] (int firstIndex, int secondIndex)
    {
        return paths.getView (firstIndex).compare (paths.getView (secondIndex));
    } };

    std::sort (failureOrder.begin(), failureOrder.end(), [&getMessageOrder, &getPathOrder] (int firstIndex, int secondIndex)
    {
        const int messageOrder { getMessageOrder (firstIndex, secondIndex) };
        return messageOrder < 0 or (messageOrder == 0 and getPathOrder (firstIndex, secondIndex) < 0);
    });

    const auto uniqueEnd { std::unique (failureOrder.begin(), failureOrder.end(), [&getMessageOrder] (int firstIndex, int secondIndex)
    {
        return getMessageOrder (firstIndex, secondIndex) == 0;
    }) };

    std::sort (failureOrder.begin(), uniqueEnd, [&getMessageOrder, &getPathOrder] (int firstIndex, int secondIndex)
    {
        const int pathOrder { getPathOrder (firstIndex, secondIndex) };
        return pathOrder < 0 or (pathOrder == 0 and getMessageOrder (firstIndex, secondIndex) < 0);
    });

    jam::Array<juce::Result> results;

    for (auto position { failureOrder.begin() }; position != uniqueEnd; ++position)
        results.add (failureResults.at (*position));

    return results;
}

std::string Paths::getDisplayPath (std::string_view rawPath)
{
    std::string displayPath { rawPath };
#if JUCE_WINDOWS
    std::replace (displayPath.begin(),
                  displayPath.end(),
                  static_cast<char> (Chars::backslash),
                  static_cast<char> (Chars::slash));
#endif
    return displayPath;
}

std::string_view Paths::getDisplayPath (const ListedFile& listedFile)
{
    const auto& [listedFolder, slotIndex] { listedFile };
    const auto& [folder, listing] { *listedFolder };
    const auto& [displayPaths, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (folder, hasGitignore, skippedCount);
    const auto& [start, length, entryType, metadata] { entries.at (slotIndex) };
    juce::ignoreUnused (entryType, metadata);
    return std::string_view { displayPaths }.substr (start, length);
}

std::string_view Paths::getDisplayPath (const Entry& entry)
{
    return std::visit ([] (const auto& alternative) -> std::string_view
    {
        using Alternative = std::decay_t<decltype (alternative)>;
        juce::ignoreUnused (alternative);

        if constexpr (std::is_same_v<Alternative, ListedFile>)
            return getDisplayPath (alternative);

        if constexpr (std::is_same_v<Alternative, NamedFile>)
        {
            const auto& [namedPath, namedFile] { alternative };
            return namedPath;
        }

        return Id::stdinPath;
    }, entry);
}

juce::File Paths::getFile (const ListedFile& listedFile)
{
    const auto& [listedFolder, slotIndex] { listedFile };
    const auto& [folder, listing] { *listedFolder };
    const auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, folderRules] { folder };
    const auto& [displayPaths, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (folderRel, displayPrefixSize, folderRules, hasGitignore, skippedCount);
    const auto& [start, length, entryType, metadata] { entries.at (slotIndex) };
    juce::ignoreUnused (entryType, metadata);
    const std::string_view name { std::string_view { displayPaths }.substr (start + folderDisplay.size(), length - folderDisplay.size()) };
    std::string fullPath { folderFull };
    appendPathSegment (fullPath, name);
    return juce::File { jam::Strings::fromView (fullPath) };
}

juce::File Paths::getFile (const Entry& entry)
{
    return std::visit ([] (const auto& alternative) -> juce::File
    {
        using Alternative = std::decay_t<decltype (alternative)>;
        juce::ignoreUnused (alternative);

        if constexpr (std::is_same_v<Alternative, ListedFile>)
            return getFile (alternative);

        if constexpr (std::is_same_v<Alternative, NamedFile>)
        {
            const auto& [namedPath, namedFile] { alternative };
            return namedFile;
        }

        return juce::File {};
    }, entry);
}

static std::pair<juce::Result, Paths::ListedEntry> getNamedFileEntry (std::string_view displayPath, const juce::File& namedFile)
{
    const std::filesystem::directory_entry entry { std::filesystem::u8path (jam::Strings::getView (namedFile.getFullPathName())) };
    const auto metadata { getMetadata (entry, Paths::EntryType::file) };

    if (metadata.has_value())
        return { juce::Result::ok(), Paths::ListedEntry { 0, 0, Paths::EntryType::file, *metadata } };

    return { TextDocument::getReadFailure (namedFile, displayPath), Paths::ListedEntry {} };
}

std::pair<juce::Result, Paths::ListedEntry> Paths::getListedEntry (const Entry& entry)
{
    return std::visit ([] (const auto& alternative) -> std::pair<juce::Result, ListedEntry>
    {
        using Alternative = std::decay_t<decltype (alternative)>;

        if constexpr (std::is_same_v<Alternative, ListedFile>)
        {
            const auto& [listedFolder, slotIndex] { alternative };
            const auto& [folder, listing] { *listedFolder };
            const auto& [displayPaths, entries, hasGitignore, skippedCount] { listing };
            juce::ignoreUnused (folder, displayPaths, hasGitignore, skippedCount);
            return { juce::Result::ok(), entries.at (slotIndex) };
        }

        if constexpr (std::is_same_v<Alternative, NamedFile>)
        {
            const auto& [namedPath, namedFile] { alternative };
            return getNamedFileEntry (namedPath, namedFile);
        }

        return { juce::Result::ok(), ListedEntry { 0, 0, EntryType::file, Metadata {} } };
    }, entry);
}

bool Paths::isExplicit (const Entry& entry)
{
    return not std::holds_alternative<ListedFile> (entry);
}

juce::File Paths::getLinkedTarget (std::string_view rawPath)
{
    std::error_code targetError;
    const auto target { std::filesystem::canonical (std::filesystem::u8path (rawPath), targetError) };

    if (targetError.value() == 0)
        return juce::File { jam::Strings::fromView (target.u8string()) };

    return juce::File::getCurrentWorkingDirectory().getChildFile (jam::Strings::fromView (rawPath));
}

void Paths::applyToFileNames (const juce::File& directory, const std::function<void (std::string_view)>& function)
{
    const std::atomic<bool> shouldStop { false };
    std::string fullPath { jam::Strings::getView (directory.getFullPathName()) };

    if (const auto listing { getDirectoryListing (fullPath, {}, {}, {}, false, false, shouldStop) })
    {
        const auto& [displayPaths, entries, hasGitignore, skippedCount] { *listing };
        juce::ignoreUnused (hasGitignore, skippedCount);

        for (const auto& [start, length, entryType, metadata] : entries)
        {
            if (entryType == EntryType::file)
                function (std::string_view { displayPaths }.substr (start, length));
        }
    }
}

static Paths::Folder getRootFolder (std::string_view rawPathView, const juce::File& rootDirectory, bool isIgnoringRules)
{
    std::shared_ptr<const IgnoreRules> rules;
    std::string relBuffer;

    if (not isIgnoringRules)
    {
        rules = IgnoreRules::create (rootDirectory);
        relBuffer = rules->getRootRelativePrefix (rootDirectory);
    }

    auto displayBuffer { getNormalizedDisplayPrefix (rawPathView) };
    const auto displayPrefixSize { displayBuffer.size() };
    return { std::string { jam::Strings::getView (rootDirectory.getFullPathName()) }, std::move (displayBuffer), std::move (relBuffer), displayPrefixSize, std::move (rules) };
}

std::pair<Paths::Failures, int> Paths::applyToDirectories (const jam::Array<std::pair<std::string_view, juce::File>>& roots,
                                                           juce::Span<const std::string_view> includeGlobs,
                                                           juce::Span<const std::string_view> excludeGlobs,
                                                           bool isIncludingHidden,
                                                           bool isIgnoringRules,
                                                           const std::function<void (int)>& onWorkerCount,
                                                           const std::function<void (int, const ListedFile&)>& onFile,
                                                           const std::atomic<bool>& shouldStop,
                                                           std::optional<int> maxDepth,
                                                           bool isListingEntries)
{
    const std::string_view gitDirectoryName { Id::gitDirectory };
    const std::string_view gitignoreName { Id::gitignoreFile };
    const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };

    const int workerCount { juce::SystemStats::getNumCpus() };
    onWorkerCount (workerCount);
    jam::Array<Failures> workerFailures;
    workerFailures.resize (workerCount);
    jam::Array<int> workerSkippedCounts;
    workerSkippedCounts.resize (workerCount);

    const auto walkFolder { [&includeGlobs, &excludeGlobs, gitDirectoryName, gitignoreName, isIncludingHidden, &workingDirectory, &workerFailures, &workerSkippedCounts, &onFile, &shouldStop, maxDepth, isListingEntries]
                            (int workerIndex, WalkItem& walkItem, auto& addItem)
    {
        if (auto* folderRange { std::get_if<FolderRange> (&walkItem) })
        {
            const auto& [listedFolder, entryRange] { *folderRange };
            workerSkippedCounts.at (workerIndex) += applyToListing (workerIndex, listedFolder, entryRange, shouldStop, includeGlobs, excludeGlobs, maxDepth, isListingEntries, addItem, onFile);
        }
        else
        {
            auto& folder { std::get<Paths::Folder> (walkItem) };

            if (not shouldStop.load (std::memory_order_relaxed))
            {
                const auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, folderRules] { folder };
                juce::ignoreUnused (folderRel, displayPrefixSize, folderRules);
                auto listing { getDirectoryListing (folderFull, folderDisplay, gitDirectoryName, gitignoreName, isIncludingHidden, isListingEntries, shouldStop) };

                if (listing.has_value())
                {
                    const auto& [listedDisplayPaths, listedEntries, listedHasGitignore, listedSkippedCount] { *listing };
                    juce::ignoreUnused (listedDisplayPaths, listedEntries, listedHasGitignore);
                    workerSkippedCounts.at (workerIndex) += listedSkippedCount;
                    const auto listedFolder { getListedFolder (std::move (folder), std::move (*listing)) };
                    addFolderRanges (listedFolder, addItem);
                    workerSkippedCounts.at (workerIndex) += applyToListing (workerIndex, listedFolder, juce::Range<int> { 0, entriesPerItem }, shouldStop, includeGlobs, excludeGlobs, maxDepth, isListingEntries, addItem, onFile);
                }
                else
                {
                    const auto unreadableDisplayPath { getDirectoryDisplayPath (folderDisplay) };
                    auto& [workerPaths, workerResults] { workerFailures.at (workerIndex) };
                    workerPaths.add (unreadableDisplayPath);
                    workerResults.add (TextDocument::getReadFailure (workingDirectory.getChildFile (jam::Strings::fromView (unreadableDisplayPath)), unreadableDisplayPath));
                }
            }
        }
    } };

    jam::Array<WalkItem> rootItems;

    for (const auto& [rawPath, rootDirectory] : roots)
        rootItems.add (WalkItem { getRootFolder (rawPath, rootDirectory, isIgnoringRules) });

    Jobs::runRecursive (workerCount, std::move (rootItems), walkFolder);

    Failures failures;

    for (auto& worker : workerFailures)
        addFailures (failures, std::move (worker));

    int skippedCount { 0 };

    for (const auto workerSkippedCount : workerSkippedCounts)
        skippedCount += workerSkippedCount;

    return { std::move (failures), skippedCount };
}

std::tuple<Paths::Failures, jam::Array<Paths::Entry>, jam::Array<std::pair<std::string_view, juce::File>>> Paths::getClassifiedPaths (juce::Span<const std::string_view> rawPaths)
{
    Failures failures;
    auto& [failurePaths, failureResults] { failures };
    jam::Array<Entry> files;
    jam::Array<std::pair<std::string_view, juce::File>> folders;

    for (const auto& rawPathView : rawPaths)
    {
        if (rawPathView.compare (Id::stdinArgument) == 0)
        {
            if (std::none_of (files.begin(), files.end(), [] (const Entry& entry)
                {
                    return std::holds_alternative<Input::StandardInput> (entry);
                }))
                files.add (Entry { Input::StandardInput {} });
        }
        else
        {
            auto targetFile { Paths::getLinkedTarget (rawPathView) };

            if (not targetFile.exists())
            {
                failurePaths.add (rawPathView);
                failureResults.add (TextDocument::getReadFailure (targetFile, rawPathView));
            }
            else if (targetFile.isDirectory())
            {
                folders.add (std::pair<std::string_view, juce::File> { rawPathView, std::move (targetFile) });
            }
            else
            {
                files.add (Entry { NamedFile { Paths::getDisplayPath (rawPathView), std::move (targetFile) } });
            }
        }
    }

    return { std::move (failures), std::move (files), std::move (folders) };
}

std::tuple<Paths::Failures, jam::Array<Paths::Entry>, int> Paths::getFiles (juce::Span<const std::string_view> rawPaths,
                                                                            juce::Span<const std::string_view> includeGlobs,
                                                                            juce::Span<const std::string_view> excludeGlobs,
                                                                            bool isIncludingHidden,
                                                                            bool isIgnoringRules,
                                                                            std::optional<int> maxDepth,
                                                                            bool isListingEntries)
{
    auto [failures, entries, roots] { getClassifiedPaths (rawPaths) };
    int skippedCount { 0 };

    if (not roots.isEmpty())
    {
        jam::Array<jam::Array<Entry>> workerEntries;
        const std::atomic<bool> shouldStop { false };
        auto [walkFailures, walkSkippedCount] { applyToDirectories (roots,
                                               includeGlobs,
                                               excludeGlobs,
                                               isIncludingHidden,
                                               isIgnoringRules,
                                               [&workerEntries] (int workerCount)
                                               {
                                                   workerEntries.resize (workerCount);
                                               },
                                               [&workerEntries] (int workerIndex, const ListedFile& listedFile)
                                               {
                                                   workerEntries.at (workerIndex).add (Entry { listedFile });
                                               },
                                               shouldStop,
                                               maxDepth, isListingEntries) };

        int totalCount { 0 };

        for (const auto& worker : workerEntries)
            totalCount += worker.size();

        entries.ensureStorageAllocated (entries.size() + totalCount);

        for (auto& worker : workerEntries)
        {
            for (auto& entry : worker)
                entries.add (std::move (entry));
        }

        addFailures (failures, std::move (walkFailures));
        skippedCount = walkSkippedCount;
    }

    std::sort (entries.begin(), entries.end(), [] (const Entry& firstEntry, const Entry& secondEntry)
    {
        const int order { getDisplayPath (firstEntry).compare (getDisplayPath (secondEntry)) };
        return order < 0 or (order == 0 and isExplicit (firstEntry) and not isExplicit (secondEntry));
    });

    if (rawPaths.size() > 1)
    {
        const auto uniqueEnd { std::unique (entries.begin(), entries.end(), [] (const Entry& firstEntry, const Entry& secondEntry)
        {
            return getDisplayPath (firstEntry).compare (getDisplayPath (secondEntry)) == 0;
        }) };
        const int uniqueCount { static_cast<int> (std::distance (entries.begin(), uniqueEnd)) };
        entries.removeRange (uniqueCount, entries.size() - uniqueCount);
    }

    return { std::move (failures), std::move (entries), skippedCount };
}

std::pair<juce::Result, jam::Array<Paths::Entry>> Paths::getReadableFiles (juce::Span<const std::string_view> rawPaths,
                                                                           juce::Span<const std::string_view> includeGlobs,
                                                                           juce::Span<const std::string_view> excludeGlobs,
                                                                           bool isIncludingHidden,
                                                                           bool isIgnoringRules)
{
    auto [pathFailures, entries, skippedCount] { getFiles (rawPaths, includeGlobs, excludeGlobs, isIncludingHidden, isIgnoringRules) };
    juce::ignoreUnused (skippedCount);
    const auto& [failurePaths, failureResults] { pathFailures };

    if (not failureResults.isEmpty())
    {
        int firstIndex { 0 };

        for (int index { 1 }; index < failurePaths.size(); ++index)
        {
            if (failurePaths.getView (index) < failurePaths.getView (firstIndex))
                firstIndex = index;
        }

        return { failureResults.at (firstIndex), jam::Array<Entry> {} };
    }

    return { juce::Result::ok(), std::move (entries) };
}

static juce::File getCarriedCacheFolder()
{
#if JUCE_WINDOWS
    return juce::File { juce::SystemStats::getEnvironmentVariable ("LOCALAPPDATA", {}) }.getChildFile (jam::Strings::fromView (files::lingLocalCache));
#else
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (jam::Strings::fromView (files::lingCache));
#endif
}

static juce::File getCarriedFileInCache (const juce::File& cacheFolder, std::string_view libraryName, std::string_view fileName)
{
    const auto languageFolders (cacheFolder.getChildFile (jam::Strings::fromView (files::language)).findChildFiles (juce::File::findDirectories, false));

    for (const auto& languageFolder : languageFolders)
    {
        const auto carriedFile { languageFolder.getChildFile (jam::Strings::fromView (libraryName)).getChildFile (jam::Strings::fromView (fileName)) };

        if (carriedFile.existsAsFile())
            return carriedFile;
    }

    return juce::File {};
}

juce::File Paths::getCarriedFile (std::string_view libraryName, std::string_view fileName)
{
    const auto configuredFile { juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                                    .getChildFile (jam::Strings::fromView (files::lingConfig))
                                    .getChildFile (jam::Strings::fromView (files::language))
                                    .getChildFile (jam::Strings::fromView (libraryName))
                                    .getChildFile (jam::Strings::fromView (files::libraryBuild))
                                    .getChildFile (jam::Strings::fromView (fileName)) };

    if (configuredFile.existsAsFile())
        return configuredFile;

    return getCarriedFileInCache (getCarriedCacheFolder(), libraryName, fileName);
}
