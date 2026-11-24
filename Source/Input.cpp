#include "Input.h"
#include "StandardStream.h"
#include "generated/Generated.h"
#ifndef _WIN32
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

static constexpr int mappingFlags { MAP_PRIVATE | MAP_RESILIENT_MEDIA };
#endif

static bool isNullDevice (const std::filesystem::path& path)
{
    std::error_code deviceError;
    return std::filesystem::equivalent (path, std::filesystem::u8path (Id::nullDevice), deviceError);
}

std::unique_ptr<Input> Input::fromFile (const juce::File& file, size_t maxReadSize)
{
    const auto path { std::filesystem::u8path (file.getFullPathName().toRawUTF8()) };
    std::error_code sizeError;
    const auto size { static_cast<size_t> (std::filesystem::file_size (path, sizeError)) };
    auto input { std::make_unique<Input> (Access {}) };

    if (sizeError.value() != 0)
    {
        if (isNullDevice (path))
            return input;

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

Input::~Input()
{
#ifndef _WIN32
    if (mappedAddress != nullptr)
        ::munmap (mappedAddress, mappedSize);
#endif
}

std::pair<juce::Result, std::unique_ptr<Input>> Input::create (const std::optional<std::string>& source, int numLines)
{
    auto input { std::make_unique<Input> (Access {}) };

    if (source.has_value())
    {
        input->bytes = *source;
        return { juce::Result::ok(), std::move (input) };
    }

    if (input->setMappedBytes() or input->setReadBytes (numLines))
        return { juce::Result::ok(), std::move (input) };

    return { juce::Result::fail (text::Diagnostics::failStandardInput), nullptr };
}

bool Input::isStream (const std::optional<std::string>& source)
{
#ifdef _WIN32
    return not source.has_value();
#else
    struct stat status {};
    return not source.has_value() and (::fstat (STDIN_FILENO, &status) != 0 or not S_ISREG (status.st_mode));
#endif
}

std::string_view Input::getBytes() const noexcept
{
    return bytes;
}

bool Input::setMappedBytes()
{
#ifdef _WIN32
    return false;
#else
    struct stat status {};

    if (::fstat (STDIN_FILENO, &status) != 0 or not S_ISREG (status.st_mode))
        return false;

    const off_t offset { ::lseek (STDIN_FILENO, 0, SEEK_CUR) };

    if (offset < 0 or offset >= status.st_size)
        return offset >= 0;

    const size_t size { static_cast<size_t> (status.st_size) };
    void* const address { ::mmap (nullptr, size, PROT_READ, mappingFlags, STDIN_FILENO, 0) };

    if (address == MAP_FAILED)
        return false;

    mappedAddress = address;
    mappedSize = size;
    bytes = std::string_view { static_cast<const char*> (address), size }.substr (static_cast<size_t> (offset));
    return ::lseek (STDIN_FILENO, status.st_size, SEEK_SET) == status.st_size;
#endif
}

bool Input::setReadBytes (int numLines)
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

        const auto numRead { StandardStream::read (buffer.getData() + size, capacity - size) };

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
