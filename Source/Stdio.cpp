#include "Stdio.h"
#include "StandardStream.h"
#include "generated/Generated.h"
#include <cerrno>
#include <cstdarg>

static constexpr size_t maximumChunkSize { 64 * 1024 };

static bool writeStream (int descriptor, std::string_view text)
{
    while (not text.empty())
    {
        const auto written { StandardStream::write (descriptor, text.data(), std::min (text.size(), maximumChunkSize)) };

        if (written <= 0)
            return written < 0 and errno == EPIPE;

        text.remove_prefix (static_cast<size_t> (written));
    }

    return true;
}

bool Stdio::isTerminalInput (const std::optional<std::string>& input)
{
    return not input.has_value() and StandardStream::isTerminal (StandardStream::input);
}

bool Stdio::isTerminalOutput() noexcept
{
    return StandardStream::isTerminal (StandardStream::output);
}

std::string Stdio::getLine (const juce::String& message)
{
    return message.toStdString() + static_cast<char> (Chars::newline);
}

std::string Stdio::getDiagnostic (const juce::String& message)
{
    return getLine (ProjectInfo::projectName + Id::diagnosticSeparator + message);
}

Stdio::Output Stdio::getFailure (const juce::String& message)
{
    return { exitError, {}, getDiagnostic (message) };
}

juce::Result Stdio::write (std::string_view bytes)
{
    if (writeStream (StandardStream::output, bytes))
        return juce::Result::ok();

    return juce::Result::fail (text::Diagnostics::failStandardOutput);
}

int Stdio::write (const Output& output)
{
    const auto& [exitCode, standardOutput, standardError] { output };
    const auto writeResult { write (std::string_view { standardOutput }) };
    writeError (standardError);

    if (writeResult.failed())
    {
        writeError (getDiagnostic (writeResult.getErrorMessage()));
        return exitError;
    }

    return exitCode;
}

void Stdio::writeError (std::string_view text)
{
    juce::ignoreUnused (writeStream (StandardStream::error, text));
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

std::string Stdio::getJoinedOutput (const jam::Array<std::string>& outputs)
{
    size_t totalSize { 0 };

    for (int index { 0 }; index < outputs.size(); ++index)
        totalSize += outputs.at (index).size();

    std::string joined;
    joined.reserve (totalSize);

    for (int index { 0 }; index < outputs.size(); ++index)
        joined.append (outputs.at (index));

    return joined;
}