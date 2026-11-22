#include "Writer.h"
#include "generated/Generated.h"
#if ! JUCE_WINDOWS
#include <sys/stat.h>
#endif

juce::Result Writer::toFile (const juce::File& targetFile,
                            std::string_view bytes,
                            bool isExecutable)
{
    const juce::TemporaryFile temp { targetFile, juce::TemporaryFile::useHiddenFile };

    {
        juce::FileOutputStream stream { temp.getFile() };

        if (stream.failedToOpen()
            or (not bytes.empty() and not stream.write (bytes.data(), bytes.size())))
            return juce::Result::fail (stream.getStatus().getErrorMessage());

        stream.flush();

        if (stream.getStatus().failed())
            return stream.getStatus();
    }

    temp.getFile().setExecutePermission (isExecutable);

    if (not temp.overwriteTargetFileWithTemporary())
        return juce::Result::fail (text::Diagnostics::failOverwriteFailed);

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
