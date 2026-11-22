#pragma once
#include <JuceHeader.h>

/** @brief Persists, validates, and prunes pre-edit transaction snapshots on disk for `saw undo`.
 */
struct UndoCache
{
    /** @brief Maximum number of retained undo transactions before oldest directories are deleted.
     */
    static constexpr int maxTransactions { 32 };

    /** @brief Hex character width of the collision-avoidance suffix on transaction IDs.
     */
    static constexpr int randomHexLength { 6 };

    /** @brief Exclusive upper bound for the random initial hex suffix on a new UTC prefix.
     */
    static constexpr int initialHexBound { 0x100000 };

    /** @brief Fixed stack buffer size in bytes for formatting UTC transaction timestamps.
     */
    static constexpr size_t timeBufferCapacity { 32 };

    /** @brief Returns the undo root directory inside user application data.
     *
     *  @return Directory holding per-transaction backup folders.
     */
    static juce::File getDirectory();

    /** @brief Generates a unique transaction identifier and matching ISO-8601 UTC timestamp.
     *
     *  @return Pair of `(transactionId, isoTimestamp)`.
     */
    static std::pair<juce::String, juce::String> getNextIdAndTimestamp();

    /** @brief Writes pre-edit backup blobs and `meta.json`, then prunes transactions beyond @ref maxTransactions.
     *
     *  Deletes the newly created transaction directory if any blob or metadata write fails.
     *
     *  @param commandName     Subcommand name stored in `meta.json`.
     *  @param canonicalPaths  Canonical file paths; must match all parallel arrays in size.
     *  @param preHashes       Pre-edit SHA-256 hex digests for each file.
     *  @param postHashes      Expected post-edit SHA-256 hex digests for each file.
     *  @param executableFlags Pre-edit executable permission flags for each file.
     *  @param preEditBlobs    Raw pre-edit file bytes stored as numbered blob files.
     *
     *  @return Status result paired with the generated transaction ID on success.
     */
    static std::pair<juce::Result, juce::String> addTransaction (const juce::String& commandName,
                                                                 const jam::Strings& canonicalPaths,
                                                                 const jam::Strings& preHashes,
                                                                 const jam::Strings& postHashes,
                                                                 const jam::Array<bool>& executableFlags,
                                                                 const jam::Array<std::string_view>& preEditBlobs);

    /** @brief Lists existing transaction identifiers ordered from newest to oldest.
     *
     *  @return Sorted array of transaction folder names, newest first.
     */
    static jam::Strings getTransactionIds();

    /** @brief Loads and validates `meta.json` and referenced blob files for @p transactionId.
     *
     *  @param transactionId Identifier of the cached transaction directory.
     *
     *  @return Status result paired with the parsed `meta.json` document, or failure with null.
     */
    static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getTransactionDocument (const juce::String& transactionId);

private:
    static void removeOldestTransactions();
};
