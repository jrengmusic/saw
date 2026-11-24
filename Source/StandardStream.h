#pragma once
#include <JuceHeader.h>

/** @brief Descriptors of the process standard streams, with the platform calls on them.
 *
 *  The wrappers keep the platform differences in one place. Callers see one result for
 *  every platform.
 */
struct StandardStream
{
    /** @brief Descriptor of standard input. */
    static constexpr int input { 0 };

    /** @brief Descriptor of standard output. */
    static constexpr int output { 1 };

    /** @brief Descriptor of standard error. */
    static constexpr int error { 2 };

    /** @brief Reads bytes from standard input.
     *
     *  On POSIX, a read that a signal interrupts is retried. On Windows, the stream is set to
     *  binary mode, and one read is limited to a fixed transfer size.
     *
     *  @param destination Buffer that receives the bytes. It must hold at least maximumSize bytes.
     *  @param maximumSize The largest number of bytes to read.
     *
     *  @return The number of bytes read, 0 at the end of the stream, or a negative value on error.
     */
    static juce::int64 read (char* destination, size_t maximumSize);

    /** @brief Writes bytes to a standard stream.
     *
     *  On POSIX, a write that a signal interrupts is retried. On Windows, the stream is set to
     *  binary mode before the write.
     *
     *  @param descriptor The stream to write: output or error.
     *  @param source     The bytes to write.
     *  @param size       The number of bytes to write.
     *
     *  @return The number of bytes written, or a negative value on error.
     */
    static juce::int64 write (int descriptor, const char* source, size_t size);

    /** @brief Checks whether a descriptor is connected to an interactive terminal.
     *
     *  @param descriptor The descriptor to test.
     *
     *  @return True when the descriptor is a TTY.
     */
    static bool isTerminal (int descriptor) noexcept;
};
