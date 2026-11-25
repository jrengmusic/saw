#pragma once
#include <JuceHeader.h>

/** @brief juce::OutputStream over one descriptor, buffered for small writes.
 *
 *  The platform calls stay in this class, so callers see one result on every platform. The
 *  stream is not seekable.
 */
class StandardStream : public juce::OutputStream
{
public:
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

    /** @brief Checks whether a descriptor is connected to an interactive terminal.
     *
     *  @param descriptor The descriptor to test.
     *
     *  @return True when the descriptor is a TTY.
     */
    static bool isTerminal (int descriptor) noexcept;

    /** @brief Binds the stream to one descriptor, with an empty buffer.
     *
     *  @param newDescriptor The open descriptor that receives the bytes.
     */
    explicit StandardStream (int newDescriptor);

    /** @brief Writes the buffered bytes before the stream is destroyed, so no buffered output is lost. */
    ~StandardStream() override;

    /** @brief Returns the status of the stream.
     *
     *  The status is set by the first write that failed. A closed pipe ends the output and leaves
     *  the status ok.
     *
     *  @return The status of the stream, ok while no write has failed.
     */
    const juce::Result& getStatus() const noexcept;

    /** @brief Writes the buffered bytes to the descriptor. */
    void flush() override;

    /** @brief Not supported: the descriptor is not seekable. The call asserts in debug builds.
     *
     *  @return Always 0.
     */
    juce::int64 getPosition() override;

    /** @brief Not supported: the descriptor is not seekable. The call asserts in debug builds.
     *
     *  @return Always false.
     */
    bool setPosition (juce::int64) override;

    /** @brief Buffers a write that fits, and sends a larger one straight to the descriptor after a flush.
     *
     *  A write that does not fit in the free space flushes the buffer first. It is then buffered when it is
     *  smaller than bufferSize, so that large text is not copied on its way to the descriptor.
     *
     *  @param dataToWrite   The bytes to write.
     *  @param numberOfBytes The number of bytes in dataToWrite.
     *
     *  @return True while the stream accepts writes, false once the stream is closed.
     */
    bool write (const void* dataToWrite, size_t numberOfBytes) override;

private:
    /** @brief Size of the buffer that holds small writes until the next flush. */
    static constexpr size_t bufferSize { 64 * 1024 };

    /** @brief Largest number of bytes passed to one platform write call. */
    static constexpr size_t maximumChunkSize { 64 * 1024 };

    /** @brief Writes one chunk to the descriptor, and retries the call when a signal interrupts it.
     *
     *  @param source The first byte of the chunk.
     *  @param size   The number of bytes in the chunk.
     *
     *  @return The number of bytes written, 0 when the descriptor took none, or a negative value on error. A closed pipe is an error with errno EPIPE.
     */
    juce::int64 writeBytes (const char* source, size_t size);

    /** @brief Writes all of the text, and marks the stream closed after a write that did not complete.
     *
     *  @param text The bytes to write.
     */
    void writeAll (std::string_view text);

    /** @brief The descriptor that receives the bytes. */
    const int descriptor;

    /** @brief Holds small writes, so that one flush makes one platform call. */
    juce::HeapBlock<char> buffer { bufferSize };

    /** @brief Number of bytes held in buffer. */
    size_t numInBuffer { 0 };

    /** @brief Set when the pipe is closed or a write failed, so that later writes do nothing. */
    bool isClosed { false };

    /** @brief The first failure of the stream, or ok. */
    juce::Result status { juce::Result::ok() };

    JUCE_DECLARE_NON_COPYABLE (StandardStream)
};
