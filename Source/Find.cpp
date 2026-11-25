#include "Find.h"
#include "FindOutcome.h"
#include "Arguments.h"
#include "Input.h"
#include "Jobs.h"
#include "Paths.h"
#include "Pattern.h"
#include "Search.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static constexpr std::array defaultPaths { Id::currentDirectory };

static jam::Array<juce::Result> getFailureResults (const Paths::Failures& failures)
{
    const auto& [failurePaths, failureResults] { failures };
    jam::Array<int> failureOrder;
    failureOrder.ensureStorageAllocated (failurePaths.size());

    for (int index { 0 }; index < failurePaths.size(); ++index)
        failureOrder.add (index);

    const auto getMessageOrder { [&results = failureResults] (int firstIndex, int secondIndex)
    {
        return jam::Strings::getView (results.at (firstIndex).getErrorMessage()).compare (jam::Strings::getView (results.at (secondIndex).getErrorMessage()));
    } };
    const auto getPathOrder { [&paths = failurePaths] (int firstIndex, int secondIndex)
    {
        return paths.getView (firstIndex).compare (paths.getView (secondIndex));
    } };

    std::sort (failureOrder.begin(), failureOrder.end(), [&getMessageOrder, &getPathOrder] (int firstIndex, int secondIndex)
    {
        const int messageOrder { getMessageOrder (firstIndex, secondIndex) };
        return messageOrder < 0 or (messageOrder == 0 and getPathOrder (firstIndex, secondIndex) < 0);
    });

    const auto uniqueEnd { std::unique (failureOrder.begin(), failureOrder.end(), [&getMessageOrder] (int firstIndex, int secondIndex)
    {
        return getMessageOrder (firstIndex, secondIndex) == 0;
    }) };

    std::sort (failureOrder.begin(), uniqueEnd, [&getMessageOrder, &getPathOrder] (int firstIndex, int secondIndex)
    {
        const int pathOrder { getPathOrder (firstIndex, secondIndex) };
        return pathOrder < 0 or (pathOrder == 0 and getMessageOrder (firstIndex, secondIndex) < 0);
    });

    jam::Array<juce::Result> results;

    for (auto position { failureOrder.begin() }; position != uniqueEnd; ++position)
        results.add (failureResults.at (*position));

    return results;
}

static std::pair<jam::Array<FindOutcome>, Paths::Failures> getDirectoriesOutcome (const jam::Array<std::pair<std::string_view, juce::File>>& roots,
                                                                                  const Pattern& pattern,
                                                                                  const Arguments& arguments,
                                                                                  const Search& search)
{
    const bool isQuiet { arguments.contains (Id::quietOption) };
    const auto includeGlobs (arguments.getValues (Id::includeOption));
    const auto excludeGlobs (arguments.getValues (Id::excludeOption));
    jam::Array<FindOutcome> workerOutcomes;
    Paths::Failures walkFailures;
    std::atomic<bool> shouldStop { false };

    if (not roots.isEmpty())
    {
        walkFailures = Paths::applyToDirectories (roots,
                                                  includeGlobs,
                                                  excludeGlobs,
                                                  arguments.contains (Id::hiddenOption),
                                                  arguments.contains (Id::noIgnoreOption),
                                                  [&workerOutcomes] (int workerCount)
                                                  {
                                                      workerOutcomes.resize (workerCount);
                                                  },
                                                  [&workerOutcomes, isQuiet, &pattern, &search, &shouldStop] (int workerIndex, const std::string& fullPath, std::string_view displayPath)
                                                  {
                                                      auto& workerOutcome { workerOutcomes.at (workerIndex) };
                                                      workerOutcome.addFile (juce::File { jam::Strings::fromView (fullPath) }, displayPath, pattern, search, false, true);

                                                      if (isQuiet and workerOutcome.getNumMatches() > 0)
                                                          shouldStop.store (true, std::memory_order_relaxed);
                                                  },
                                                  shouldStop);
    }

    return { std::move (workerOutcomes), std::move (walkFailures) };
}

static jam::Array<FindOutcome> getFilesOutcome (const jam::Array<Paths::Entry>& files,
                                                const Pattern& pattern,
                                                const Search& search,
                                                bool isShowingPathPrefix)
{
    jam::Array<FindOutcome> outcomes;
    outcomes.resize (files.size());

    Jobs::run (files.size(),
               [&files, &pattern, &search, isShowingPathPrefix, &outcomes] (int index)
               {
                   const auto& [displayPath, file, isExplicit] { files.at (index) };
                   juce::ignoreUnused (isExplicit);
                   outcomes.at (index).addFile (file, displayPath, pattern, search, true, isShowingPathPrefix);
               });

    return outcomes;
}

static Stdio::Output runFilesFind (juce::Span<const std::string_view> rawPaths,
                                   const Pattern& pattern,
                                   const Arguments& arguments,
                                   const Search& search)
{
    const bool isQuiet { arguments.contains (Id::quietOption) };
    auto [failures, files, roots] { Paths::getClassifiedPaths (rawPaths) };
    const bool isShowingPathPrefix { rawPaths.size() > 1 };
    auto outcomes (getFilesOutcome (files, pattern, search, isShowingPathPrefix));
    auto [directoryOutcomes, directoryFailures] { getDirectoriesOutcome (roots, pattern, arguments, search) };

    for (int index { 0 }; index < directoryOutcomes.size(); ++index)
        outcomes.add (std::move (directoryOutcomes.at (index)));

    Paths::addFailures (failures, std::move (directoryFailures));
    int totalMatches { 0 };

    for (int index { 0 }; index < outcomes.size(); ++index)
    {
        totalMatches += outcomes.at (index).getNumMatches();
        Paths::addFailures (failures, outcomes.at (index).removeFailures());
    }

    auto& [failurePaths, failureResults] { failures };
    juce::ignoreUnused (failurePaths);

    if (isQuiet and totalMatches > 0)
        return Stdio::getOutput (Stdio::exitSuccess);

    const bool hasFailures { not failureResults.isEmpty() };
    const int exitCode { hasFailures ? Stdio::exitError : totalMatches > 0 ? Stdio::exitSuccess : Stdio::exitMismatch };
    return Stdio::getOutput (exitCode, isQuiet ? Stdio::Documents {} : FindOutcome::getSortedDocuments (std::move (outcomes)), getFailureResults (failures));
}

static Stdio::Output runStdinFind (const Pattern& pattern,
                                   const Arguments& arguments,
                                   const Search& search,
                                   const std::optional<std::string>& input)
{
    if (Input::isStream (input) and search.isStreamSearch())
    {
        auto [streamResult, streamMatches, streamDocuments, readSize] { search.runStream (pattern) };

        if (streamResult.failed())
            return Stdio::getFailure (streamResult);

        if (readSize == 0)
            return Stdio::getFailure (text::Diagnostics::failMissingPath);

        return Stdio::getOutput (streamMatches > 0 ? Stdio::exitSuccess : Stdio::exitMismatch, std::move (streamDocuments));
    }

    const auto [inputResult, standardInput] { TextDocument::getInput (input, Input::allLines) };

    if (inputResult.failed())
        return Stdio::getFailure (inputResult);

    const auto stdinBytes { standardInput->getBytes() };
    jam::RegExpAutomaton automaton;
    auto [searchResult, matchCount, documents] { search.run (stdinBytes,
                                                             Id::stdinPath,
                                                             pattern,
                                                             automaton,
                                                             true,
                                                             false) };

    if (searchResult.failed())
        return Stdio::getFailure (searchResult);

    const int exitCode { matchCount > 0 ? Stdio::exitSuccess : Stdio::exitMismatch };
    return Stdio::getOutput (exitCode, std::move (documents));
}

static Stdio::Output runListFiles (const Arguments& arguments)
{
    const auto positionals (arguments.getPositionals (0));
    const auto rawPaths (positionals.empty() ? juce::Span<const std::string_view> { defaultPaths } : positionals);
    auto [pathFailures, files] { Paths::getFiles (rawPaths,
                                                  arguments.getValues (Id::includeOption),
                                                  arguments.getValues (Id::excludeOption),
                                                  arguments.contains (Id::hiddenOption),
                                                  arguments.contains (Id::noIgnoreOption)) };
    auto& [failurePaths, failureResults] { pathFailures };
    juce::ignoreUnused (failurePaths);
    const bool hasFailures { not failureResults.isEmpty() };
    std::string output;

    if (not arguments.contains (Id::quietOption))
    {
        for (const auto& [displayPath, file, isExplicit] : files)
        {
            juce::ignoreUnused (file, isExplicit);
            jam::Format::appendBytes (output, displayPath);
            jam::Format::appendUtf8 (output, Chars::newline);
        }
    }

    const int exitCode { hasFailures ? Stdio::exitError : files.isEmpty() ? Stdio::exitMismatch : Stdio::exitSuccess };
    return Stdio::getOutput (exitCode, Stdio::getDocuments (std::move (output)), getFailureResults (pathFailures));
}

static Stdio::Output runWithValidatedOptions (const Arguments& arguments,
                                              const Search& search,
                                              const std::optional<std::string>& input)
{
    const auto isPatternOption { arguments.contains (Id::patternOption) };
    const int firstPathIndex { isPatternOption ? 0 : 1 };
    const auto positionals (arguments.getPositionals (0));
    const std::string_view patternText { isPatternOption ? std::string_view {} : positionals.front() };
    const auto patternTexts (isPatternOption ? arguments.getValues (Id::patternOption) : juce::Span<const std::string_view> { &patternText, 1 });
    const auto [patternResult, pattern] { Pattern::create (patternTexts,
                                                           arguments.contains (Id::regexOption),
                                                           arguments.contains (Id::ignoreCaseOption),
                                                           arguments.contains (Id::exactOption),
                                                           arguments.contains (Id::wordOption)) };

    if (patternResult.failed())
        return Stdio::getFailure (patternResult);

    const auto rawPaths (arguments.getPositionals (firstPathIndex));

    if (rawPaths.empty() and not Stdio::isTerminalInput (input))
        return runStdinFind (*pattern, arguments, search, input);

    return runFilesFind (rawPaths.empty() ? juce::Span<const std::string_view> { defaultPaths } : rawPaths, *pattern, arguments, search);
}

static Stdio::Output runWithValidatedArguments (const Arguments& arguments, const std::optional<std::string>& input)
{
    const auto& positionals { arguments.getPositionals() };

    if (arguments.contains (Id::filesOption))
        return runListFiles (arguments);

    if (positionals.size() < 1 and not arguments.contains (Id::patternOption))
        return Stdio::getFailure (text::Diagnostics::failFindRequiresPattern);

    const auto [contextResult, contextDefault] { arguments.getInteger (Id::contextOption, 0, 0) };

    if (contextResult.failed())
        return Stdio::getFailure (contextResult);

    const auto [beforeResult, requestedBeforeContext] { arguments.getInteger (Id::beforeContextOption, 0, contextDefault) };

    if (beforeResult.failed())
        return Stdio::getFailure (beforeResult);

    const auto [afterContextResult, requestedAfterContext] { arguments.getInteger (Id::afterContextOption, 0, contextDefault) };

    if (afterContextResult.failed())
        return Stdio::getFailure (afterContextResult);

    const auto [afterResult, afterLine] { arguments.getInteger (Id::afterOption, 0, 0) };

    if (afterResult.failed())
        return Stdio::getFailure (afterResult);

    const auto [linesResult, lineRange] { arguments.getLineRangeFilter() };

    if (linesResult.failed())
        return Stdio::getFailure (linesResult);

    const auto isContextSuppressed { arguments.contains (Id::quietOption) or arguments.contains (Id::filesWithMatchesOption) or arguments.contains (Id::countOption) or arguments.contains (Id::onlyMatchingOption) };
    const int beforeContext { isContextSuppressed ? 0 : requestedBeforeContext };
    const int afterContext { isContextSuppressed ? 0 : requestedAfterContext };

    const Search search { arguments, beforeContext, afterContext, afterLine, lineRange };
    return runWithValidatedOptions (arguments, search, input);
}

Arguments Find::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList,
                       { Id::regexOption, Id::ignoreCaseOption, Id::exactOption, Id::invertOption,
                         Id::countOption, Id::quietOption, Id::lineNumberOption, Id::noLineNumberOption,
                         Id::hashOption, Id::firstOption, Id::hiddenOption, Id::noIgnoreOption,
                         Id::filesWithMatchesOption, Id::onlyMatchingOption, Id::wordOption, Id::filesOption },
                       { Id::contextOption, Id::afterOption, Id::linesOption, Id::includeOption, Id::excludeOption,
                         Id::afterContextOption, Id::beforeContextOption, Id::patternOption },
                       {} };
}

Stdio::Output Find::run (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    return runWithValidatedArguments (arguments, input);
}
