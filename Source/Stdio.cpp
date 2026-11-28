#include "Stdio.h"
#include "StandardStream.h"
#include "PageRanges.h"

bool Stdio::isTerminalInput (const Input::Source& input)
{
    return std::holds_alternative<Input::StandardInput> (input) and StandardStream::isTerminal (StandardStream::input);
}

bool Stdio::isTerminalOutput() noexcept
{
    return StandardStream::isTerminal (StandardStream::output);
}

std::string Stdio::getVersionLine()
{
    std::string line;
    jam::Format::appendBytes (line, ProjectInfo::projectName);
    jam::Format::appendUtf8 (line, Chars::space);
    jam::Format::appendBytes (line, ProjectInfo::versionString);
    jam::Format::appendUtf8 (line, Chars::space);
    jam::Format::appendUtf8 (line, Chars::openParen);
    jam::Format::appendBytes (line, CAST_COMMIT);
    jam::Format::appendUtf8 (line, Chars::closeParen);
    jam::Format::appendUtf8 (line, Chars::newline);
    return line;
}

static size_t getKeptLength (std::string_view bytes, size_t remainingBytes)
{
    size_t keptLength { 0 };
    size_t newlineIndex { jam::simd::findByte (bytes.data(), bytes.size(), 0, static_cast<char> (Chars::newline)) };

    while (newlineIndex < remainingBytes)
    {
        keptLength = newlineIndex + 1;
        newlineIndex = jam::simd::findByte (bytes.data(), bytes.size(), keptLength, static_cast<char> (Chars::newline));
    }

    return keptLength;
}

std::tuple<Stdio::Output, size_t, int, int> Stdio::getLimitedOutput (Output output, size_t remainingBytesBefore, int printedLinesBefore)
{
    auto& [exitCode, documents, failures, errorDocuments] { output };
    juce::ignoreUnused (exitCode, failures, errorDocuments);
    Documents limitedDocuments;
    size_t remainingBytes { remainingBytesBefore };
    int printedLines { printedLinesBefore };
    int omittedLines { 0 };

    for (auto& document : documents)
    {
        const auto bytes { document->getBytes() };
        const auto keptLength { bytes.size() <= remainingBytes ? bytes.size() : getKeptLength (bytes, remainingBytes) };
        printedLines += PageRanges::getLineCount (bytes.substr (0, keptLength));
        omittedLines += PageRanges::getLineCount (bytes.substr (keptLength));
        remainingBytes = keptLength == bytes.size() ? remainingBytes - keptLength : 0;

        if (keptLength > 0)
            limitedDocuments.add (keptLength == bytes.size() ? std::move (document) : TextDocument::fromDocument (std::move (document), bytes.substr (0, keptLength)));
    }

    if (omittedLines > 0)
        limitedDocuments.add (TextDocument::fromText (getDiagnostic (text::Diagnostics::summaryOutputLimit, printedLines, text::Diagnostics::linesPrinted, omittedLines, text::Diagnostics::omittedToPrintAll), {}));

    documents = std::move (limitedDocuments);
    return { std::move (output), remainingBytes, printedLines, omittedLines };
}

Stdio::Output Stdio::getOutput (int exitCode, Documents documents, jam::Array<juce::Result> failures, Documents errorDocuments)
{
    return { exitCode, std::move (documents), std::move (failures), std::move (errorDocuments) };
}

Stdio::Documents Stdio::getDocuments (std::string&& text)
{
    Documents documents;

    if (not text.empty())
        documents.add (TextDocument::fromText (std::move (text), {}));

    return documents;
}

Stdio::Documents Stdio::getDocuments (juce::MemoryBlock&& block)
{
    Documents documents;

    if (not block.isEmpty())
        documents.add (TextDocument::fromBlock (std::move (block), {}));

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
    const auto& [exitCode, documents, failures, errorDocuments] { output };
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

    for (const auto& errorDocument : errorDocuments)
        writeToStandardError (errorDocument->getBytes());

    for (const auto& failure : failures)
        applyToPieces (failure, writeToStandardError);

    const auto& standardOutputStatus { standardOutput.getStatus() };

    if (standardOutputStatus.failed())
        applyToPieces (standardOutputStatus, writeToStandardError);

    standardError.flush();
    return standardOutputStatus.failed() ? exitError : exitCode;
}
