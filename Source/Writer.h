#pragma once
#include <JuceHeader.h>

/** @brief Writes files through hidden temporary files, and checks that a file can be replaced.
 */
struct Writer
{
    /** @brief Replaces a file with new bytes, through a hidden temporary file in the same folder.
     *
     *  When the target exists, the temporary file takes the metadata of the target. The final
     *  step is one replace, so a reader sees the old bytes or the new bytes.
     *
     *  @param targetFile Destination file to create or overwrite.
     *  @param bytes      Raw bytes to write.
     *
     *  @return Result indicating write and replace success or failure.
     */
    static juce::Result toFile (const juce::File& targetFile, std::string_view bytes);

    /** @brief Writes bytes to a file that is absent or empty.
     *
     *  @param newFile The file to write. It must be absent or empty.
     *  @param bytes   Raw bytes to write.
     *
     *  @return Result indicating write success or failure.
     */
    static juce::Result toNewFile (const juce::File& newFile, std::string_view bytes);

    /** @brief Writes a file that is absent or empty through a stream that the function fills.
     *
     *  The function returns false when a write fails. The size of the file is compared with the stream
     *  position after the flush, because a short write of the OS leaves the status of juce::FileOutputStream
     *  ok, and its flush ignores the buffered write.
     *
     *  @param newFile    The file to write. It must be absent or empty.
     *  @param writeBytes Writes the bytes to the stream. Returns false when a write fails.
     *
     *  @return Result indicating write success or failure.
     */
    static juce::Result toNewFile (const juce::File& newFile, const std::function<bool (juce::OutputStream&)>& writeBytes);

    /** @brief Writes the planned bytes to a staged file, with the metadata of the target, ready for an exchange.
     *
     *  When the staging folder has the same group as the target folder, the staged file is written
     *  directly. Otherwise a temporary file is written in the folder of the target, so it takes the
     *  group of that folder, and then it is moved to the staged file.
     *
     *  @param stagedFile  The file to write in the staging folder.
     *  @param targetFile  The file that the staged file will be exchanged with. Its metadata is copied.
     *  @param bytes       The planned bytes to write.
     *
     *  @return Result indicating write success or failure.
     */
    static juce::Result toStagedFile (const juce::File& stagedFile, const juce::File& targetFile, std::string_view bytes);

    /** @brief Builds the failure of a write that did not complete.
     *
     *  @param displayPath The path of the file, as the user sees it.
     *  @param reason      The reason that the write failed.
     *
     *  @return Failure that names the path and the reason, and ends with the rolled-back text (text::Diagnostics::rolledBack).
     */
    static juce::Result getWriteFailure (std::string_view displayPath, std::string_view reason);

    /** @brief Checks that a file can be replaced by a write.
     *
     *  A file that does not exist passes. A file with more than one hard link, or without write
     *  access, is refused. A file that cannot be deleted from a writable folder is refused too, because
     *  the write replaces the file. A file whose link count cannot be read is refused with the read failure.
     *
     *  @param file        The file to check.
     *  @param displayPath Path string used in the diagnostic.
     *
     *  @return Ok when the file can be written, otherwise a failure that names the file.
     */
    static juce::Result getWriteStatus (const juce::File& file, std::string_view displayPath);
};
