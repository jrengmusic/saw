#include <JuceHeader.h>

static bool writeFrame (const juce::File& frameFile, const std::function<void (juce::OutputStream&)>& writeBytes)
{
    frameFile.deleteFile();
    juce::FileOutputStream stream { frameFile };

    if (stream.openedOk())
    {
        writeBytes (stream);
        stream.flush();
        return stream.getStatus().wasOk();
    }

    return false;
}

static bool writeHelpFrame (const juce::File& frameFile, const juce::String& documentName)
{
    jam::TerminalPage page { { BinaryData::fetcher }, jam::Strings { files::styleSheet } };
    auto document { jam::MarkdownDocument::parse (BinaryData::getString (documentName), documentName) };
    assert (document != nullptr);
    page.setDocument (std::move (*document));
    return writeFrame (frameFile, [&page] (juce::OutputStream& stream)
    {
        page.writeToStream (stream);
    });
}

static bool writeEraseFrame (const juce::File& frameFile)
{
    return writeFrame (frameFile, [] (juce::OutputStream& stream)
    {
        jam::TerminalEncoder::writeEraseScreen (stream);
    });
}

int main (int argc, char* argv[])
{
    static constexpr int folderIndex { 1 };
    static constexpr int eraseFileIndex { 2 };
    static constexpr int firstDocumentIndex { 3 };
    static constexpr const char* frameExtension { ".frame" };
    assert (argc > firstDocumentIndex);

    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    const juce::File folder { juce::String::fromUTF8 (argv[folderIndex]) };
    const juce::File eraseFile { folder.getChildFile (juce::String::fromUTF8 (argv[eraseFileIndex])) };
    bool isWritten { writeEraseFrame (eraseFile) };

    for (int index { firstDocumentIndex }; index < argc; ++index)
    {
        const auto documentName { juce::String::fromUTF8 (argv[index]) };
        const auto frameFile { folder.getChildFile (documentName).withFileExtension (frameExtension) };
        isWritten = writeHelpFrame (frameFile, documentName) and isWritten;
    }

    return isWritten ? EXIT_SUCCESS : EXIT_FAILURE;
}
