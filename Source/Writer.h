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

    /** @brief Checks that a file can be replaced by a write.
     *
     *  A file that does not exist passes. A file with more than one hard link, or without write
     *  access, is refused.
     *
     *  @param file        The file to check.
     *  @param displayPath Path string used in the diagnostic.
     *
     *  @return Ok when the file can be written, otherwise a failure that names the file.
     */
    static juce::Result getWriteStatus (const juce::File& file, const juce::String& displayPath);

    /** @brief Reports whether any POSIX execute bit is set on @p file.
     *
     *  @param file File to inspect.
     *
     *  @return True on POSIX when user, group, or other execute permission is set; false on Windows.
     */
    static bool isExecutable (const juce::File& file) noexcept;
};
