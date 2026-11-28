#pragma once
#include <JuceHeader.h>
#include "IgnoreRules.h"
#include "Input.h"
#include "generated/Generated.h"

/** @brief Resolves file and directory inputs with glob, hidden, and ignore filtering.
 */
struct Paths
{
    /** @brief Walked folder state: full path, display prefix, relative path, root display prefix length, and active ignore rules. */
    using Folder = std::tuple<std::string, std::string, std::string, size_t, std::shared_ptr<const IgnoreRules>>;
    /** @brief Kind of directory entry reported during a walk. */
    enum class EntryType { file, directory, symlink };
    /** @brief Entry metadata: byte size, modification time in Unix milliseconds, and POSIX permission bits. */
    using Metadata = std::tuple<juce::int64, juce::int64, int>;
    /** @brief One entry slot in a folder listing: display path offset, display path length, entry type, and metadata. */
    using ListedEntry = std::tuple<size_t, size_t, EntryType, Metadata>;
    /** @brief Directory listing: contiguous display-path buffer, entry slots, gitignore presence, and skipped count. */
    using Listing = std::tuple<std::string, jam::Array<ListedEntry>, bool, int>;
    /** @brief A walked folder paired with its listing, shared by every ListedFile slot in that folder. */
    using ListedFolder = std::pair<Folder, Listing>;
    /** @brief Handle to one entry inside a shared ListedFolder, by slot index. */
    using ListedFile = std::pair<std::shared_ptr<const ListedFolder>, int>;
    /** @brief Explicit command-line file: display path paired with its resolved juce::File. */
    using NamedFile = std::pair<std::string, juce::File>;
    /** @brief One resolved input entry: a walked listing slot, an explicit named file, or standard input. */
    using Entry = std::variant<ListedFile, NamedFile, Input::StandardInput>;

    /** @brief Default root path list (`"."`) used when a verb receives no positional path. */
    static constexpr std::array<std::string_view, 1> defaultPaths { Id::currentDirectory };

    /** @brief Paths that could not be read, each with the result that describes the failure.
     *
     *  The path is the raw path that the user gave, or the display path of a folder that could not be read.
     *  The two lists have the same length, and the path at each index is paired with the result at that index.
     */
    using Failures = std::pair<jam::Strings, jam::Array<juce::Result>>;

    /** @brief Expands file and directory paths into sorted entries.
     *
     *  Each path is resolved one time here, so that a verb reads the file it receives and resolves nothing again.
     *
     *  @param rawPaths          Input file or directory paths.
     *  @param includeGlobs      Wildcard patterns (`*`, `?`) a file name or path must match when non-empty.
     *  @param excludeGlobs      Wildcard patterns that exclude matching files.
     *  @param isIncludingHidden True to include dot-prefixed entries.
     *  @param isIgnoringRules   True to bypass `.gitignore` and global exclude rules.
     *  @param maxDepth          Optional maximum recursion depth below each root.
     *  @param isListingEntries  True to report directories and symbolic links in addition to regular files.
     *  @param isReadingMetadata True to populate size, modification time, and permissions on each listed entry.
     *
     *  @return Tuple of: the failures, each with its failure result, in the order found: first the failures of the
     *          raw paths, in the order given, then the failures of the folder walk, by worker index; the entries, one per
     *          file, sorted by display path, each with its display path, its resolved file, and its explicit
     *          flag, true when the file was named on the command line; and the count of entries that the walk did not search.
     *
     *  A file named on the command line wins over the same file found by the walk. The sort puts the explicit
     *  entry first among entries with the same display path, and the duplicate removal keeps the first entry.
     *  The duplicate removal runs only when more than one raw path is given.
     */
    static std::tuple<Failures, jam::Array<Entry>, int> getFiles (juce::Span<const std::string_view> rawPaths,
                                                                  juce::Span<const std::string_view> includeGlobs,
                                                                  juce::Span<const std::string_view> excludeGlobs,
                                                                  bool isIncludingHidden,
                                                                  bool isIgnoringRules,
                                                                  std::optional<int> maxDepth = std::nullopt,
                                                                  bool isListingEntries = false,
                                                                  bool isReadingMetadata = false);

    /** @brief Expands the paths like getFiles, and stops at the first failure.
     *
     *  @param rawPaths          Input file or directory paths.
     *  @param includeGlobs      Wildcard patterns (`*`, `?`) a file name or path must match when non-empty.
     *  @param excludeGlobs      Wildcard patterns that exclude matching files.
     *  @param isIncludingHidden True to include dot-prefixed entries.
     *  @param isIgnoringRules   True to bypass `.gitignore` and global exclude rules.
     *
     *  @return Pair of: the result of the first failure in path order, or ok; and the entries, sorted by
     *          display path. The entries are empty when a failure exists.
     */
    static std::pair<juce::Result, jam::Array<Entry>> getReadableFiles (juce::Span<const std::string_view> rawPaths,
                                                                        juce::Span<const std::string_view> includeGlobs,
                                                                        juce::Span<const std::string_view> excludeGlobs,
                                                                        bool isIncludingHidden,
                                                                        bool isIgnoringRules);

    /** @brief Sorts the raw paths into failures, named files, and folders to walk.
     *
     *  A path that does not exist is a failure. A file is an explicit entry, because the user named it.
     *  A folder keeps its raw path, which becomes the prefix of the display paths of the files under it.
     *  A standard input argument gives one entry with an empty file, and it is added once.
     *
     *  @param rawPaths The paths as the user gave them.
     *
     *  @return Tuple of: the failures; the explicit entries of the named files; and the folders, each as a pair
     *          of its raw path and its resolved folder.
     */
    static std::tuple<Failures, jam::Array<Entry>, jam::Array<std::pair<std::string_view, juce::File>>> getClassifiedPaths (juce::Span<const std::string_view> rawPaths);

    /** @brief Converts a raw path to the form that is shown to the user.
     *
     *  On Windows, backslash separators become slash separators. Other platforms keep the path as it is.
     *
     *  @param rawPath The path as the user gave it.
     *
     *  @return The display path.
     */
    static std::string getDisplayPath (std::string_view rawPath);

    /** @brief Returns the display path view owned by a ListedFile's shared listing.
     *
     *  @param listedFile The listing handle.
     *
     *  @return Display path view valid while the shared listing lives.
     */
    static std::string_view getDisplayPath (const ListedFile& listedFile);

    /** @brief Returns the display path view of any Entry.
     *
     *  @param entry The entry.
     *
     *  @return Display path view, or the stdin path marker for standard input.
     */
    static std::string_view getDisplayPath (const Entry& entry);

    /** @brief Resolves a ListedFile handle to its juce::File on disk.
     *
     *  @param listedFile The listing handle.
     *
     *  @return Resolved juce::File.
     */
    static juce::File getFile (const ListedFile& listedFile);

    /** @brief Resolves any Entry to its juce::File on disk, or an empty File for standard input.
     *
     *  @param entry The entry.
     *
     *  @return Resolved juce::File, or empty File for standard input.
     */
    static juce::File getFile (const Entry& entry);

    /** @brief Returns the ListedEntry slot (entry type and metadata) for any Entry.
     *
     *  For a NamedFile, reads metadata from disk on demand.
     *
     *  @param entry The entry.
     *
     *  @return Ok paired with the ListedEntry, or a read failure paired with a default slot.
     */
    static std::pair<juce::Result, ListedEntry> getListedEntry (const Entry& entry);

    /** @brief Tells whether an Entry was named explicitly on the command line rather than discovered by walking.
     *
     *  @param entry The entry.
     *
     *  @return True for NamedFile and StandardInput, false for ListedFile.
     */
    static bool isExplicit (const Entry& entry);

    /** @brief Resolves a raw path to its physical target, following symbolic links.
     *
     *  std::filesystem::canonical follows each link before it applies a ".." after it. A text
     *  removal of ".." would name another file. When canonical fails, for example on a missing path,
     *  a broken link, or a closed folder, the result is the raw path against the working folder, so
     *  that the caller reports it.
     *
     *  @param rawPath The path as the user gave it.
     *
     *  @return The physical target, or the raw path against the working folder when it cannot be resolved.
     */
    static juce::File getLinkedTarget (std::string_view rawPath);

    /** @brief Tests whether a text matches a wildcard pattern, where `*` matches any run of characters and `?` matches one character.
     *
     *  The whole text must match.
     *
     *  @param wildcard The wildcard pattern.
     *  @param text     The text to test.
     *
     *  @return True when the whole text matches the pattern.
     */
    static bool isWildcardMatch (std::string_view wildcard, std::string_view text) noexcept;

    /** @brief Returns the file of a carried library build, from the configured folder or from the cache.
     *
     *  The configured folder is searched first. When it holds no such file, the cache folder of each language is searched.
     *
     *  @param libraryName The name of the library.
     *  @param fileName    The name of the file.
     *
     *  @return The file that exists, or an empty File when no folder holds it.
     */
    static juce::File getCarriedFile (std::string_view libraryName, std::string_view fileName);

    /** @brief Calls a function with the name of each regular file in one folder. The walk does not recurse.
     *
     *  Folders, symbolic links, and dot-prefixed names are skipped. No ignore rules apply. A folder that cannot
     *  be listed gives no call.
     *
     *  @param directory The folder to list.
     *  @param function  The function that receives each file name. The view is valid only during the call.
     */
    static void applyToFileNames (const juce::File& directory, const std::function<void (std::string_view)>& function);

    /** @brief Appends the failures of addedFailures to failures, keeping each path paired with its result.
     *
     *  @param failures      The failures that receive the added entries.
     *  @param addedFailures The failures to append, in their order.
     */
    static void addFailures (Failures& failures, Failures&& addedFailures);

    static jam::Array<juce::Result> getFailureResults (const Failures& failures);

    /** @brief Walks each root and reports each included file. Returns each unreadable folder.
     *
     *  Each root is one item of one walk, so that many small roots do not each start and join threads.
     *  A folder item reads its listing once and publishes that listing. The folder item then walks the first
     *  64 entries of the listing. Each further range of 64 entries of the same listing is one more item.
     *  Symbolic links and Windows junctions are neither walked nor reported. A folder that cannot be read, or
     *  whose listing fails part-way, is unreadable. It is returned, not reported through onFile.
     *  The walk runs on worker threads. Each callback receives the index of its worker. One thread uses one
     *  worker index at a time, so a caller keeps one slot per worker and needs no lock. The order of the
     *  callbacks is not defined.
     *
     *  @param roots             Pairs of the caller-supplied root path prefix, used in display paths, and the resolved directory to walk.
     *  @param includeGlobs      Wildcard patterns (`*`, `?`) a file must match when non-empty.
     *  @param excludeGlobs      Wildcard patterns that exclude matching files.
     *  @param isIncludingHidden True to include dot-prefixed entries.
     *  @param isIgnoringRules   True to bypass `.gitignore` and global exclude rules.
     *  @param onWorkerCount     Callback invoked once before the walk, with the number of workers.
     *  @param onFile            Callback invoked per matching entry with `(workerIndex, listedFile)`.
     *  @param shouldStop        Atomic cancellation flag polled during the walk.
     *  @param maxDepth          Optional maximum recursion depth below each root.
     *  @param isListingEntries  True to report directories and symbolic links in addition to regular files.
     *  @param isReadingMetadata True to populate size, modification time, and permissions on each listed entry.
     *
     *  @return The unreadable folders, each with its display path and its read failure, and the count of entries that the walk skipped.
     *          The order of the folders depends on the walk, so it is not defined.
     */
    static std::pair<Failures, int> applyToDirectories (const jam::Array<std::pair<std::string_view, juce::File>>& roots,
                                                        juce::Span<const std::string_view> includeGlobs,
                                                        juce::Span<const std::string_view> excludeGlobs,
                                                        bool isIncludingHidden,
                                                        bool isIgnoringRules,
                                                        const std::function<void (int)>& onWorkerCount,
                                                        const std::function<void (int, const ListedFile&)>& onFile,
                                                        const std::atomic<bool>& shouldStop,
                                                        std::optional<int> maxDepth = std::nullopt,
                                                        bool isListingEntries = false,
                                                        bool isReadingMetadata = false);
};
