#include "FindOutcome.h"
#include "Stdio.h"

void FindOutcome::addFile (const juce::File& file, std::string_view displayPath, const Pattern& pattern, const Search& search, bool isExplicit, bool isShowingPathPrefix, const Input::Source& source)
{
    auto [status, matchCount, documents] { search.runFile (file, displayPath, pattern, automaton, isExplicit, isShowingPathPrefix, source) };

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
            files.add (std::move (documents));
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
    jam::Array<Stdio::Documents> allFiles;

    for (auto& outcome : outcomes)
    {
        for (auto& outcomeFiles : outcome.files)
            allFiles.add (std::move (outcomeFiles));
    }

    if (allFiles.size() > 1)
    {
        std::stable_sort (allFiles.begin(), allFiles.end(), [] (const Stdio::Documents& firstFile, const Stdio::Documents& secondFile)
        {
            return firstFile.first()->getDisplayPath() < secondFile.first()->getDisplayPath();
        });
        const auto uniqueEnd { std::unique (allFiles.begin(), allFiles.end(), [] (const Stdio::Documents& firstFile, const Stdio::Documents& secondFile)
        {
            return firstFile.first()->getDisplayPath().compare (secondFile.first()->getDisplayPath()) == 0;
        }) };
        const int uniqueCount { static_cast<int> (std::distance (allFiles.begin(), uniqueEnd)) };
        allFiles.removeRange (uniqueCount, allFiles.size() - uniqueCount);
    }

    Stdio::Documents documents;

    for (auto& file : allFiles)
        Stdio::addDocuments (documents, std::move (file));

    return documents;
}
