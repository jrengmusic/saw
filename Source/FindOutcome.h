#pragma once
#include <JuceHeader.h>
#include "Paths.h"
#include "Pattern.h"
#include "Search.h"
#include "Stdio.h"

/** @brief Result slot of one search worker: the documents of the matching files, the match count, and the failures.
 *
 *  One job writes one slot at a time, so the jobs search in parallel with no lock. The slots are
 *  read after every job has finished.
 */
class FindOutcome
{
public:
    /** @brief Searches one file, and records its documents, its match count, or its failure.
     *
     *  @param file                The file to search.
     *  @param displayPath         The path of the file, as the user sees it.
     *  @param pattern             The pattern that the lines are matched against.
     *  @param search              The search options that shape the output.
     *  @param isExplicit          True when the file was named on the command line.
     *  @param isShowingPathPrefix True when each output line carries the display path.
     *  @param source              The source of the input, passed to the search of the file.
     */
    void addFile (const juce::File& file, std::string_view displayPath, const Pattern& pattern, const Search& search, bool isExplicit, bool isShowingPathPrefix, const Input::Source& source);

    /** @brief Returns the number of matches of the files that searched without a failure.
     *
     *  @return The number of matches recorded by addFile.
     */
    int getNumMatches() const noexcept;

    /** @brief Moves the failures out of the slot, so that the caller reports them once.
     *
     *  @return The display paths and the failure results, in the order that addFile recorded them.
     */
    Paths::Failures removeFailures();

    /** @brief Lists the documents of several slots, sorted by display path, with the documents of each file together.
     *
     *  The walk has no order, so the sort gives the same output for the same tree. When one file is recorded
     *  two times, its first record is kept. The documents are moved, so no text is copied.
     *
     *  @param outcomes The slots of all workers.
     *
     *  @return The documents of every matching file, in display path order.
     */
    static Stdio::Documents getSortedDocuments (jam::Array<FindOutcome> outcomes);

private:
    /** @brief Matches recorded by the files that searched without a failure. */
    int numMatches { 0 };

    /** @brief Documents of each file that has matching output. */
    jam::Array<Stdio::Documents> files;

    /** @brief Display paths and results of the files that failed. */
    Paths::Failures failures;

    /** @brief Regular-expression automaton that addFile passes to the search of its file. */
    jam::RegExpAutomaton automaton;
};
