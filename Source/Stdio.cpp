#include "Stdio.h"
#include "generated/Generated.h"
#include <cstdarg>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#else
#include <unistd.h>
#endif

void Stdio::setBinaryOutput() noexcept
{
#ifdef _WIN32
    _setmode (_fileno (stdout), _O_BINARY);
#endif
}

bool Stdio::isTerminalInput() noexcept
{
#ifdef _WIN32
    return _isatty (_fileno (stdin)) != 0;
#else
    return isatty (fileno (stdin)) != 0;
#endif
}

bool Stdio::isTerminalOutput() noexcept
{
#ifdef _WIN32
    return _isatty (_fileno (stdout)) != 0;
#else
    return isatty (fileno (stdout)) != 0;
#endif
}

void Stdio::print (const juce::String& message)
{
    setBinaryOutput();
    printf ("%s\n", message.toRawUTF8());
}

void Stdio::printSummary (const juce::String& message)
{
    print (ProjectInfo::projectName + Id::diagnosticSeparator + message);
}

void Stdio::printError (const juce::String& message)
{
    fprintf (stderr, "%s\n", (ProjectInfo::projectName + Id::diagnosticSeparator + message).toRawUTF8());
}

void Stdio::write (std::string_view text)
{
    setBinaryOutput();
    fwrite (text.data(), sizeof (char), text.size(), stdout);
}

juce::String Stdio::getFormatted (const char* format, ...)
{
    assert (format != nullptr);
    va_list measureArgs;
    va_start (measureArgs, format);
    va_list formatArgs;
    va_copy (formatArgs, measureArgs);
    const int length { std::vsnprintf (nullptr, 0, format, measureArgs) };
    va_end (measureArgs);

    juce::String formatted;

    if (length > 0)
    {
        std::string buffer (static_cast<size_t> (length), '\0');
        std::vsnprintf (buffer.data(), buffer.size() + 1, format, formatArgs);
        formatted = juce::String::fromUTF8 (buffer.data(), length);
    }

    va_end (formatArgs);
    return formatted;
}

std::pair<juce::Result, std::string> Stdio::getInput()
{
#ifdef _WIN32
    _setmode (_fileno (stdin), _O_BINARY);
#endif

    std::string content {};
    std::array<char, BUFSIZ> buffer {};
    auto bytesRead { fread (buffer.data(), 1, buffer.size(), stdin) };

    while (bytesRead > 0)
    {
        content.append (buffer.data(), bytesRead);
        bytesRead = fread (buffer.data(), 1, buffer.size(), stdin);
    }

    if (ferror (stdin) != 0)
        return { juce::Result::fail (text::Diagnostics::failStandardInput), {} };

    return { juce::Result::ok(), std::move (content) };
}
