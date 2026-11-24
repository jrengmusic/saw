#pragma once
#include <JuceHeader.h>

/** @brief Resolves file and directory inputs with glob, hidden, and ignore filtering.
 */
struct Paths
{
    /** @brief Expands file and directory paths into sorted files and explicit flags.
     *
     *  Each path is resolved one time here, so that a verb reads the file it receives and resolves nothing again.
     *
     *  @param rawPaths          Input file or directory paths.
     *  @param includeGlobs      Wildcard patterns (`*`, `?`) a file name or path must match when non-empty.
     *  @param excludeGlobs      Wildcard patterns that exclude matching files.
     *  @param isIncludingHidden True to include dot-prefixed entries.
     *  @param isIgnoringRules   True to bypass `.gitignore` and global exclude rules.
     *
     *  @return Tuple of: the failures, sorted by path, each with its failure result; the display path
     *          and the resolved file of each input, sorted by display path; and one flag per file, true
     *          when the file was named on the command line.
     *
     *  Duplicate paths are removed when more than one raw path is given.
     */
    static std::tuple<std::vector<std::pair<std::string, juce::Result>>, std::vector<std::pair<std::string, juce::File>>, jam::Array<bool>> getFiles (const jam::Strings& rawPaths,
                                                                                                                                                      const jam::Strings& includeGlobs,
                                                                                                                                                      const jam::Strings& excludeGlobs,
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
     *  @return Tuple of: the result of the first failure in path order, or ok; the display paths and
     *          resolved files, sorted by display path; and the explicit flags. The list and the flags are empty when a failure exists.
     */
    static std::tuple<juce::Result, std::vector<std::pair<std::string, juce::File>>, jam::Array<bool>> getReadableFiles (const jam::Strings& rawPaths,
                                                                                                                         const jam::Strings& includeGlobs,
                                                                                                                         const jam::Strings& excludeGlobs,
                                                                                                                         bool isIncludingHidden,
                                                                                                                         bool isIgnoringRules);

    /** @brief Converts a raw path to the form that is shown to the user.
     *
     *  On Windows, backslash separators become slash separators. Other platforms keep the path as it is.
     *
     *  @param rawPath The path as the user gave it.
     *
     *  @return The display path.
     */
    static std::string getDisplayPath (std::string_view rawPath);

    /** @brief Orders two path entries by their path, for sorting. The value is ignored.
     *
     *  @tparam Value  The type of the value that the entry holds.
     *  @param firstEntry  The first entry.
     *  @param secondEntry The second entry.
     *
     *  @return True when the path of firstEntry sorts before the path of secondEntry.
     */
    template <typename Value>
    static bool isPathEarlier (const std::pair<std::string, Value>& firstEntry, const std::pair<std::string, Value>& secondEntry)
    {
        const auto& [firstPath, firstValue] { firstEntry };
        const auto& [secondPath, secondValue] { secondEntry };
        juce::ignoreUnused (firstValue, secondValue);
        return firstPath < secondPath;
    }

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

    /** @brief Lists the names of the regular files in one folder.
     *
     *  Folders, hidden names, and symbolic links are not listed. A folder that cannot be read, or whose
     *  listing fails part-way, gives an empty list.
     *
     *  @param directory The folder to list. Its entries are not walked.
     *
     *  @return The file names, without their folder path.
     */
    static jam::Strings getFileNames (const juce::File& directory);

    /** @brief Walks each root and reports each included file and each unreadable folder.
     *
     *  All roots are items of one walk, so that many small roots do not each start and join threads.
     *  A folder that cannot be read, or whose listing fails part-way, is reported as unreadable.
     *  Symbolic links and Windows junctions are neither walked nor reported. The walk runs on worker
     *  threads. Folders and batches of files are the items of Jobs::runRecursive. Each callback
     *  receives the index of its worker. One thread uses one worker index at a time, so a caller keeps
     *  one slot per worker and needs no lock. The order of the callbacks is not defined.
     *
     *  @param roots             Pairs of the caller-supplied root path prefix, used in display paths, and the resolved directory to walk.
     *  @param includeGlobs      Wildcard patterns (`*`, `?`) a file must match when non-empty.
     *  @param excludeGlobs      Wildcard patterns that exclude matching files.
     *  @param isIncludingHidden True to include dot-prefixed entries.
     *  @param isIgnoringRules   True to bypass `.gitignore` and global exclude rules.
     *  @param onWorkerCount     Callback invoked once before the walk, with the number of workers.
     *  @param onFile            Callback invoked per matching file with `(workerIndex, fullPath, displayPath)`.
     *  @param onUnreadable      Callback invoked per folder that cannot be read, with `(workerIndex, displayPath)`.
     *  @param shouldStop        Atomic cancellation flag polled during the walk.
     */
    static void applyToDirectories (const std::vector<std::pair<std::string, juce::File>>& roots,
                                    const jam::Strings& includeGlobs,
                                    const jam::Strings& excludeGlobs,
                                    bool isIncludingHidden,
                                    bool isIgnoringRules,
                                    const std::function<void (int)>& onWorkerCount,
                                    const std::function<void (int, const std::string&, std::string_view)>& onFile,
                                    const std::function<void (int, std::string_view)>& onUnreadable,
                                    const std::atomic<bool>& shouldStop);
};
