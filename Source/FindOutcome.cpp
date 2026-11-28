#include "FindOutcome.h"
#include "Stdio.h"

void FindOutcome::addFile (Paths::Entry entry, const Pattern& pattern, const Search& search, bool isShowingPathPrefix, const Input::Source& source)
{
    const auto displayPath { Paths::getDisplayPath (entry) };
    const bool isExplicit { Paths::isExplicit (entry) };
    auto [status, matchCount, documents] { search.runFile (Paths::getFile (entry), displayPath, pattern, automaton, isExplicit, isShowingPathPrefix, source) };

    if (status.failed())
    {
        auto& [failurePaths, failureResults] { failures };
        failurePaths.add (displayPath);
        failureResults.add (std::move (status));
    }
    else
    {
        numMatches += matchCount;

        if (not documents.isEmpty())
            files.add (std::pair<Paths::Entry, Stdio::Documents> { std::move (entry), std::move (documents) });
    }
}

int FindOutcome::getNumMatches() const noexcept
{
    return numMatches;
}

Paths::Failures FindOutcome::removeFailures()
{
    return std::move (failures);
}

Stdio::Documents FindOutcome::getSortedDocuments (jam::Array<FindOutcome> outcomes)
{
    jam::Array<std::pair<Paths::Entry, Stdio::Documents>> allFiles;

    for (auto& outcome : outcomes)
    {
        for (auto& outcomeFiles : outcome.files)
            allFiles.add (std::move (outcomeFiles));
    }

    if (allFiles.size() > 1)
    {
        std::stable_sort (allFiles.begin(), allFiles.end(), [] (const auto& firstFile, const auto& secondFile)
        {
            const auto& [firstEntry, firstDocuments] { firstFile };
            const auto& [secondEntry, secondDocuments] { secondFile };
            return Paths::getDisplayPath (firstEntry) < Paths::getDisplayPath (secondEntry);
        });
        const auto uniqueEnd { std::unique (allFiles.begin(), allFiles.end(), [] (const auto& firstFile, const auto& secondFile)
        {
            const auto& [firstEntry, firstDocuments] { firstFile };
            const auto& [secondEntry, secondDocuments] { secondFile };
            return Paths::getDisplayPath (firstEntry).compare (Paths::getDisplayPath (secondEntry)) == 0;
        }) };
        const int uniqueCount { static_cast<int> (std::distance (allFiles.begin(), uniqueEnd)) };
        allFiles.removeRange (uniqueCount, allFiles.size() - uniqueCount);
    }

    Stdio::Documents documents;

    for (auto& file : allFiles)
    {
        auto& [fileEntry, fileDocuments] { file };
        Stdio::addDocuments (documents, std::move (fileDocuments));
    }

    return documents;
}
