#pragma once
#include <JuceHeader.h>
#include "TextDocument.h"

/** @brief Stores the pre-edit snapshots of transactions on disk, so that `saw undo` can restore them.
 *
 *  Each transaction has one record file and one blob file per original file. The record file
 *  holds the JSON of the transaction. A blob file holds the bytes of one original file.
 *
 *  A record is staged in the pending folder, published with one rename into the undo folder,
 *  and claimed by `undo` with the reverse rename. No lock exists.
 */
struct UndoCache
{
    /** @brief Returns the folder that holds the blob files.
     *
     *  @return Blob folder inside the user application data folder.
     */
    static juce::File getBlobDirectory();

    /** @brief Returns the path of the record of a transaction in the pending folder.
     *
     *  @param transactionId Identifier of the transaction.
     *
     *  @return Path of the pending record file. The file may not exist.
     */
    static juce::File getPendingFile (const juce::String& transactionId);

    /** @brief Returns the path of the blob file of one original file of a transaction.
     *
     *  @param transactionId Identifier of the transaction.
     *  @param index         Zero-based index of the file in the transaction.
     *
     *  @return Path of the blob file. The file may not exist.
     */
    static juce::File getBlobFile (const juce::String& transactionId, int index);

    /** @brief Publishes a pending record into the undo folder with one rename.
     *
     *  @param transactionId Identifier of the transaction whose record is pending.
     *
     *  @return Ok when the rename succeeds, otherwise the failure of the rename.
     */
    static juce::Result publishTransaction (const juce::String& transactionId);

    /** @brief Claims a transaction for `undo`, by moving its record out of the undo folder.
     *
     *  The claim creates the pending record, then moves the undo record onto it. When a pending
     *  record already exists, the claim fails, unless isForce is set. A forced claim takes a pending
     *  record that a stopped process left behind.
     *
     *  @param transactionId Identifier of the transaction to claim.
     *  @param isForce       True to take a pending record that already exists.
     *
     *  @return Status result paired with true when the record came out of the undo folder, and
     *          false when the record was not moved.
     */
    static std::pair<juce::Result, bool> claimTransaction (const juce::String& transactionId, bool isForce);

    /** @brief Stages the blob files and the record of a transaction, in parallel.
     *
     *  A blob holds the planned bytes when the target and the blob folder can be swapped, so that
     *  the commit is one exchange of two files. Otherwise a blob holds the original bytes. Any
     *  failed write removes the transaction again.
     *
     *  @param commandName     Subcommand name stored in the record.
     *  @param targetFiles     The files that the transaction changes, in order.
     *  @param executableFlags The executable permission flag of each target file, before the edit.
     *  @param originalDocuments The document of each target file, before the edit.
     *  @param plannedTexts    The planned bytes of each target file.
     *
     *  @return Status result paired with the generated transaction id on success.
     */
    static std::pair<juce::Result, juce::String> addTransaction (const juce::String& commandName,
                                                                 const jam::Array<juce::File>& targetFiles,
                                                                 const jam::Array<bool>& executableFlags,
                                                                 const jam::Array<std::unique_ptr<TextDocument>>& originalDocuments,
                                                                 const std::vector<std::string>& plannedTexts);

    /** @brief Lists the transaction ids, newest first.
     *
     *  A pending id is listed only when its record is valid.
     *
     *  @return The ids, newest first.
     */
    static jam::Strings getTransactionIds();

    /** @brief Loads and validates the record of a transaction, and checks that its blob files exist.
     *
     *  @param transactionId Identifier of the transaction.
     *
     *  @return Status result paired with the parsed record, or failure with null. A record that
     *          does not exist is reported as unknown. A record that does not validate is reported as corrupt.
     */
    static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getTransactionDocument (const juce::String& transactionId);

    /** @brief Removes a transaction: its pending record, its undo record, and its blob files.
     *
     *  @param transactionId Identifier of the transaction to remove.
     */
    static void removeTransaction (const juce::String& transactionId);

    /** @brief Removes the oldest transactions, and keeps the newest 32.
     *
     *  A blob file is deleted only when the record of its transaction was deleted. A concurrent
     *  `undo` that claimed the record then keeps its blob files.
     */
    static void removeOldestTransactions();
};
