#include "Writer.h"
#include "Stdio.h"
#include "generated/Generated.h"

static juce::Result setMetadata (const juce::File& file, const juce::File& source)
{
    if (jam::File::setMetadata (file, source))
        return juce::Result::ok();

    return Stdio::fail (text::Diagnostics::failOverwriteFailed);
}

static juce::Result setBytes (const juce::File& file, const std::function<bool (juce::OutputStream&)>& writeBytes)
{
    juce::FileOutputStream stream { file };

    if (stream.failedToOpen())
        return Stdio::fail (text::Diagnostics::failOverwriteFailed);

    const bool isWritten { writeBytes (stream) };
    stream.flush();

    if (stream.getStatus().failed())
        return stream.getStatus();

    if (not isWritten or file.getSize() != stream.getPosition())
        return Stdio::fail (text::Diagnostics::failOverwriteFailed);

    return juce::Result::ok();
}

static juce::Result setPlannedFile (const juce::File& file, const juce::File& targetFile, std::string_view bytes)
{
    const auto writeStatus { Writer::toNewFile (file, bytes) };

    if (writeStatus.failed())
        return writeStatus;

    return setMetadata (file, targetFile);
}

juce::Result Writer::toNewFile (const juce::File& newFile, std::string_view bytes)
{
    jassert (newFile.getSize() == 0);
    return setBytes (newFile, [bytes] (juce::OutputStream& stream)
    {
        static constexpr size_t maximumChunkSize { std::numeric_limits<int>::max() };
        auto remaining { bytes };

        while (not remaining.empty())
        {
            const auto chunk { remaining.substr (0, maximumChunkSize) };

            if (not stream.write (chunk.data(), chunk.size()))
                return false;

            remaining.remove_prefix (chunk.size());
        }

        return true;
    });
}

juce::Result Writer::toNewFile (const juce::File& newFile, const std::function<bool (juce::OutputStream&)>& writeBytes)
{
    jassert (newFile.getSize() == 0);
    return setBytes (newFile, writeBytes);
}

juce::Result Writer::toFile (const juce::File& targetFile, std::string_view bytes)
{
    const juce::TemporaryFile temp { targetFile, juce::TemporaryFile::useHiddenFile };
    const auto writeStatus { toNewFile (temp.getFile(), bytes) };

    if (writeStatus.failed())
        return writeStatus;

    if (targetFile.existsAsFile())
    {
        const auto metadataStatus { setMetadata (temp.getFile(), targetFile) };

        if (metadataStatus.failed())
            return metadataStatus;
    }

    if (not jam::File::replaceFileIn (temp.getFile(), targetFile))
        return Stdio::fail (text::Diagnostics::failOverwriteFailed);

    return juce::Result::ok();
}

juce::Result Writer::toStagedFile (const juce::File& stagedFile, const juce::File& targetFile, std::string_view bytes)
{
    if (jam::File::hasSameGroup (stagedFile.getParentDirectory(), targetFile.getParentDirectory()))
        return setPlannedFile (stagedFile, targetFile, bytes);

    const juce::TemporaryFile temp { targetFile, juce::TemporaryFile::useHiddenFile };
    const auto writeStatus { setPlannedFile (temp.getFile(), targetFile, bytes) };

    if (writeStatus.failed())
        return writeStatus;

    if (not jam::File::moveFileTo (temp.getFile(), stagedFile))
        return Stdio::fail (text::Diagnostics::failOverwriteFailed);

    return juce::Result::ok();
}

juce::Result Writer::getWriteFailure (std::string_view displayPath, std::string_view reason)
{
    return Stdio::fail (text::Diagnostics::failWrite,
                        displayPath,
                        jam::Strings::getView (Id::diagnosticSeparator),
                        reason,
                        text::Diagnostics::rolledBack);
}

juce::Result Writer::getWriteStatus (const juce::File& file, std::string_view displayPath)
{
    if (file.existsAsFile())
    {
        std::error_code linkError;
        const auto numLinks { std::filesystem::hard_link_count (std::filesystem::u8path (jam::Strings::getView (file.getFullPathName())), linkError) };

        if (linkError.value() != 0)
            return Stdio::fail (text::Diagnostics::failRead, displayPath);

        if (numLinks > 1)
            return Stdio::fail (text::Diagnostics::failHardLinkedFile, displayPath);

        if (not file.hasWriteAccess())
            return Stdio::fail (text::Diagnostics::failReadOnlyFile, displayPath);

        if (file.getParentDirectory().hasWriteAccess() and not jam::File::hasDeleteAccess (file))
            return getWriteFailure (displayPath, text::Diagnostics::failOverwriteFailed);
    }

    return juce::Result::ok();
}
