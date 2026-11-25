#include "StandardStream.h"
#include "generated/Generated.h"
#include <cerrno>
#if JUCE_WINDOWS
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

StandardStream::StandardStream (int newDescriptor)
    : descriptor (newDescriptor)
{
}

StandardStream::~StandardStream()
{
    flush();
}

const juce::Result& StandardStream::getStatus() const noexcept
{
    return status;
}

void StandardStream::flush()
{
    writeAll (std::string_view { buffer, numInBuffer });
    numInBuffer = 0;
}

juce::int64 StandardStream::getPosition()
{
    jassertfalse;
    return 0;
}

bool StandardStream::setPosition (juce::int64)
{
    jassertfalse;
    return false;
}

bool StandardStream::write (const void* dataToWrite, size_t numberOfBytes)
{
    const auto* source { static_cast<const char*> (dataToWrite) };

    if (numberOfBytes <= bufferSize - numInBuffer)
    {
        std::memcpy (buffer + numInBuffer, source, numberOfBytes);
        numInBuffer += numberOfBytes;
    }
    else
    {
        flush();

        if (numberOfBytes < bufferSize)
        {
            std::memcpy (buffer + numInBuffer, source, numberOfBytes);
            numInBuffer += numberOfBytes;
        }
        else
        {
            writeAll (std::string_view { source, numberOfBytes });
        }
    }

    return not isClosed;
}

void StandardStream::writeAll (std::string_view text)
{
    while (not isClosed and not text.empty())
    {
        const auto written { writeBytes (text.data(), std::min (text.size(), maximumChunkSize)) };

        if (written > 0)
        {
            text.remove_prefix (static_cast<size_t> (written));
        }
        else
        {
            isClosed = true;

            if (written == 0 or errno != EPIPE)
                status = juce::Result::fail (text::Diagnostics::failStandardOutput);
        }
    }
}

#if JUCE_WINDOWS
juce::int64 StandardStream::read (char* destination, size_t maximumSize)
{
    static constexpr size_t maximumTransferSize { 1024 * 1024 };

    _setmode (input, _O_BINARY);
    return static_cast<juce::int64> (_read (input, destination, static_cast<unsigned int> (std::min (maximumSize, maximumTransferSize))));
}

juce::int64 StandardStream::writeBytes (const char* source, size_t size)
{
    _setmode (descriptor, _O_BINARY);
    return static_cast<juce::int64> (_write (descriptor, source, static_cast<unsigned int> (size)));
}

bool StandardStream::isTerminal (int descriptor) noexcept
{
    return _isatty (descriptor) != 0;
}
#else
juce::int64 StandardStream::read (char* destination, size_t maximumSize)
{
    auto numRead { ::read (input, destination, maximumSize) };

    while (numRead < 0 and errno == EINTR)
        numRead = ::read (input, destination, maximumSize);

    return static_cast<juce::int64> (numRead);
}

juce::int64 StandardStream::writeBytes (const char* source, size_t size)
{
    auto numWritten { ::write (descriptor, source, size) };

    while (numWritten < 0 and errno == EINTR)
        numWritten = ::write (descriptor, source, size);

    return static_cast<juce::int64> (numWritten);
}

bool StandardStream::isTerminal (int descriptor) noexcept
{
    return ::isatty (descriptor) != 0;
}
#endif
