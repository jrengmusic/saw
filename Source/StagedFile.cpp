#include "StagedFile.h"
#include "Stdio.h"
#include "UndoCache.h"
#include "Writer.h"
#include "generated/Generated.h"

StagedFile::StagedFile (const juce::File& newTargetFile, std::unique_ptr<TextDocument> newOriginalDocument, LineDiff newDiff, int newBlobIndex)
    : targetFile (newTargetFile),
      originalDocument (std::move (newOriginalDocument)),
      plannedContent (std::move (newDiff)),
      executable (jam::File::isExecutable (newTargetFile)),
      blobIndex (newBlobIndex)
{
    assert (originalDocument != nullptr);
}

StagedFile::StagedFile (const juce::File& newTargetFile, std::unique_ptr<TextDocument> newOriginalDocument, std::unique_ptr<TextDocument> newPlannedDocument, bool newIsExecutable, bool newIsExisting, bool newIsPlannedExisting, int newBlobIndex)
    : targetFile (newTargetFile),
      originalDocument (std::move (newOriginalDocument)),
      plannedContent (std::move (newPlannedDocument)),
      executable (newIsExecutable),
      existing (newIsExisting),
      plannedExisting (newIsPlannedExisting),
      blobIndex (newBlobIndex)
{
    const auto* plannedDocument { std::get_if<std::unique_ptr<TextDocument>> (&plannedContent) };
    assert (originalDocument != nullptr);
    assert (plannedDocument != nullptr and *plannedDocument != nullptr);
}

const juce::File& StagedFile::getTargetFile() const noexcept
{
    return targetFile;
}

std::string_view StagedFile::getDisplayPath() const noexcept
{
    return originalDocument->getDisplayPath();
}

std::string_view StagedFile::getOriginalBytes() const noexcept
{
    return originalDocument->getBytes();
}

std::string_view StagedFile::getPlannedBytes() const noexcept
{
    if (const auto* plannedDiff { std::get_if<LineDiff> (&plannedContent) })
        return plannedDiff->getPlannedBytes();

    const auto& plannedDocument { std::get<std::unique_ptr<TextDocument>> (plannedContent) };
    return plannedDocument->getBytes();
}

bool StagedFile::isExecutable() const noexcept
{
    return executable;
}

bool StagedFile::isExisting() const noexcept
{
    return existing;
}

void StagedFile::appendUnifiedDiff (std::string& output) const
{
    if (const auto* plannedDiff { std::get_if<LineDiff> (&plannedContent) })
    {
        plannedDiff->appendUnifiedDiff (output, *originalDocument, plannedDiff->getPlannedBytes(), originalDocument->getDisplayPath(), text::Formats::diffPlanned, existing, plannedExisting);
    }
    else
    {
        const auto& plannedDocument { std::get<std::unique_ptr<TextDocument>> (plannedContent) };
        LineDiff (*originalDocument, *plannedDocument).appendUnifiedDiff (output, *originalDocument, plannedDocument->getBytes(), originalDocument->getDisplayPath(), text::Formats::diffPlanned, existing, plannedExisting);
    }
}

std::pair<juce::Result, bool> StagedFile::commit (std::string_view transactionId) const
{
    const auto blobFile { UndoCache::getBlobFile (transactionId, blobIndex) };

    if (jam::File::isSwappable (targetFile, blobFile))
    {
        if (const auto swapped { swap (blobFile) })
            return *swapped;

        const auto backupStatus { Writer::toFile (blobFile, getOriginalBytes()) };

        if (backupStatus.failed())
            return { backupStatus, false };
    }

    return replace();
}

std::pair<juce::Result, bool> StagedFile::undo (std::string_view transactionId) const
{
    if (not plannedExisting)
        return remove();

    const auto blobFile { UndoCache::getBlobFile (transactionId, blobIndex) };
    const bool isSwappable { existing
                             and jam::File::isSwappable (targetFile, blobFile)
                             and jam::File::setMetadata (blobFile, targetFile)
                             and blobFile.setLastModificationTime (juce::Time::getCurrentTime()) };

    if (isSwappable)
    {
        if (const auto swapped { swap (blobFile) })
            return *swapped;
    }

    return replace();
}

juce::Result StagedFile::restore (std::string_view transactionId) const
{
    if (not plannedExisting)
        return restoreRemoved();

    if (not existing)
    {
        const auto plannedStatus { getBytesStatus (targetFile, getPlannedBytes()) };

        if (plannedStatus.failed())
            return plannedStatus;

        return targetFile.deleteFile() ? juce::Result::ok() : Stdio::fail (text::Diagnostics::failOverwriteFailed);
    }

    return restoreReplaced (transactionId);
}

juce::Result StagedFile::restoreReplaced (std::string_view transactionId) const
{
    const auto blobFile { UndoCache::getBlobFile (transactionId, blobIndex) };

    if (getBytesStatus (blobFile, getPlannedBytes()).failed())
    {
        const auto blobStatus { getBytesStatus (blobFile, getOriginalBytes()) };

        if (blobStatus.failed())
            return blobStatus;

        if (jam::File::swapFiles (targetFile, blobFile))
        {
            const auto swapStatus { getBytesStatus (blobFile, getPlannedBytes()) };

            if (swapStatus.failed())
            {
                jam::File::swapFiles (targetFile, blobFile);
                return swapStatus;
            }

            return getBytesStatus (targetFile, getOriginalBytes());
        }
    }

    const auto plannedStatus { getBytesStatus (targetFile, getPlannedBytes()) };

    if (plannedStatus.failed())
        return plannedStatus;

    const auto writeStatus { Writer::toFile (targetFile, getOriginalBytes()) };

    if (writeStatus.failed())
        return writeStatus;

    return getBytesStatus (targetFile, getOriginalBytes());
}

juce::Result StagedFile::getBytesStatus (const juce::File& file, std::string_view expectedBytes) const
{
    return TextDocument::getBytesStatus (file, getDisplayPath(), expectedBytes, text::Diagnostics::failChecksumMismatch);
}

std::optional<std::pair<juce::Result, bool>> StagedFile::swap (const juce::File& blobFile) const
{
    if (jam::File::swapFiles (targetFile, blobFile))
    {
        const auto preStatus { getBytesStatus (blobFile, getOriginalBytes()) };

        if (preStatus.failed())
        {
            const bool isSwappedBack { jam::File::swapFiles (targetFile, blobFile) };

            if (isSwappedBack and getBytesStatus (blobFile, getPlannedBytes()).wasOk())
                return std::pair<juce::Result, bool> { preStatus, false };

            if (isSwappedBack)
                jam::File::swapFiles (targetFile, blobFile);

            return std::pair<juce::Result, bool> { preStatus, true };
        }

        const auto postStatus { getBytesStatus (targetFile, getPlannedBytes()) };

        if (postStatus.failed())
            return std::pair<juce::Result, bool> { postStatus, true };

        return std::pair<juce::Result, bool> { juce::Result::ok(), false };
    }

    return std::nullopt;
}

std::pair<juce::Result, bool> StagedFile::replace() const
{
    if (existing)
    {
        const auto originalStatus { getBytesStatus (targetFile, getOriginalBytes()) };

        if (originalStatus.failed())
            return { originalStatus, false };
    }

    if (not existing and targetFile.exists())
        return { Stdio::fail (text::Diagnostics::failChecksumMismatch), false };

    const auto writeStatus { Writer::toFile (targetFile, getPlannedBytes()) };

    if (writeStatus.failed())
    {
        const bool isUnchanged { existing ? getBytesStatus (targetFile, getOriginalBytes()).wasOk()
                                          : not targetFile.existsAsFile() };
        return { writeStatus, not isUnchanged };
    }

    if (not existing and jam::File::isExecutable (targetFile) != executable)
    {
        if (not targetFile.setExecutePermission (executable))
            return { Stdio::fail (text::Diagnostics::failOverwriteFailed), true };
    }

    const auto plannedStatus { getBytesStatus (targetFile, getPlannedBytes()) };

    if (plannedStatus.failed())
        return { plannedStatus, true };

    return { juce::Result::ok(), false };
}

std::pair<juce::Result, bool> StagedFile::remove() const
{
    const auto originalStatus { getBytesStatus (targetFile, getOriginalBytes()) };

    if (originalStatus.failed())
        return { originalStatus, false };

    if (not jam::File::deleteFile (targetFile))
        return { Stdio::fail (text::Diagnostics::failOverwriteFailed), false };

    return { juce::Result::ok(), false };
}

juce::Result StagedFile::restoreRemoved() const
{
    if (targetFile.existsAsFile())
        return getBytesStatus (targetFile, getOriginalBytes());

    const auto writeStatus { Writer::toFile (targetFile, getOriginalBytes()) };

    if (writeStatus.failed())
        return writeStatus;

    if (jam::File::isExecutable (targetFile) != executable)
    {
        if (not targetFile.setExecutePermission (executable))
            return Stdio::fail (text::Diagnostics::failOverwriteFailed);
    }

    return getBytesStatus (targetFile, getOriginalBytes());
}
