#pragma once
#include <JuceHeader.h>

/** @brief Resolves file and directory inputs with glob, hidden, and ignore filtering.
 */
struct Paths
{
    /** @brief Reports whether inputs may produce output from multiple files.
     *
     *  @param rawPaths User-supplied file or directory path arguments.
     *
     *  @return True when more than one path is given or the single input resolves to a directory.
     */
    static bool hasMultipleOrDirectoryInputs (const jam::Strings& rawPaths);

    /** @brief Expands file and directory paths into sorted display paths and explicit flags.
     *
     *  @param rawPaths          Input file or directory paths.
     *  @param includeGlobs      Wildcard patterns (`*`, `?`) a file name or path must match when non-empty.
     *  @param excludeGlobs      Wildcard patterns that exclude matching files.
     *  @param isIncludingHidden True to include dot-prefixed entries.
     *  @param isIgnoringRules   True to bypass `.gitignore` and global exclude rules.
     *
     *  @return Tuple of status result, sorted display paths, and per-path explicit-input flags.
     */
    static std::tuple<juce::Result, std::vector<std::string>, jam::Array<bool>> getFiles (const jam::Strings& rawPaths,
                                                                                          const jam::Strings& includeGlobs,
                                                                                          const jam::Strings& excludeGlobs,
                                                                                          bool isIncludingHidden,
                                                                                          bool isIgnoringRules);

    /** @brief Traverses @p rootDirectory and invokes @p onFile for each included regular file.
     *
     *  Dispatches top-level branches across worker threads on POSIX and stops when @p shouldStop is set.
     *
     *  @param rootDirectory     Resolved directory to walk.
     *  @param rawPathView       Caller-supplied root path prefix used in display paths.
     *  @param includeGlobs      Wildcard patterns (`*`, `?`) a file must match when non-empty.
     *  @param excludeGlobs      Wildcard patterns that exclude matching files.
     *  @param isIncludingHidden True to include dot-prefixed entries.
     *  @param isIgnoringRules   True to bypass `.gitignore` and global exclude rules.
     *  @param onBranchCount     Callback invoked once before traversal with the worker branch count.
     *  @param onFile            Callback invoked per matching file with `(branchIndex, fullPath, displayPath)`.
     *  @param shouldStop        Atomic cancellation flag polled during traversal.
     */
    static void applyToDirectory (const juce::File& rootDirectory,
                                  std::string_view rawPathView,
                                  const jam::Strings& includeGlobs,
                                  const jam::Strings& excludeGlobs,
                                  bool isIncludingHidden,
                                  bool isIgnoringRules,
                                  const std::function<void (int)>& onBranchCount,
                                  const std::function<void (int, const std::string&, std::string_view)>& onFile,
                                  const std::atomic<bool>& shouldStop);
};
