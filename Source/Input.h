#pragma once
#include <JuceHeader.h>

/** @brief Bytes of one source behind one view: a mapped file, a buffer read into memory, or a supplied text.
 *
 *  The Input owns each mapping and each buffer that it creates. The standard-input mapping is held by
 *  native, which unmaps it when the Input is destroyed. A mapped file is held by mappedFile, which unmaps
 *  it. A buffer is held by buffer. A supplied text is not owned, so the view refers to the caller's text.
 */
class Input
{
    /** @brief Tag type that only the members of Input can name, so that only they construct an Input. */
    struct Access {};

public:
    /** @brief Source that selects the process standard input. */
    struct StandardInput {};

    /** @brief Source that selects the null device, which holds no data. */
    struct NullDevice {};

    /** @brief The source of the bytes of a command: the process standard input, the null device, or a supplied text. */
    using Source = std::variant<StandardInput, NullDevice, std::string_view>;

    /** @brief Line count that reads the whole input. */
    static constexpr int allLines { std::numeric_limits<int>::max() };

    /** @brief Byte limit that makes fromFile read every file into an owned buffer.
     *
     *  No file is larger than this value, so no file is mapped.
     */
    static constexpr size_t allBytes { std::numeric_limits<size_t>::max() };

    /** @brief Largest file size that fromFile reads into a buffer.
     *
     *  A larger file is mapped. When the map fails, the file is read into a buffer.
     */
    static constexpr size_t maxBufferedReadSize { 256 * 1024 };

    /** @brief Creates an empty Input. Only the members of Input can name Access, so only they construct one. */
    explicit Input (Access);

    /** @brief Destroys the Input. The members release the mappings and the buffer.
     *
     *  The destructor is defaulted. native unmaps the standard input, and mappedFile unmaps a mapped file.
     */
    ~Input();

    /** @brief Creates the input of a command from its source.
     *
     *  A supplied text is used as it is. The Input holds a view on it, so the text must outlive the Input.
     *  For the null device, or for standard input that holds no data and is not a terminal, the Input is empty and nothing is read.
     *  Otherwise a regular standard input is mapped from its current offset. An offset at or past the end gives an empty input.
     *  When the map fails, or standard input is not a regular file, standard input is read instead. Standard input that is read
     *  stops at numLines newlines or the end. A mapped standard input is not limited by numLines.
     *
     *  @param source   The standard input, the null device, or a supplied text.
     *  @param numLines The number of newlines to read from standard input that is read. allLines reads to the end.
     *
     *  @return Status result paired with the Input, or nullptr when standard input cannot be read.
     */
    static std::pair<juce::Result, std::unique_ptr<Input>> create (const Source& source, int numLines);

    /** @brief Tests whether a source can hold data.
     *
     *  A supplied text always holds data, and the null device never does. Standard input holds data when it is a disk file, a pipe,
     *  or a socket on POSIX, or a disk file or a pipe on Windows.
     *
     *  @param source The standard input, the null device, or a supplied text.
     *
     *  @return True when the source can hold data.
     */
    static bool hasData (const Source& source);

    /** @brief Opens a file as an Input.
     *
     *  The size comes from std::filesystem::file_size. When that call fails, the null device gives an empty Input. A named pipe is read
     *  through a stream when maxReadSize is not allBytes. Any other path that fails the size call, such as a socket, a device, or a folder, gives nullptr.
     *  A file larger than maxReadSize is mapped. When the map fails, the file is read into an owned buffer. A smaller file is read into an owned buffer,
     *  and a read that returns fewer bytes than the size gives nullptr. Passing allBytes always reads into an owned buffer, so a later write to the file cannot change the bytes.
     *
     *  @param file        The file to open.
     *  @param maxReadSize The largest file size that is read into a buffer.
     *
     *  @return The Input, or nullptr when the file cannot be opened or read.
     */
    static std::unique_ptr<Input> fromFile (const juce::File& file, size_t maxReadSize);

    /** @brief Checks whether the input of a command is a stream, not a regular file.
     *
     *  On Windows, the process standard input is a stream. On other platforms, the process standard input is a stream when its status
     *  is not a regular file, or when its status cannot be read. A supplied text and the null device are never a stream.
     *
     *  @param source The standard input, the null device, or a supplied text.
     *
     *  @return True when the input is a stream.
     */
    static bool isStream (const Source& source);

    /** @brief Returns the bytes of the input.
     *
     *  @return A view on the bytes. The view is valid while this Input lives, and, for a supplied
     *          source, while that source lives.
     */
    std::string_view getBytes() const noexcept;

private:
    /** @brief Owns the memory mapping of the standard input on POSIX platforms.
     *
     *  The constructor maps the standard input with mmap, and the destructor unmaps it with munmap.
     *  On Windows, Native is empty, because standard input is never mapped there.
     */
    struct Native;

    /** @brief Maps the standard input from its current offset to its end, when standard input is a regular file.
     *
     *  @return True when the mapping gives the bytes, or when the offset is at or past the end, which gives
     *          an empty input. False when standard input is not a regular file or the map fails, so the
     *          caller reads it instead.
     */
    bool setMappedBytes();

    /** @brief Reads standard input into the buffer, until numLines newlines are read or the end is reached.
     *
     *  @param read      The function that fills the destination with up to the given size of bytes. It returns the count read, zero at the end, or a negative value on failure.
     *  @param numLines The number of newlines that ends the read. allLines reads to the end.
     *
     *  @return True when the read ends at the end of the input or at numLines, false when a read fails.
     */
    bool setReadBytes (const std::function<juce::int64 (char*, size_t)>& read, int numLines);

    /** @brief Holds the bytes read into memory, for a file that is read and for standard input that is read. */
    juce::HeapBlock<char> buffer;

    /** @brief Holds the mapping of a file that is larger than the read limit, and unmaps it on destruction. */
    std::unique_ptr<juce::MemoryMappedFile> mappedFile;

    /** @brief Holds the mapping of standard input on POSIX platforms, and unmaps it on destruction. */
    std::unique_ptr<Native> native;

    /** @brief View of the bytes of the input. The owner is buffer, mappedFile, native, or the caller's text. */
    std::string_view bytes;

    JUCE_DECLARE_NON_COPYABLE (Input)
};
