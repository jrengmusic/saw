#pragma once
#include <JuceHeader.h>
#include "StagedFile.h"

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

    /** @brief Returns the path of the blob file of one original file of a transaction.
     *
     *  @param transactionId Identifier of the transaction.
     *  @param index         Zero-based index of the file in the transaction.
     *
     *  @return Path of the blob file. The file may not exist.
     */
    static juce::File getBlobFile (std::string_view transactionId, int index);

    /** @brief Publishes a pending record into the undo folder with one rename.
     *
     *  @param transactionId Identifier of the transaction whose record is pending.
     *
     *  @return Ok when the rename succeeds. Otherwise the failure that names the transaction id and reports
     *          that its record is pending.
     */
    static juce::Result publishTransaction (std::string_view transactionId);

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
    static std::pair<juce::Result, bool> claimTransaction (std::string_view transactionId, bool isForce);

    /** @brief Stores a transaction in the pending folder: its record, and the blob files that hold the original bytes.
     *
     *  @param commandName The name of the mutating subcommand, recorded with the transaction.
     *  @param stagedFiles The staged files of the transaction, in transaction order.
     *  @param folders     The folders that the transaction creates, recorded with the transaction.
     *
     *  @return Status result paired with the transaction id. The id is empty when the record cannot be stored.
     */
    static std::pair<juce::Result, std::string> addTransaction (std::string_view commandName,
                                                                const jam::Array<StagedFile>& stagedFiles,
                                                                const jam::Array<juce::File>& folders);

    static void applyToTransactions (const std::function<void (std::string_view, const juce::Result&, const jam::JsonDocument*)>& function);

    /** @brief Loads and validates the record of a transaction, and checks that its blob files exist.
     *
     *  @param transactionId Identifier of the transaction.
     *
     *  @return Status result paired with the parsed record, or failure with null. A record that
     *          does not exist is reported as unknown. A record that does not validate is reported as corrupt.
     */
    static std::pair<juce::Result, std::unique_ptr<jam::JsonDocument>> getTransactionDocument (std::string_view transactionId);

    /** @brief Removes a transaction: its pending record, its undo record, and its blob files.
     *
     *  @param transactionId Identifier of the transaction to remove.
     */
    static void removeTransaction (std::string_view transactionId);

    /** @brief Removes the oldest transactions, and keeps the newest 32.
     *
     *  A blob file is deleted only when the record of its transaction was deleted. A concurrent
     *  `undo` that claimed the record then keeps its blob files.
     */
    static void removeOldestTransactions();
};
