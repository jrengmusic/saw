#pragma once
#include <JuceHeader.h>
#include "LineDiff.h"
#include "TextDocument.h"

/** @brief One target file of a transaction, with its original bytes and its planned bytes.
 *
 *  A commit and an undo write the planned bytes. A restore brings the original bytes back. Each
 *  operation reads both byte sets from the documents and the diff that the staged file owns.
 */
class StagedFile
{
public:
    /** @brief Stages an edit of an existing file. The planned bytes come from the diff.
     *
     *  The planned content is the diff itself, so the planned bytes are built one time and are not stored again.
     *  The executable bit is read from the target file. The target is marked as existing.
     *
     *  @param newTargetFile       The file that the transaction writes.
     *  @param newOriginalDocument The document before the edit. Must not be null.
     *  @param newDiff             The line-level diff that produces the planned bytes.
     *  @param newBlobIndex        The index of this file in the transaction. It names the blob file.
     */
    StagedFile (const juce::File& newTargetFile, std::unique_ptr<TextDocument> newOriginalDocument, LineDiff newDiff, int newBlobIndex);

    /** @brief Stages a file whose planned content is a document, not a diff, as an undo entry does.
     *
     *  @param newTargetFile        The file that the transaction writes.
     *  @param newOriginalDocument  The document before the write. Must not be null.
     *  @param newPlannedDocument   The document of the planned bytes. Must not be null.
     *  @param newIsExecutable      The executable bit that a created target gets.
     *  @param newIsExisting        True when the target exists before the write.
     *  @param newIsPlannedExisting True when the target exists after the write.
     *  @param newBlobIndex         The index of this file in the transaction. It names the blob file.
     */
    StagedFile (const juce::File& newTargetFile, std::unique_ptr<TextDocument> newOriginalDocument, std::unique_ptr<TextDocument> newPlannedDocument, bool newIsExecutable, bool newIsExisting, bool newIsPlannedExisting, int newBlobIndex);

    /** @brief Returns the file that the transaction writes.
     *
     *  @return The target file.
     */
    const juce::File& getTargetFile() const noexcept;

    /** @brief Returns the path of the file as the user sees it.
     *
     *  @return The display path of the original document.
     */
    std::string_view getDisplayPath() const noexcept;

    /** @brief Returns the bytes of the original document.
     *
     *  @return The original bytes. The view is valid while this staged file lives.
     */
    std::string_view getOriginalBytes() const noexcept;

    /** @brief Returns the bytes that the transaction writes.
     *
     *  For an edit, the bytes come from the diff. For an undo entry, they come from the planned document.
     *
     *  @return The planned bytes. The view is valid while this staged file lives.
     */
    std::string_view getPlannedBytes() const noexcept;

    /** @brief Reports the executable bit that the record stores and that a created target gets.
     *
     *  @return True when the executable bit is set.
     */
    bool isExecutable() const noexcept;

    /** @brief Reports whether the target exists before the transaction writes it.
     *
     *  @return True when the target exists before the write.
     */
    bool isExisting() const noexcept;

    /** @brief Appends the unified diff from the original bytes to the planned bytes.
     *
     *  An undo entry has no diff, so the diff is computed here with Myers.
     *
     *  @param output  The text that receives the unified diff. Nothing is appended when the diff holds no changes.
     */
    void appendUnifiedDiff (std::string& output) const;

    /** @brief Writes the planned bytes, with a swap of the target and its blob when the target can be swapped.
     *
     *  A swappable target is exchanged with its blob, so that the blob keeps the original bytes for the undo.
     *  When the pair cannot be swapped, the original bytes are written to the blob, and the target is then replaced directly.
     *
     *  @param transactionId The transaction whose blob file is used.
     *
     *  @return Status result paired with true when the target may have changed, so that restore is needed.
     */
    std::pair<juce::Result, bool> commit (std::string_view transactionId) const;

    /** @brief Writes the planned bytes of an undo entry, with a swap of the target and its blob when the target can be swapped.
     *
     *  A planned absence removes the target. An existing target that can be swapped is exchanged with its blob. Before the exchange, the blob takes
     *  the metadata of the target and the current time. Otherwise the planned bytes are written to the target.
     *
     *  @param transactionId The transaction whose blob file is used.
     *
     *  @return Status result paired with true when the target may have changed, so that restore is needed.
     */
    std::pair<juce::Result, bool> undo (std::string_view transactionId) const;

    /** @brief Returns the target to its original bytes after a commit or an undo changed it. A target that the transaction removed is restored by restoreRemoved.
     *
     *  A file that the transaction created is deleted, once its planned bytes are checked.
     *  A blob that holds the original bytes is exchanged with the target. A blob that holds neither the planned
     *  bytes nor the original bytes is a failure. Otherwise the original bytes are written to the target, once the
     *  target is checked to hold the planned bytes.
     *
     *  @param transactionId The transaction whose blob file is used.
     *
     *  @return Ok when the target holds the original bytes, otherwise the failure.
     */
    juce::Result restore (std::string_view transactionId) const;

private:
    /** @brief Checks that a file holds the expected bytes.
     *
     *  @param file          The file to read.
     *  @param expectedBytes The bytes that the file must hold.
     *
     *  @return Ok when the bytes match. A file that cannot be read gives the read failure with the display path.
     *          Other bytes give the failure text::Diagnostics::failChecksumMismatch.
     */
    juce::Result getBytesStatus (const juce::File& file, std::string_view expectedBytes) const;

    /** @brief Exchanges the target with a blob file, then checks the bytes of the blob and the target.
     *
     *  After a successful exchange, the blob must hold the original bytes. When it does not, the exchange is
     *  reverted, and the failure is returned with true unless the blob then holds the planned bytes. The
     *  planned bytes of the target are checked after the exchange, and a failure there reports true.
     *
     *  @param blobFile The blob file to exchange with the target.
     *
     *  @return Nullopt when the pair cannot be swapped. Otherwise the status paired with true when the target may have changed.
     */
    std::optional<std::pair<juce::Result, bool>> swap (const juce::File& blobFile) const;

    /** @brief Writes the planned bytes to the target, without a swap.
     *
     *  An existing target must hold the original bytes first. A new target must not exist.
     *  The executable bit is set only for a new target.
     *
     *  @return Status result paired with true when the write may have changed the target.
     */
    std::pair<juce::Result, bool> replace() const;

    /** @brief Deletes the target, once it is checked to hold the original bytes.
     *
     *  @return Status result paired with false.
     */
    std::pair<juce::Result, bool> remove() const;

    /** @brief Restores a target that the transaction removed. An existing file is checked against the original bytes, and a missing file is written with them.
     *
     *  @return Ok when the target holds the original bytes, otherwise the failure.
     */
    juce::Result restoreRemoved() const;

    /** @brief Restores a target that existed before the transaction.
     *
     *  When the blob does not hold the planned bytes, the blob must hold the original bytes, and the blob and the target are exchanged.
     *  The exchange is reverted when the blob then does not hold the planned bytes. Otherwise the target must hold the planned bytes,
     *  and the original bytes are written to it.
     *
     *  @param transactionId The transaction whose blob file is used.
     *
     *  @return Ok when the target holds the original bytes, otherwise the failure.
     */
    juce::Result restoreReplaced (std::string_view transactionId) const;

    /** @brief The file that the transaction writes. */
    juce::File targetFile;

    /** @brief The document before the write. Replace and restore use its bytes as the expected content. */
    std::unique_ptr<TextDocument> originalDocument;

    /** @brief The planned bytes: a LineDiff for an edit, or the planned document of an undo entry. */
    std::variant<LineDiff, std::unique_ptr<TextDocument>> plannedContent;

    /** @brief The executable bit that a created target gets. */
    bool executable { false };

    /** @brief True when the target exists before the write. Restore deletes a target that did not exist. */
    bool existing { true };

    /** @brief True when the target exists after the write. A planned removal sets it to false. */
    bool plannedExisting { true };

    /** @brief The index of this file in the transaction. It names the blob file. */
    int blobIndex { 0 };
};
