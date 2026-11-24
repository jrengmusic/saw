#include "Paths.h"
#include "IgnoreRules.h"
#include "Jobs.h"
#include "TextDocument.h"
#include "TransparentStringHash.h"
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
        normalized.push_back (static_cast<char> (Chars::slash));

    return normalized;
}

static bool matchesWildcard (std::string_view wildcard, std::string_view text) noexcept
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
                            const jam::Strings& globs)
{
    for (int index { 0 }; index < globs.size(); ++index)
    {
        const auto glob { globs.getView (index) };

        if (matchesWildcard (glob, fileName) or matchesWildcard (glob, displayPath) or matchesWildcard (glob, relativePath))
            return true;
    }

    return false;
}

static bool shouldIncludeFile (std::string_view fileName,
                               std::string_view displayPath,
                               std::string_view relativePath,
                               const jam::Strings& includeGlobs,
                               const jam::Strings& excludeGlobs)
{
    if (includeGlobs.size() > 0 and not matchesAnyGlob (fileName, displayPath, relativePath, includeGlobs))
        return false;

    if (excludeGlobs.size() > 0 and matchesAnyGlob (fileName, displayPath, relativePath, excludeGlobs))
        return false;

    return true;
}

static void appendPathSegment (std::string& path, std::string_view segment)
{
    if (path.empty() or path.back() != static_cast<char> (Chars::slash))
        path.push_back (static_cast<char> (Chars::slash));

    path.append (segment);
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

static constexpr int filesPerItem { 64 };

using Folder = std::tuple<std::string, std::string, std::string, size_t, std::shared_ptr<const IgnoreRules>, std::vector<std::string>>;

template <typename OnFile>
static void applyToFileNames (int workerIndex,
                              std::string& fullPath,
                              std::string& displayBuffer,
                              const std::vector<std::string>& fileNames,
                              const std::atomic<bool>& shouldStop,
                              const OnFile& onFile)
{
    for (const auto& fileName : fileNames)
    {
        if (shouldStop.load (std::memory_order_relaxed))
            break;

        const auto savedFullSize { fullPath.size() };
        const auto savedDisplaySize { displayBuffer.size() };
        appendPathSegment (fullPath, fileName);
        displayBuffer.append (fileName);
        onFile (workerIndex, fullPath, displayBuffer);
        fullPath.resize (savedFullSize);
        displayBuffer.resize (savedDisplaySize);
    }
}

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

static std::optional<std::tuple<std::string, std::vector<std::tuple<size_t, size_t, bool>>, bool>> getDirectoryListing (const std::string& fullPath,
                                                                                                                        std::string_view gitDirectoryName,
                                                                                                                        std::string_view gitignoreName,
                                                                                                                        bool isIncludingHidden,
                                                                                                                        const std::atomic<bool>& shouldStop)
{
    std::error_code listingError;
    std::filesystem::directory_iterator entry { std::filesystem::u8path (fullPath), listingError };
    std::string names;
    std::vector<std::tuple<size_t, size_t, bool>> entries;
    bool hasGitignore { false };

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
            entries.emplace_back (names.size(), name->size(), isDirectory);
            names.append (*name);
        }
    }

    if (listingError.value() != 0)
        return std::nullopt;

    return std::tuple<std::string, std::vector<std::tuple<size_t, size_t, bool>>, bool> { std::move (names), std::move (entries), hasGitignore };
}

template <typename AddItem, typename OnFile>
static void applyToListing (int workerIndex,
                            const std::string& names,
                            const std::vector<std::tuple<size_t, size_t, bool>>& entries,
                            const std::shared_ptr<const IgnoreRules>& childRules,
                            std::string& fullPath,
                            std::string& displayBuffer,
                            std::string& relBuffer,
                            size_t displayPrefixSize,
                            const jam::Strings& includeGlobs,
                            const jam::Strings& excludeGlobs,
                            const std::atomic<bool>& shouldStop,
                            const AddItem& addItem,
                            const OnFile& onFile)
{
    std::vector<std::string> pendingNames;

    for (const auto& [start, length, isDirectory] : entries)
    {
        if (shouldStop.load (std::memory_order_relaxed))
            break;

        const std::string_view name { std::string_view { names }.substr (start, length) };
        const auto savedDisplaySize { displayBuffer.size() };
        const auto savedRelSize { relBuffer.size() };
        displayBuffer.append (name);

        if (childRules != nullptr)
            relBuffer.append (name);

        const bool isIncluded { childRules == nullptr or not childRules->isIgnored (relBuffer, isDirectory) };

        if (isIncluded and isDirectory)
        {
            const auto savedFullSize { fullPath.size() };
            appendPathSegment (fullPath, name);
            displayBuffer.push_back (static_cast<char> (Chars::slash));

            if (childRules != nullptr)
                relBuffer.push_back (static_cast<char> (Chars::slash));

            addItem (Folder { fullPath, displayBuffer, relBuffer, displayPrefixSize, childRules, {} });
            fullPath.resize (savedFullSize);
        }
        else if (isIncluded and shouldIncludeFile (name, displayBuffer, std::string_view { displayBuffer }.substr (displayPrefixSize), includeGlobs, excludeGlobs))
        {
            pendingNames.emplace_back (name);
        }

        displayBuffer.resize (savedDisplaySize);
        relBuffer.resize (savedRelSize);

        if (pendingNames.size() >= static_cast<size_t> (filesPerItem))
        {
            addItem (Folder { fullPath, displayBuffer, relBuffer, displayPrefixSize, childRules, std::move (pendingNames) });
            pendingNames.clear();
        }
    }

    applyToFileNames (workerIndex, fullPath, displayBuffer, pendingNames, shouldStop, onFile);
}

static jam::Array<bool> getExplicitFlags (const std::vector<std::pair<std::string, juce::File>>& collectedFiles,
                                          const jam::HashSet<std::string, TransparentStringHash, std::equal_to<>>& explicitSet)
{
    jam::Array<bool> flags;
    flags.ensureStorageAllocated (static_cast<int> (collectedFiles.size()));

    if (explicitSet.empty())
    {
        flags.insertMultiple (0, false, static_cast<int> (collectedFiles.size()));
    }
    else
    {
        for (const auto& [path, file] : collectedFiles)
        {
            juce::ignoreUnused (file);
            flags.add (explicitSet.contains (path));
        }
    }

    return flags;
}

static std::string_view getDirectoryDisplayPath (std::string_view displayPrefix)
{
    const bool isTrailingSlash { displayPrefix.size() > 1 and displayPrefix.back() == static_cast<char> (Chars::slash) };
    return isTrailingSlash ? displayPrefix.substr (0, displayPrefix.size() - 1) : displayPrefix;
}

std::string Paths::getDisplayPath (std::string_view rawPath)
{
    std::string displayPath { rawPath };
#ifdef _WIN32
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

jam::Strings Paths::getFileNames (const juce::File& directory)
{
    const std::atomic<bool> shouldStop { false };
    auto fullPath { directory.getFullPathName().toStdString() };
    jam::Strings fileNames;

    if (const auto listing { getDirectoryListing (fullPath, {}, {}, false, shouldStop) })
    {
        const auto& [names, entries, hasGitignore] { *listing };
        juce::ignoreUnused (hasGitignore);

        for (const auto& [start, length, isDirectory] : entries)
        {
            if (not isDirectory)
                fileNames.add (std::string_view { names }.substr (start, length));
        }
    }

    return fileNames;
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
    return { rootDirectory.getFullPathName().toStdString(), std::move (displayBuffer), std::move (relBuffer), displayPrefixSize, std::move (rules), std::vector<std::string> {} };
}

void Paths::applyToDirectories (const std::vector<std::pair<std::string, juce::File>>& roots,
                                const jam::Strings& includeGlobs,
                                const jam::Strings& excludeGlobs,
                                bool isIncludingHidden,
                                bool isIgnoringRules,
                                const std::function<void (int)>& onWorkerCount,
                                const std::function<void (int, const std::string&, std::string_view)>& onFile,
                                const std::function<void (int, std::string_view)>& onUnreadable,
                                const std::atomic<bool>& shouldStop)
{
    const std::string_view gitDirectoryName { Id::gitDirectory };
    const std::string_view gitignoreName { Id::gitignoreFile };

    const int workerCount { juce::SystemStats::getNumCpus() };
    onWorkerCount (workerCount);

    const auto walkFolder = [&includeGlobs, &excludeGlobs, gitDirectoryName, gitignoreName, isIncludingHidden, isIgnoringRules, &roots, &onFile, &onUnreadable, &shouldStop]
                            (int workerIndex, Folder& folder, auto& addItem)
    {
        auto& [folderFull, folderDisplay, folderRel, displayPrefixSize, folderRules, fileNames] { folder };

        if (folderFull.empty())
        {
            for (const auto& [rawPath, rootDirectory] : roots)
                addItem (getRootFolder (rawPath, rootDirectory, isIgnoringRules));
        }
        else if (not fileNames.empty())
        {
            applyToFileNames (workerIndex, folderFull, folderDisplay, fileNames, shouldStop, onFile);
        }
        else if (not shouldStop.load (std::memory_order_relaxed))
        {
            const auto listing { getDirectoryListing (folderFull, gitDirectoryName, gitignoreName, isIncludingHidden, shouldStop) };

            if (listing.has_value())
            {
                const auto& [names, entries, hasGitignore] { *listing };
                const bool isWalkRoot { folderDisplay.size() == displayPrefixSize };
                auto childRules { folderRules };

                if (folderRules != nullptr and hasGitignore and not isWalkRoot)
                    childRules = IgnoreRules::getChildRules (folderRules,
                                                             juce::File (juce::String::fromUTF8 (folderFull.data(), static_cast<int> (folderFull.size()))),
                                                             folderRel);

                applyToListing (workerIndex, names, entries, childRules, folderFull, folderDisplay, folderRel, displayPrefixSize, includeGlobs, excludeGlobs, shouldStop, addItem, onFile);
            }
            else
            {
                onUnreadable (workerIndex, getDirectoryDisplayPath (folderDisplay));
            }
        }
    };

    Jobs::runRecursive (workerCount, Folder {}, walkFolder);
}

std::tuple<std::vector<std::pair<std::string, juce::Result>>, std::vector<std::pair<std::string, juce::File>>, jam::Array<bool>> Paths::getFiles (const jam::Strings& rawPaths,
                                                                                                                                                  const jam::Strings& includeGlobs,
                                                                                                                                                  const jam::Strings& excludeGlobs,
                                                                                                                                                  bool isIncludingHidden,
                                                                                                                                                  bool isIgnoringRules)
{
    std::vector<std::pair<std::string, juce::Result>> failures;
    std::vector<std::pair<std::string, juce::File>> collectedFiles;
    std::vector<std::pair<std::string, juce::File>> roots;
    jam::HashSet<std::string, TransparentStringHash, std::equal_to<>> explicitSet;

    for (int index { 0 }; index < rawPaths.size(); ++index)
    {
        const auto rawPathView { rawPaths.getView (index) };
        auto targetFile { Paths::getLinkedTarget (rawPathView) };

        if (not targetFile.exists())
        {
            failures.emplace_back (std::string { rawPathView }, TextDocument::getReadFailure (targetFile, jam::Strings::fromView (rawPathView)));
        }
        else if (targetFile.isDirectory())
        {
            roots.emplace_back (std::string { rawPathView }, std::move (targetFile));
        }
        else
        {
            std::string normalized { Paths::getDisplayPath (rawPathView) };
            explicitSet.emplace (normalized);
            collectedFiles.emplace_back (std::move (normalized), std::move (targetFile));
        }
    }

    if (not roots.empty())
    {
        std::vector<std::vector<std::pair<std::string, juce::File>>> workerFiles;
        std::vector<std::vector<std::pair<std::string, juce::Result>>> workerFailures;
        const std::atomic<bool> shouldStop { false };
        const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };
        applyToDirectories (roots,
                            includeGlobs,
                            excludeGlobs,
                            isIncludingHidden,
                            isIgnoringRules,
                            [&workerFiles, &workerFailures] (int workerCount)
                            {
                                workerFiles.resize (static_cast<size_t> (workerCount));
                                workerFailures.resize (static_cast<size_t> (workerCount));
                            },
                            [&workerFiles] (int workerIndex, const std::string& fileFullPath, std::string_view fileDisplayPath)
                            {
                                workerFiles.at (static_cast<size_t> (workerIndex)).emplace_back (std::string { fileDisplayPath }, juce::File { jam::Strings::fromView (fileFullPath) });
                            },
                            [&workerFailures, &workingDirectory] (int workerIndex, std::string_view unreadableDisplayPath)
                            {
                                const auto displayString { jam::Strings::fromView (unreadableDisplayPath) };
                                workerFailures.at (static_cast<size_t> (workerIndex))
                                    .emplace_back (std::string { unreadableDisplayPath },
                                                   TextDocument::getReadFailure (workingDirectory.getChildFile (displayString), displayString));
                            },
                            shouldStop);

        size_t totalCount { 0 };

        for (const auto& worker : workerFiles)
            totalCount += worker.size();

        collectedFiles.reserve (collectedFiles.size() + totalCount);

        for (auto& worker : workerFiles)
        {
            for (auto& path : worker)
                collectedFiles.emplace_back (std::move (path));
        }

        for (auto& worker : workerFailures)
        {
            for (auto& failure : worker)
                failures.push_back (std::move (failure));
        }
    }

    std::sort (failures.begin(), failures.end(), &Paths::isPathEarlier<juce::Result>);
    std::sort (collectedFiles.begin(), collectedFiles.end(), &Paths::isPathEarlier<juce::File>);

    if (rawPaths.size() > 1)
        collectedFiles.erase (std::unique (collectedFiles.begin(),
                                           collectedFiles.end(),
                                           [] (const auto& firstEntry, const auto& secondEntry)
                                           {
                                               return not Paths::isPathEarlier (firstEntry, secondEntry) and not Paths::isPathEarlier (secondEntry, firstEntry);
                                           }),
                              collectedFiles.end());

    auto isExplicitFlags (getExplicitFlags (collectedFiles, explicitSet));
    return { std::move (failures), std::move (collectedFiles), std::move (isExplicitFlags) };
}

std::tuple<juce::Result, std::vector<std::pair<std::string, juce::File>>, jam::Array<bool>> Paths::getReadableFiles (const jam::Strings& rawPaths,
                                                                                                                     const jam::Strings& includeGlobs,
                                                                                                                     const jam::Strings& excludeGlobs,
                                                                                                                     bool isIncludingHidden,
                                                                                                                     bool isIgnoringRules)
{
    auto [pathFailures, files, isExplicitFlags] { getFiles (rawPaths, includeGlobs, excludeGlobs, isIncludingHidden, isIgnoringRules) };

    if (not pathFailures.empty())
    {
        const auto& [failurePath, failureResult] { pathFailures.front() };
        juce::ignoreUnused (failurePath);
        return { failureResult, std::vector<std::pair<std::string, juce::File>> {}, jam::Array<bool> {} };
    }

    return { juce::Result::ok(), std::move (files), std::move (isExplicitFlags) };
}
