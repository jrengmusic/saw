#include "Input.h"
#include "StandardStream.h"
#include "Stdio.h"
#include "generated/Generated.h"
#if JUCE_WINDOWS
#include <windows.h>
#endif
#if ! JUCE_WINDOWS
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

static bool isNullDevice (const std::filesystem::path& path)
{
    std::error_code deviceError;
    return std::filesystem::equivalent (path, std::filesystem::u8path (Id::nullDevice), deviceError);
}

#if JUCE_WINDOWS
struct Input::Native
{
};
#else
struct Input::Native
{
    static constexpr int mappingFlags { MAP_PRIVATE | MAP_RESILIENT_MEDIA };

    explicit Native (size_t newSize)
        : address (::mmap (nullptr, newSize, PROT_READ, mappingFlags, STDIN_FILENO, 0)),
          size (newSize)
    {
    }

    ~Native()
    {
        if (address != MAP_FAILED)
            ::munmap (address, size);
    }

    void* address { MAP_FAILED };
    size_t size { 0 };

    JUCE_DECLARE_NON_COPYABLE (Native)
};
#endif

std::unique_ptr<Input> Input::fromFile (const juce::File& file, size_t maxReadSize)
{
    const auto path { std::filesystem::u8path (jam::Strings::getView (file.getFullPathName())) };
    std::error_code sizeError;
    const auto size { static_cast<size_t> (std::filesystem::file_size (path, sizeError)) };
    auto input { std::make_unique<Input> (Access {}) };

    if (sizeError.value() != 0)
    {
        if (isNullDevice (path))
            return input;

        std::error_code fifoError;

        if (maxReadSize != allBytes and std::filesystem::is_fifo (path, fifoError))
        {
            juce::FileInputStream stream { file };

            if (stream.failedToOpen())
                return nullptr;

            if (input->setReadBytes ([&stream] (char* destination, size_t maximumSize)
                                     {
                                         return stream.InputStream::read (destination, maximumSize);
                                     }, allLines))
                return input;
        }

        return nullptr;
    }

    if (size > maxReadSize)
    {
        input->mappedFile = std::make_unique<juce::MemoryMappedFile> (file, juce::MemoryMappedFile::readOnly);

        if (input->mappedFile->getData() != nullptr)
        {
            input->bytes = { static_cast<const char*> (input->mappedFile->getData()), input->mappedFile->getSize() };
            return input;
        }
    }

    juce::FileInputStream stream { file };

    if (stream.failedToOpen())
        return nullptr;

    input->buffer.malloc (size);

    if (static_cast<size_t> (stream.InputStream::read (input->buffer.getData(), size)) != size)
        return nullptr;

    input->bytes = { input->buffer.getData(), size };
    return input;
}

Input::Input (Access) {}

Input::~Input() = default;

std::pair<juce::Result, std::unique_ptr<Input>> Input::create (const Source& source, int numLines)
{
    auto input { std::make_unique<Input> (Access {}) };

    if (std::holds_alternative<std::string_view> (source))
    {
        input->bytes = std::get<std::string_view> (source);
        return { juce::Result::ok(), std::move (input) };
    }

    if (not Stdio::isTerminalInput (source) and not hasData (source))
        return { juce::Result::ok(), std::move (input) };

    if (input->setMappedBytes() or input->setReadBytes (StandardStream::read, numLines))
        return { juce::Result::ok(), std::move (input) };

    return { juce::Result::fail (text::Diagnostics::failStandardInput), nullptr };
}

bool Input::isStream (const Source& source)
{
#if JUCE_WINDOWS
    return std::holds_alternative<StandardInput> (source);
#else
    struct stat status {};
    return std::holds_alternative<StandardInput> (source) and (::fstat (STDIN_FILENO, &status) != 0 or not S_ISREG (status.st_mode));
#endif
}

bool Input::hasData (const Source& source)
{
    if (std::holds_alternative<std::string_view> (source))
        return true;

    if (std::holds_alternative<NullDevice> (source))
        return false;

#if JUCE_WINDOWS
    const auto fileType { GetFileType (GetStdHandle (STD_INPUT_HANDLE)) };
    return fileType == FILE_TYPE_DISK or fileType == FILE_TYPE_PIPE;
#else
    struct stat status {};
    return ::fstat (STDIN_FILENO, &status) == 0 and (S_ISREG (status.st_mode) or S_ISFIFO (status.st_mode) or S_ISSOCK (status.st_mode));
#endif
}

std::string_view Input::getBytes() const noexcept
{
    return bytes;
}

bool Input::setMappedBytes()
{
#if JUCE_WINDOWS
    return false;
#else
    struct stat status {};

    if (::fstat (STDIN_FILENO, &status) != 0 or not S_ISREG (status.st_mode))
        return false;

    const off_t offset { ::lseek (STDIN_FILENO, 0, SEEK_CUR) };

    if (offset < 0 or offset >= status.st_size)
        return offset >= 0;

    const size_t size { static_cast<size_t> (status.st_size) };
    auto mapping { std::make_unique<Native> (size) };

    if (mapping->address == MAP_FAILED)
        return false;

    native = std::move (mapping);
    bytes = std::string_view { static_cast<const char*> (native->address), size }.substr (static_cast<size_t> (offset));
    return ::lseek (STDIN_FILENO, status.st_size, SEEK_SET) == status.st_size;
#endif
}

bool Input::setReadBytes (const std::function<juce::int64 (char*, size_t)>& read, int numLines)
{
    static constexpr size_t initialCapacity { 64 * 1024 };
    size_t capacity { initialCapacity };
    size_t size { 0 };
    int lineCount { 0 };
    buffer.malloc (capacity);

    while (true)
    {
        if (size == capacity)
        {
            capacity *= 2;
            buffer.realloc (capacity);
        }

        const auto numRead { read (buffer.getData() + size, capacity - size) };

        if (numRead == 0)
        {
            bytes = std::string_view { buffer.getData(), size };
            return true;
        }

        if (numRead < 0)
            return false;

        lineCount += static_cast<int> (jam::simd::countByte (buffer.getData() + size, static_cast<size_t> (numRead), static_cast<char> (Chars::newline)));
        size += static_cast<size_t> (numRead);

        if (lineCount >= numLines)
        {
            bytes = std::string_view { buffer.getData(), size };
            return true;
        }
    }
}
