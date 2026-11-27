#pragma once
#include <JuceHeader.h>

/** @brief Resolves file and directory inputs with glob, hidden, and ignore filtering.
 */
struct Paths
{
    /** @brief One file to read: its display path, the file it resolves to, and its explicit flag.
     *
     *  The explicit flag is true when the user named the file on the command line. A file found by a walk has false.
     */
    using Entry = std::tuple<std::string, juce::File, bool>;

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
                                                                  bool isIgnoringRules);

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
     *  @param onFile            Callback invoked per matching file with `(workerIndex, fullPath, displayPath)`.
     *  @param shouldStop        Atomic cancellation flag polled during the walk.
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
                                                        const std::function<void (int, const std::string&, std::string_view)>& onFile,
                                                        const std::atomic<bool>& shouldStop);
};
