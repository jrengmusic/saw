#include "Paths.h"
#include "IgnoreRules.h"
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

using Folder = std::tuple<std::string, std::string, std::string, size_t, std::shared_ptr<const IgnoreRules>>;
using Listing = std::tuple<std::string, jam::Array<std::tuple<size_t, size_t, bool>>, bool, int>;
using ListedFolder = std::pair<Folder, Listing>;
using FolderRange = std::pair<std::shared_ptr<const ListedFolder>, juce::Range<int>>;
using WalkItem = std::variant<Folder, FolderRange>;

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

static std::optional<Listing> getDirectoryListing (const std::string& fullPath,
                                                   std::string_view gitDirectoryName,
                                                   std::string_view gitignoreName,
                                                   bool isIncludingHidden,
                                                   const std::atomic<bool>& shouldStop)
{
    std::error_code listingError;
    std::filesystem::directory_iterator entry { std::filesystem::u8path (fullPath), listingError };
    std::string names;
    jam::Array<std::tuple<size_t, size_t, bool>> entries;
    bool hasGitignore { false };
    int skippedCount { 0 };

    for (; listingError.value() == 0 and entry != std::filesystem::directory_iterator {} and not shouldStop.load (std::memory_order_relaxed); entry.increment (listingError))
    {
        std::error_code entryError;
        const bool isSymbolicLink { entry->is_symlink (entryError) };
        const bool isDirectory { not isSymbolicLink and entry->is_directory (entryError) and std::filesystem::is_directory (entry->symlink_status (entryError)) };
        const bool isRegularFile { not isSymbolicLink and entry->is_regular_file (entryError) };
        const auto name { getEntryName (*entry) };

        if (not name.has_value())
            return std::nullopt;

        hasGitignore = hasGitignore or (isRegularFile and name->compare (gitignoreName) == 0);

        if ((isDirectory or isRegularFile) and shouldVisitEntry (*name, gitDirectoryName, isIncludingHidden))
        {
            entries.add (std::tuple<size_t, size_t, bool> { names.size(), name->size(), isDirectory });
            jam::Format::appendBytes (names, *name);
        }
        else if ((isDirectory or isRegularFile) and name->compare (gitDirectoryName) != 0)
        {
            ++skippedCount;
        }
    }

    if (listingError.value() != 0)
        return std::nullopt;

    return Listing { std::move (names), std::move (entries), hasGitignore, skippedCount };
}

static std::shared_ptr<const ListedFolder> getListedFolder (Folder folder, Listing listing)
{
    auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, folderRules] { folder };
    const bool isWalkRoot { folderDisplay.size() == displayPrefixSize };
    const auto& [names, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (names, entries, skippedCount);

    if (folderRules != nullptr and hasGitignore and not isWalkRoot)
        folderRules = IgnoreRules::getChildRules (folderRules,
                                                  juce::File (jam::Strings::fromView (folderFull)),
                                                  folderRel);

    return std::make_shared<ListedFolder> (std::move (folder), std::move (listing));
}

template <typename AddItem>
static void addFolderRanges (const std::shared_ptr<const ListedFolder>& listedFolder, const AddItem& addItem)
{
    const auto& [folder, listing] { *listedFolder };
    const auto& [names, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (folder, names, hasGitignore, skippedCount);
    const int numEntries { entries.size() };

    for (int entryStart { entriesPerItem }; entryStart < numEntries; entryStart += entriesPerItem)
        addItem (WalkItem { FolderRange { listedFolder, juce::Range<int> { entryStart, std::min (entryStart + entriesPerItem, numEntries) } } });
}

template <typename AddItem, typename OnFile>
static int applyToListing (int workerIndex,
                           const ListedFolder& listedFolder,
                           juce::Range<int> entryRange,
                           const std::atomic<bool>& shouldStop,
                           juce::Span<const std::string_view> includeGlobs,
                           juce::Span<const std::string_view> excludeGlobs,
                           const AddItem& addItem,
                           const OnFile& onFile)
{
    const auto& [folder, listing] { listedFolder };
    const auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, childRules] { folder };
    const auto& [names, entries, hasGitignore, skippedCount] { listing };
    juce::ignoreUnused (hasGitignore, skippedCount);
    int ignoredCount { 0 };
    const int entryEnd { std::min (entryRange.getEnd(), entries.size()) };
    std::string fullPath { folderFull };
    std::string displayBuffer { folderDisplay };
    std::string relBuffer { folderRel };

    for (int index { entryRange.getStart() }; index < entryEnd; ++index)
    {
        if (shouldStop.load (std::memory_order_relaxed))
            break;

        const auto& [start, length, isDirectory] { entries.at (index) };
        const std::string_view name { std::string_view { names }.substr (start, length) };
        const auto savedDisplaySize { displayBuffer.size() };
        const auto savedRelSize { relBuffer.size() };
        jam::Format::appendBytes (displayBuffer, name);

        if (childRules != nullptr)
            jam::Format::appendBytes (relBuffer, name);

        const bool isIncluded { childRules == nullptr or not childRules->isIgnored (relBuffer, isDirectory) };

        if (not isIncluded) ++ignoredCount;

        if (isIncluded and isDirectory)
        {
            const auto savedFullSize { fullPath.size() };
            appendPathSegment (fullPath, name);
            jam::Format::appendUtf8 (displayBuffer, Chars::slash);

            if (childRules != nullptr)
                jam::Format::appendUtf8 (relBuffer, Chars::slash);

            addItem (WalkItem { Folder { fullPath, displayBuffer, relBuffer, displayPrefixSize, childRules } });
            fullPath.resize (savedFullSize);
        }
        else if (isIncluded and shouldIncludeFile (name, displayBuffer, std::string_view { displayBuffer }.substr (displayPrefixSize), includeGlobs, excludeGlobs))
        {
            const auto savedFullSize { fullPath.size() };
            appendPathSegment (fullPath, name);
            onFile (workerIndex, fullPath, displayBuffer);
            fullPath.resize (savedFullSize);
        }

        displayBuffer.resize (savedDisplaySize);
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

    if (const auto listing { getDirectoryListing (fullPath, {}, {}, false, shouldStop) })
    {
        const auto& [names, entries, hasGitignore, skippedCount] { *listing };
        juce::ignoreUnused (hasGitignore, skippedCount);

        for (const auto& [start, length, isDirectory] : entries)
        {
            if (not isDirectory)
                function (std::string_view { names }.substr (start, length));
        }
    }
}

static Folder getRootFolder (std::string_view rawPathView, const juce::File& rootDirectory, bool isIgnoringRules)
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
                                                           const std::function<void (int, const std::string&, std::string_view)>& onFile,
                                                           const std::atomic<bool>& shouldStop)
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

    const auto walkFolder { [&includeGlobs, &excludeGlobs, gitDirectoryName, gitignoreName, isIncludingHidden, &workingDirectory, &workerFailures, &workerSkippedCounts, &onFile, &shouldStop]
                            (int workerIndex, WalkItem& walkItem, auto& addItem)
    {
        if (auto* folderRange { std::get_if<FolderRange> (&walkItem) })
        {
            const auto& [listedFolder, entryRange] { *folderRange };
            workerSkippedCounts.at (workerIndex) += applyToListing (workerIndex, *listedFolder, entryRange, shouldStop, includeGlobs, excludeGlobs, addItem, onFile);
        }
        else
        {
            auto& folder { std::get<Folder> (walkItem) };

            if (not shouldStop.load (std::memory_order_relaxed))
            {
                const auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, folderRules] { folder };
                juce::ignoreUnused (folderRel, displayPrefixSize, folderRules);
                auto listing { getDirectoryListing (folderFull, gitDirectoryName, gitignoreName, isIncludingHidden, shouldStop) };

                if (listing.has_value())
                {
                    const auto& [listedNames, listedEntries, listedHasGitignore, listedSkippedCount] { *listing };
                    juce::ignoreUnused (listedNames, listedEntries, listedHasGitignore);
                    workerSkippedCounts.at (workerIndex) += listedSkippedCount;
                    const auto listedFolder { getListedFolder (std::move (folder), std::move (*listing)) };
                    addFolderRanges (listedFolder, addItem);
                    workerSkippedCounts.at (workerIndex) += applyToListing (workerIndex, *listedFolder, juce::Range<int> { 0, entriesPerItem }, shouldStop, includeGlobs, excludeGlobs, addItem, onFile);
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
                    const auto& [displayPath, file, isExplicit] { entry };
                    juce::ignoreUnused (displayPath, isExplicit);
                    return file == juce::File {};
                }))
                files.add (Entry { std::string { Id::stdinPath }, juce::File {}, true });
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
                files.add (Entry { Paths::getDisplayPath (rawPathView), std::move (targetFile), true });
            }
        }
    }

    return { std::move (failures), std::move (files), std::move (folders) };
}

std::tuple<Paths::Failures, jam::Array<Paths::Entry>, int> Paths::getFiles (juce::Span<const std::string_view> rawPaths,
                                                                            juce::Span<const std::string_view> includeGlobs,
                                                                            juce::Span<const std::string_view> excludeGlobs,
                                                                            bool isIncludingHidden,
                                                                            bool isIgnoringRules)
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
                                               [&workerEntries] (int workerIndex, const std::string& fileFullPath, std::string_view fileDisplayPath)
                                               {
                                                   workerEntries.at (workerIndex).add (Entry { std::string { fileDisplayPath }, juce::File { jam::Strings::fromView (fileFullPath) }, false });
                                               },
                                               shouldStop) };

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
        const auto& [firstPath, firstFile, isFirstExplicit] { firstEntry };
        const auto& [secondPath, secondFile, isSecondExplicit] { secondEntry };
        juce::ignoreUnused (firstFile, secondFile);
        const int order { firstPath.compare (secondPath) };
        return order < 0 or (order == 0 and isFirstExplicit and not isSecondExplicit);
    });

    if (rawPaths.size() > 1)
    {
        const auto uniqueEnd { std::unique (entries.begin(), entries.end(), [] (const Entry& firstEntry, const Entry& secondEntry)
        {
            const auto& [firstPath, firstFile, isFirstExplicit] { firstEntry };
            const auto& [secondPath, secondFile, isSecondExplicit] { secondEntry };
            juce::ignoreUnused (firstFile, isFirstExplicit, secondFile, isSecondExplicit);
            return firstPath.compare (secondPath) == 0;
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
