#include "Paths.h"
#include "IgnoreRules.h"
#include "Jobs.h"
#include "generated/Generated.h"

#ifndef _WIN32
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct TransparentStringHash
{
    using is_transparent = void;
    using is_avalanching = void;

    std::uint64_t operator() (std::string_view key) const noexcept
    {
        return jam::Hash<std::string_view> {}(key);
    }
};

static std::string getNormalizedPath (std::string_view rawPath)
{
    std::string normalized { rawPath };
    std::replace (normalized.begin(),
                  normalized.end(),
                  static_cast<char> (Chars::backslash),
                  static_cast<char> (Chars::slash));

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
                            std::string_view relativePath,
                            const jam::Strings& globs)
{
    for (int index { 0 }; index < globs.size(); ++index)
    {
        const auto glob { globs.getView (index) };

        if (matchesWildcard (glob, fileName) or matchesWildcard (glob, relativePath))
            return true;
    }

    return false;
}

static bool shouldIncludeFile (std::string_view fileName,
                               std::string_view relativePath,
                               const jam::Strings& includeGlobs,
                               const jam::Strings& excludeGlobs)
{
    if (includeGlobs.size() > 0 and not matchesAnyGlob (fileName, relativePath, includeGlobs))
        return false;

    if (excludeGlobs.size() > 0 and matchesAnyGlob (fileName, relativePath, excludeGlobs))
        return false;

    return true;
}

#ifndef _WIN32

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
    if ((name.size() == 1 and name.at (0) == static_cast<char> (Chars::dot))
        or name.compare (Chars::doubleDot) == 0
        or name.compare (gitDirectoryName) == 0)
        return false;

    if (not isIncludingHidden and not name.empty() and name.at (0) == static_cast<char> (Chars::dot))
        return false;

    return true;
}

static std::pair<bool, bool> getEntryFlags (unsigned char entryType,
                                            std::string& fullPath,
                                            std::string_view name)
{
    if (entryType == DT_DIR)
        return { true, false };

    if (entryType == DT_REG)
        return { false, true };

    if (entryType == DT_UNKNOWN)
    {
        const auto savedSize { fullPath.size() };
        appendPathSegment (fullPath, name);
        struct stat entryStat {};
        const auto statResult { ::lstat (fullPath.c_str(), &entryStat) };
        fullPath.resize (savedSize);

        if (statResult == 0)
            return { S_ISDIR (entryStat.st_mode), S_ISREG (entryStat.st_mode) };
    }

    return { false, false };
}

static bool hasGitignoreFile (std::string& directoryPath, std::string_view gitignoreName)
{
    const auto savedSize { directoryPath.size() };
    appendPathSegment (directoryPath, gitignoreName);
    const bool exists { ::access (directoryPath.c_str(), F_OK) == 0 };
    directoryPath.resize (savedSize);
    return exists;
}

template <typename OnDirectory, typename OnFile>
static void applyToDirectoryEntries (std::string& fullPath,
                                     std::string& displayBuffer,
                                     std::string& relBuffer,
                                     const IgnoreRules* ignoreRules,
                                     const jam::Strings& includeGlobs,
                                     const jam::Strings& excludeGlobs,
                                     std::string_view gitDirectoryName,
                                     bool isIncludingHidden,
                                     const OnDirectory& onDirectory,
                                     const OnFile& onFile,
                                     const std::atomic<bool>& shouldStop)
{
    if (not shouldStop.load (std::memory_order_relaxed))
    {
        std::unique_ptr<DIR, int (*) (DIR*)> directoryStream { ::opendir (fullPath.c_str()), &::closedir };

        if (directoryStream != nullptr)
        {
            for (const auto* entry { ::readdir (directoryStream.get()) };
                 entry != nullptr and not shouldStop.load (std::memory_order_relaxed);
                 entry = ::readdir (directoryStream.get()))
            {
                const std::string_view name { entry->d_name };

                if (shouldVisitEntry (name, gitDirectoryName, isIncludingHidden))
                {
                    const auto [isDirectory, isRegularFile] { getEntryFlags (entry->d_type, fullPath, name) };

                    if (isDirectory or isRegularFile)
                    {
                        const auto savedDisplaySize { displayBuffer.size() };
                        const auto savedRelSize { relBuffer.size() };
                        displayBuffer.append (name);

                        if (ignoreRules != nullptr)
                            relBuffer.append (name);

                        if (ignoreRules == nullptr or not ignoreRules->isIgnored (relBuffer, isDirectory))
                        {
                            const auto savedFullSize { fullPath.size() };
                            appendPathSegment (fullPath, name);

                            if (isDirectory)
                            {
                                displayBuffer.push_back (static_cast<char> (Chars::slash));
                                onDirectory();
                            }
                            else if (shouldIncludeFile (name, displayBuffer, includeGlobs, excludeGlobs))
                            {
                                onFile (fullPath, displayBuffer);
                            }

                            fullPath.resize (savedFullSize);
                        }

                        displayBuffer.resize (savedDisplaySize);

                        if (ignoreRules != nullptr)
                            relBuffer.resize (savedRelSize);
                    }
                }
            }
        }
    }
}

template <typename OnFile>
static void walkDirectory (std::string& fullPath,
                           std::string& displayBuffer,
                           std::string& relBuffer,
                           const IgnoreRules* ignoreRules,
                           const jam::Strings& includeGlobs,
                           const jam::Strings& excludeGlobs,
                           std::string_view gitDirectoryName,
                           std::string_view gitignoreName,
                           bool isIncludingHidden,
                           const OnFile& onFile,
                           const std::atomic<bool>& shouldStop)
{
    applyToDirectoryEntries (fullPath,
                             displayBuffer,
                             relBuffer,
                             ignoreRules,
                             includeGlobs,
                             excludeGlobs,
                             gitDirectoryName,
                             isIncludingHidden,
                             [&fullPath, &displayBuffer, &relBuffer,
                              ignoreRules, &includeGlobs, &excludeGlobs,
                              gitDirectoryName, gitignoreName, isIncludingHidden, &onFile, &shouldStop]
                             {
                                 const bool hasChildRules { ignoreRules != nullptr and hasGitignoreFile (fullPath, gitignoreName) };
                                 const auto childRules { hasChildRules
                                     ? std::optional<IgnoreRules> { ignoreRules->getChildRules (
                                         juce::File (juce::String::fromUTF8 (fullPath.data(), static_cast<int> (fullPath.size()))),
                                         relBuffer) }
                                     : std::nullopt };

                                 if (ignoreRules != nullptr)
                                     relBuffer.push_back (static_cast<char> (Chars::slash));

                                 walkDirectory (fullPath,
                                                displayBuffer,
                                                relBuffer,
                                                childRules.has_value() ? &(*childRules) : ignoreRules,
                                                includeGlobs,
                                                excludeGlobs,
                                                gitDirectoryName,
                                                gitignoreName,
                                                isIncludingHidden,
                                                onFile,
                                                shouldStop);
                             },
                             onFile,
                             shouldStop);
}

template <typename OnFile>
static void walkDirectoryBranch (std::string localFull,
                                 std::string localDisplay,
                                 std::string localRel,
                                 const IgnoreRules* ignoreRules,
                                 const jam::Strings& includeGlobs,
                                 const jam::Strings& excludeGlobs,
                                 std::string_view gitDirectoryName,
                                 std::string_view gitignoreName,
                                 bool isIncludingHidden,
                                 const OnFile& onFile,
                                 const std::atomic<bool>& shouldStop)
{
    const bool hasChildRules { ignoreRules != nullptr and hasGitignoreFile (localFull, gitignoreName) };
    const std::string_view relWithoutTrailingSlash {
        not localRel.empty() and localRel.back() == static_cast<char> (Chars::slash)
            ? std::string_view (localRel.data(), localRel.size() - 1)
            : std::string_view (localRel)
    };
    const auto childRules { hasChildRules
        ? std::optional<IgnoreRules> { ignoreRules->getChildRules (
            juce::File (juce::String::fromUTF8 (localFull.data(), static_cast<int> (localFull.size()))),
            relWithoutTrailingSlash) }
        : std::nullopt };

    walkDirectory (localFull,
                   localDisplay,
                   localRel,
                   childRules.has_value() ? &(*childRules) : ignoreRules,
                   includeGlobs,
                   excludeGlobs,
                   gitDirectoryName,
                   gitignoreName,
                   isIncludingHidden,
                   onFile,
                   shouldStop);
}

static std::tuple<std::vector<std::string>,
                  std::vector<std::string>,
                  std::vector<std::string>,
                  std::vector<std::string>,
                  std::vector<std::string>>
    collectRootEntries (std::string& fullPath,
                        std::string& displayBuffer,
                        std::string& relBuffer,
                        const IgnoreRules* ignoreRules,
                        const jam::Strings& includeGlobs,
                        const jam::Strings& excludeGlobs,
                        std::string_view gitDirectoryName,
                        bool isIncludingHidden,
                        const std::atomic<bool>& shouldStop)
{
    std::vector<std::string> rootFileFullPaths;
    std::vector<std::string> rootFileDisplayPaths;
    std::vector<std::string> subdirFullPaths;
    std::vector<std::string> subdirDisplayPrefixes;
    std::vector<std::string> subdirRelPrefixes;

    applyToDirectoryEntries (fullPath,
                             displayBuffer,
                             relBuffer,
                             ignoreRules,
                             includeGlobs,
                             excludeGlobs,
                             gitDirectoryName,
                             isIncludingHidden,
                             [&fullPath, &displayBuffer, &relBuffer, ignoreRules,
                              &subdirFullPaths, &subdirDisplayPrefixes, &subdirRelPrefixes]
                             {
                                 subdirFullPaths.emplace_back (fullPath);
                                 subdirDisplayPrefixes.emplace_back (displayBuffer);

                                 if (ignoreRules != nullptr)
                                 {
                                     relBuffer.push_back (static_cast<char> (Chars::slash));
                                     subdirRelPrefixes.emplace_back (relBuffer);
                                 }
                                 else
                                 {
                                     subdirRelPrefixes.emplace_back();
                                 }
                             },
                             [&rootFileFullPaths, &rootFileDisplayPaths] (const std::string& fileFullPath,
                                                                          std::string_view fileDisplayPath)
                             {
                                 rootFileFullPaths.emplace_back (fileFullPath);
                                 rootFileDisplayPaths.emplace_back (fileDisplayPath);
                             },
                             shouldStop);

    return { std::move (rootFileFullPaths),
             std::move (rootFileDisplayPaths),
             std::move (subdirFullPaths),
             std::move (subdirDisplayPrefixes),
             std::move (subdirRelPrefixes) };
}

#else

template <typename OnFile>
static void walkDirectory (const juce::File& directory,
                           std::string& displayBuffer,
                           std::string& relBuffer,
                           const IgnoreRules* ignoreRules,
                           const jam::Strings& includeGlobs,
                           const jam::Strings& excludeGlobs,
                           std::string_view gitDirectoryName,
                           bool isIncludingHidden,
                           const OnFile& onFile,
                           const std::atomic<bool>& shouldStop)
{
    if (not shouldStop.load (std::memory_order_relaxed))
    {
        for (const auto& dirEntry : juce::RangedDirectoryIterator (directory, false,
                                                                    juce::String::charToString (Chars::asterisk),
                                                                    juce::File::findFilesAndDirectories,
                                                                    juce::File::FollowSymlinks::no))
        {
            if (shouldStop.load (std::memory_order_relaxed))
                break;

            const auto file { dirEntry.getFile() };
            const auto nameString { file.getFileName() };
            const auto name { jam::Strings::getView (nameString) };
            const auto isDirectory { dirEntry.isDirectory() };

            if (name.compare (gitDirectoryName) != 0
                and (isIncludingHidden or (not name.empty() and name.at (0) != static_cast<char> (Chars::dot))))
            {
                const auto savedDisplaySize { displayBuffer.size() };
                const auto savedRelSize { relBuffer.size() };
                displayBuffer.append (name);

                if (ignoreRules != nullptr)
                    relBuffer.append (name);

                if (ignoreRules == nullptr or not ignoreRules->isIgnored (relBuffer, isDirectory))
                {
                    if (isDirectory)
                    {
                        displayBuffer.push_back (static_cast<char> (Chars::slash));
                        const bool hasChildRules { ignoreRules != nullptr and file.getChildFile (Id::gitignoreFile).existsAsFile() };
                        const auto childRules { hasChildRules
                            ? std::optional<IgnoreRules> { ignoreRules->getChildRules (file, relBuffer) }
                            : std::nullopt };

                        if (ignoreRules != nullptr)
                            relBuffer.push_back (static_cast<char> (Chars::slash));

                        walkDirectory (file,
                                       displayBuffer,
                                       relBuffer,
                                       childRules.has_value() ? &(*childRules) : ignoreRules,
                                       includeGlobs,
                                       excludeGlobs,
                                       gitDirectoryName,
                                       isIncludingHidden,
                                       onFile,
                                       shouldStop);
                    }
                    else if (shouldIncludeFile (name, displayBuffer, includeGlobs, excludeGlobs))
                    {
                        const auto fileFullPath { file.getFullPathName().toStdString() };
                        onFile (fileFullPath, displayBuffer);
                    }
                }

                displayBuffer.resize (savedDisplaySize);

                if (ignoreRules != nullptr)
                    relBuffer.resize (savedRelSize);
            }
        }
    }
}

#endif

static jam::Array<bool> getExplicitFlags (const std::vector<std::string>& collectedPaths,
                                          const jam::HashSet<std::string, TransparentStringHash, std::equal_to<>>& explicitSet)
{
    jam::Array<bool> flags;
    flags.ensureStorageAllocated (static_cast<int> (collectedPaths.size()));

    if (explicitSet.empty())
    {
        flags.insertMultiple (0, false, static_cast<int> (collectedPaths.size()));
    }
    else
    {
        for (const auto& path : collectedPaths)
            flags.add (explicitSet.contains (path));
    }

    return flags;
}

bool Paths::hasMultipleOrDirectoryInputs (const jam::Strings& rawPaths)
{
    if (rawPaths.size() > 1)
        return true;

    if (rawPaths.size() == 1)
    {
        const auto resolved { juce::File::getCurrentWorkingDirectory()
                                  .getChildFile (jam::Strings::fromView (rawPaths.getView (0)))
                                  .getLinkedTarget() };
        return resolved.isDirectory();
    }

    return false;
}

void Paths::applyToDirectory (const juce::File& rootDirectory,
                              std::string_view rawPathView,
                              const jam::Strings& includeGlobs,
                              const jam::Strings& excludeGlobs,
                              bool isIncludingHidden,
                              bool isIgnoringRules,
                              const std::function<void (int)>& onBranchCount,
                              const std::function<void (int, const std::string&, std::string_view)>& onFile,
                              const std::atomic<bool>& shouldStop)
{
    const auto gitDirectoryName { jam::Strings::getView (Id::gitDirectory) };
    std::optional<IgnoreRules> rules;
    std::string relBuffer;

    if (not isIgnoringRules)
    {
        rules.emplace (rootDirectory);
        relBuffer = rules->getRootRelativePrefix (rootDirectory);
    }

    const IgnoreRules* ignoreRules { rules.has_value() ? &(*rules) : nullptr };
    auto displayBuffer { getNormalizedDisplayPrefix (rawPathView) };
#ifndef _WIN32
    const auto gitignoreName { jam::Strings::getView (Id::gitignoreFile) };
    auto fullPath { rootDirectory.getFullPathName().toStdString() };
    auto [rootFileFullPaths, rootFileDisplayPaths, subdirFullPaths, subdirDisplayPrefixes, subdirRelPrefixes] {
        collectRootEntries (fullPath,
                            displayBuffer,
                            relBuffer,
                            ignoreRules,
                            includeGlobs,
                            excludeGlobs,
                            gitDirectoryName,
                            isIncludingHidden,
                            shouldStop)
    };
    const int subdirCount { static_cast<int> (subdirFullPaths.size()) };
    const int totalBranches { subdirCount + 1 };
    onBranchCount (totalBranches);

    const auto& rootFiles { rootFileFullPaths };
    const auto& rootDisplays { rootFileDisplayPaths };
    const auto& fullPaths { subdirFullPaths };
    const auto& displayPrefixes { subdirDisplayPrefixes };
    const auto& relPrefixes { subdirRelPrefixes };

    Jobs::run (totalBranches,
               [&rootFiles, &rootDisplays, &fullPaths, &displayPrefixes, &relPrefixes,
                ignoreRules, &includeGlobs, &excludeGlobs,
                gitDirectoryName, gitignoreName, isIncludingHidden, &onFile, &shouldStop] (int idx)
    {
        if (idx == 0)
        {
            for (size_t fileIndex { 0 };
                 fileIndex < rootFiles.size() and not shouldStop.load (std::memory_order_relaxed);
                 ++fileIndex)
            {
                onFile (0, rootFiles.at (fileIndex), rootDisplays.at (fileIndex));
            }
        }
        else
        {
            const auto subdirIndex { static_cast<size_t> (idx - 1) };
            walkDirectoryBranch (fullPaths.at (subdirIndex),
                                 displayPrefixes.at (subdirIndex),
                                 relPrefixes.at (subdirIndex),
                                 ignoreRules,
                                 includeGlobs,
                                 excludeGlobs,
                                 gitDirectoryName,
                                 gitignoreName,
                                 isIncludingHidden,
                                 [&onFile, idx] (const std::string& fileFullPath, std::string_view fileDisplayPath)
                                 {
                                     onFile (idx, fileFullPath, fileDisplayPath);
                                 },
                                 shouldStop);
        }
    });
#else
    onBranchCount (1);
    walkDirectory (rootDirectory,
                   displayBuffer,
                   relBuffer,
                   ignoreRules,
                   includeGlobs,
                   excludeGlobs,
                   gitDirectoryName,
                   isIncludingHidden,
                   [&onFile] (const std::string& fileFullPath, std::string_view fileDisplayPath)
                   {
                       onFile (0, fileFullPath, fileDisplayPath);
                   },
                   shouldStop);
#endif
}

std::tuple<juce::Result, std::vector<std::string>, jam::Array<bool>> Paths::getFiles (const jam::Strings& rawPaths,
                                                                                      const jam::Strings& includeGlobs,
                                                                                      const jam::Strings& excludeGlobs,
                                                                                      bool isIncludingHidden,
                                                                                      bool isIgnoringRules)
{
    std::vector<std::string> collectedPaths;
    jam::HashSet<std::string, TransparentStringHash, std::equal_to<>> explicitSet;
    const auto workingDirectory { juce::File::getCurrentWorkingDirectory() };

    for (int index { 0 }; index < rawPaths.size(); ++index)
    {
        const auto rawPathView { rawPaths.getView (index) };
        const auto resolved { workingDirectory.getChildFile (jam::Strings::fromView (rawPathView)).getLinkedTarget() };

        if (not resolved.exists())
        {
            const auto rawPathString { jam::Strings::fromView (rawPathView) };
            return { juce::Result::fail (
                         juce::String::formatted (text::Diagnostics::failNotFound, rawPathString.toRawUTF8())),
                     std::vector<std::string> {},
                     jam::Array<bool> {} };
        }

        if (resolved.isDirectory())
        {
            std::vector<std::vector<std::string>> branchPaths;
            const std::atomic<bool> shouldStop { false };
            applyToDirectory (resolved,
                              rawPathView,
                              includeGlobs,
                              excludeGlobs,
                              isIncludingHidden,
                              isIgnoringRules,
                              [&branchPaths] (int branchCount)
                              {
                                  branchPaths.resize (static_cast<size_t> (branchCount));
                              },
                              [&branchPaths] (int branchIndex, const std::string& fileFullPath, std::string_view fileDisplayPath)
                              {
                                  juce::ignoreUnused (fileFullPath);
                                  branchPaths.at (static_cast<size_t> (branchIndex)).emplace_back (fileDisplayPath);
                              },
                              shouldStop);

            size_t totalBranchCount { 0 };

            for (const auto& branch : branchPaths)
                totalBranchCount += branch.size();

            collectedPaths.reserve (collectedPaths.size() + totalBranchCount);

            for (auto& branch : branchPaths)
            {
                for (auto& path : branch)
                    collectedPaths.emplace_back (std::move (path));
            }
        }
        else
        {
            std::string normalized { rawPathView };
            std::replace (normalized.begin(),
                          normalized.end(),
                          static_cast<char> (Chars::backslash),
                          static_cast<char> (Chars::slash));
            collectedPaths.push_back (normalized);
            explicitSet.emplace (std::move (normalized));
        }
    }

    std::sort (collectedPaths.begin(), collectedPaths.end());

    if (rawPaths.size() > 1)
        collectedPaths.erase (std::unique (collectedPaths.begin(), collectedPaths.end()), collectedPaths.end());

    auto isExplicitFlags (getExplicitFlags (collectedPaths, explicitSet));
    return { juce::Result::ok(), std::move (collectedPaths), std::move (isExplicitFlags) };
}
