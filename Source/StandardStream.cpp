#include "StandardStream.h"
#include <cerrno>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

#ifdef _WIN32
static constexpr size_t maximumTransferSize { 1024 * 1024 };

juce::int64 StandardStream::read (char* destination, size_t maximumSize)
{
    _setmode (input, _O_BINARY);
    return static_cast<juce::int64> (_read (input, destination, static_cast<unsigned int> (std::min (maximumSize, maximumTransferSize))));
}

juce::int64 StandardStream::write (int descriptor, const char* source, size_t size)
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

juce::int64 StandardStream::write (int descriptor, const char* source, size_t size)
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
