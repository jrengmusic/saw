#pragma once
#include <JuceHeader.h>

/** @brief Writes files atomically through hidden temporary files and checks executable bits.
 */
struct Writer
{
    /** @brief Writes @p bytes to a hidden temporary file, sets permissions, and replaces @p targetFile.
     *
     *  @param targetFile   Destination file to create or overwrite atomically.
     *  @param bytes        Raw bytes to write.
     *  @param isExecutable True to apply executable permission to the written file.
     *
     *  @return Result indicating write and atomic rename success or failure.
     */
    static juce::Result toFile (const juce::File& targetFile,
                                std::string_view bytes,
                                bool isExecutable);

    /** @brief Reports whether any POSIX execute bit is set on @p file.
     *
     *  @param file File to inspect.
     *
     *  @return True on POSIX when user, group, or other execute permission is set; false on Windows.
     */
    static bool isExecutable (const juce::File& file) noexcept;
};
