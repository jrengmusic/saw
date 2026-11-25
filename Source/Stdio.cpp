#include "Stdio.h"
#include "StandardStream.h"

bool Stdio::isTerminalInput (const std::optional<std::string>& input)
{
    return not input.has_value() and StandardStream::isTerminal (StandardStream::input);
}

bool Stdio::isTerminalOutput() noexcept
{
    return StandardStream::isTerminal (StandardStream::output);
}

Stdio::Output Stdio::getOutput (int exitCode, Documents documents, jam::Array<juce::Result> failures)
{
    return { exitCode, std::move (documents), std::move (failures) };
}

Stdio::Documents Stdio::getDocuments (std::string&& text, std::string_view displayPath)
{
    Documents documents;

    if (not text.empty())
        documents.add (TextDocument::fromText (std::move (text), displayPath));

    return documents;
}

void Stdio::addDocuments (Documents& documents, Documents&& addedDocuments)
{
    if (documents.isEmpty())
    {
        documents = std::move (addedDocuments);
    }
    else
    {
        for (auto& document : addedDocuments)
            documents.add (std::move (document));
    }
}

Stdio::Output Stdio::getFailure (const juce::Result& status)
{
    jam::Array<juce::Result> failures;
    failures.add (status);
    return getOutput (exitError, {}, std::move (failures));
}

void Stdio::appendPiece (std::string& message, std::string_view piece)
{
    jam::Format::appendBytes (message, piece);
}

void Stdio::appendPiece (std::string& message, int piece)
{
    jam::Format::appendDecimal (message, piece);
}

void Stdio::appendPiece (std::string& message, juce::juce_wchar piece)
{
    jam::Format::appendUtf8 (message, piece);
}

void Stdio::appendPiece (std::string& message, const juce::Result& piece)
{
    jam::Format::appendBytes (message, jam::Strings::getView (piece.getErrorMessage()));
}

void Stdio::appendPiece (juce::String& message, std::string_view piece)
{
    if (not piece.empty())
        message.appendCharPointer (juce::CharPointer_UTF8 { piece.data() }, juce::CharPointer_UTF8 { piece.data() + piece.size() });
}

void Stdio::appendPiece (juce::String& message, int piece)
{
    message << piece;
}

void Stdio::appendPiece (juce::String& message, juce::juce_wchar piece)
{
    message << juce::String::charToString (piece);
}

void Stdio::appendPiece (juce::String& message, const juce::Result& piece)
{
    message << piece.getErrorMessage();
}

juce::Result Stdio::getFirstFailure (const jam::Array<juce::Result>& statuses)
{
    for (int index { 0 }; index < statuses.size(); ++index)
    {
        if (statuses.at (index).failed())
            return statuses.at (index);
    }

    return juce::Result::ok();
}

int Stdio::write (const Output& output)
{
    const auto& [exitCode, documents, failures] { output };
    StandardStream standardOutput { StandardStream::output };

    for (const auto& document : documents)
    {
        const auto text { document->getBytes() };
        standardOutput.write (text.data(), text.size());
    }

    standardOutput.flush();

    StandardStream standardError { StandardStream::error };
    const auto writeToStandardError { [&standardError] (std::string_view piece)
    {
        standardError.write (piece.data(), piece.size());
    } };

    for (const auto& failure : failures)
        applyToPieces (failure, writeToStandardError);

    const auto& standardOutputStatus { standardOutput.getStatus() };

    if (standardOutputStatus.failed())
        applyToPieces (standardOutputStatus, writeToStandardError);

    standardError.flush();
    return standardOutputStatus.failed() ? exitError : exitCode;
}
