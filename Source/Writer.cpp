#include "Writer.h"
#include "Stdio.h"
#include "generated/Generated.h"
#include <filesystem>
#if ! JUCE_WINDOWS
#include <sys/stat.h>
#endif

static juce::Result setMetadata (const juce::File& file, const juce::File& source)
{
    if (jam::File::setMetadata (file, source))
        return juce::Result::ok();

    return juce::Result::fail (text::Diagnostics::failOverwriteFailed);
}

static juce::Result setBytes (const juce::File& file, std::string_view bytes)
{
    static constexpr size_t maximumChunkSize { std::numeric_limits<int>::max() };
    juce::FileOutputStream stream { file };

    if (stream.failedToOpen())
        return juce::Result::fail (text::Diagnostics::failOverwriteFailed);

    auto remaining { bytes };

    while (not remaining.empty())
    {
        const auto chunk { remaining.substr (0, maximumChunkSize) };

        if (not stream.write (chunk.data(), chunk.size()))
            return juce::Result::fail (stream.getStatus().getErrorMessage());

        remaining.remove_prefix (chunk.size());
    }

    stream.flush();
    return stream.getStatus();
}

static juce::Result setPlannedFile (const juce::File& file, const juce::File& targetFile, std::string_view bytes)
{
    const auto writeStatus { setBytes (file, bytes) };

    if (writeStatus.failed())
        return writeStatus;

    return setMetadata (file, targetFile);
}

juce::Result Writer::toNewFile (const juce::File& newFile, std::string_view bytes)
{
    jassert (newFile.getSize() == 0);
    return setBytes (newFile, bytes);
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

#if JUCE_WINDOWS
    const bool isReplaced { temp.overwriteTargetFileWithTemporary() };
#else
    const bool isReplaced { jam::File::moveFileTo (temp.getFile(), targetFile) };
#endif

    if (not isReplaced)
        return juce::Result::fail (text::Diagnostics::failOverwriteFailed);

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
        return juce::Result::fail (text::Diagnostics::failOverwriteFailed);

    return juce::Result::ok();
}

juce::Result Writer::getWriteStatus (const juce::File& file, const juce::String& displayPath)
{
    if (file.existsAsFile())
    {
        std::error_code linkError;
        const auto numLinks { std::filesystem::hard_link_count (std::filesystem::u8path (file.getFullPathName().toStdString()), linkError) };

        if (linkError.value() != 0)
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failRead, displayPath.toRawUTF8()));

        if (numLinks > 1)
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failHardLinkedFile, displayPath.toRawUTF8()));

        if (not file.hasWriteAccess())
            return juce::Result::fail (Stdio::getFormatted (text::Diagnostics::failReadOnlyFile, displayPath.toRawUTF8()));
    }

    return juce::Result::ok();
}

bool Writer::isExecutable (const juce::File& file) noexcept
{
#if ! JUCE_WINDOWS
    struct stat info {};
    return stat (file.getFullPathName().toRawUTF8(), &info) == 0
           and (info.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
#else
    juce::ignoreUnused (file);
    return false;
#endif
}
