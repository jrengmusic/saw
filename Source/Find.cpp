#include "Find.h"
#include "Arguments.h"
#include "Input.h"
#include "Jobs.h"
#include "Paths.h"
#include "Pattern.h"
#include "Search.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static std::string getFailureDiagnostics (std::vector<std::pair<std::string, juce::Result>> failures)
{
    std::sort (failures.begin(), failures.end(), &Paths::isPathEarlier<juce::Result>);

    jam::Strings printedMessages;
    std::string diagnostics;

    for (const auto& [failurePath, failureResult] : failures)
    {
        juce::ignoreUnused (failurePath);
        const auto errorMessage { failureResult.getErrorMessage() };

        if (printedMessages.addIfNotAlreadyThere (errorMessage, false))
            diagnostics += Stdio::getDiagnostic (errorMessage);
    }

    return diagnostics;
}

static std::string getSortedOutput (std::vector<std::pair<std::string, std::string>> matchedOutputs)
{
    std::stable_sort (matchedOutputs.begin(), matchedOutputs.end(), &Paths::isPathEarlier<std::string>);
    size_t totalSize { 0 };

    for (const auto& [displayPath, output] : matchedOutputs)
    {
        juce::ignoreUnused (displayPath);
        totalSize += output.size();
    }

    if (matchedOutputs.size() == 1)
    {
        auto& [displayPath, output] { matchedOutputs.front() };
        juce::ignoreUnused (displayPath);
        return std::move (output);
    }

    std::string printedOutput;
    printedOutput.reserve (totalSize);
    std::string_view previousPath;

    for (const auto& [displayPath, output] : matchedOutputs)
    {
        if (previousPath.compare (displayPath) != 0)
            printedOutput.append (output);

        previousPath = displayPath;
    }

    return printedOutput;
}

static Stdio::Output runFilesFind (const jam::Strings& rawPaths,
                                   const Pattern& pattern,
                                   const Arguments& arguments,
                                   const Search& search)
{
    const bool isQuiet { arguments.contains (Id::quietOption) };
    const auto includeGlobs (arguments.getValues (Id::includeOption));
    const auto excludeGlobs (arguments.getValues (Id::excludeOption));
    std::vector<std::pair<std::string, juce::Result>> failures;
    std::vector<std::pair<std::string, juce::File>> explicitFiles;
    std::vector<std::pair<std::string, juce::File>> roots;
    std::vector<std::vector<std::pair<std::string, std::string>>> workerOutputs;
    std::vector<int> workerMatchCounts;
    std::vector<std::vector<std::pair<std::string, juce::Result>>> workerFailures;
    std::vector<jam::RegExpAutomaton> workerAutomata;
    std::atomic<bool> shouldStop { false };

    for (int index { 0 }; index < rawPaths.size(); ++index)
    {
        const auto rawPathView { rawPaths.getView (index) };
        const auto targetFile { Paths::getLinkedTarget (rawPathView) };

        if (not targetFile.exists())
        {
            failures.emplace_back (std::string { rawPathView }, TextDocument::getReadFailure (targetFile, jam::Strings::fromView (rawPathView)));
        }
        else if (targetFile.isDirectory())
        {
            roots.emplace_back (std::string { rawPathView }, targetFile);
        }
        else
        {
            explicitFiles.emplace_back (Paths::getDisplayPath (rawPathView), targetFile);
        }
    }

    if (not roots.empty())
    {
        Paths::applyToDirectories (roots,
                                   includeGlobs,
                                   excludeGlobs,
                                   arguments.contains (Id::hiddenOption),
                                   arguments.contains (Id::noIgnoreOption),
                                   [&workerOutputs, &workerMatchCounts, &workerFailures, &workerAutomata] (int workerCount)
                                   {
                                       workerOutputs.resize (static_cast<size_t> (workerCount));
                                       workerMatchCounts.resize (static_cast<size_t> (workerCount), 0);
                                       workerFailures.resize (static_cast<size_t> (workerCount));
                                       workerAutomata.resize (static_cast<size_t> (workerCount));
                                   },
                                   [&workerOutputs, &workerMatchCounts, &workerFailures, &workerAutomata, &shouldStop, isQuiet, &pattern, &search] (int workerIndex, const std::string& fullPath, std::string_view displayPath)
                                   {
                                       auto [status, matchCount, output] { search.runFile (juce::File { jam::Strings::fromView (fullPath) },
                                                                                           displayPath,
                                                                                           pattern,
                                                                                           workerAutomata.at (static_cast<size_t> (workerIndex)),
                                                                                           false,
                                                                                           true) };

                                       if (status.failed())
                                       {
                                           workerFailures.at (static_cast<size_t> (workerIndex)).emplace_back (std::string { displayPath }, std::move (status));
                                       }
                                       else if (matchCount > 0)
                                       {
                                           workerMatchCounts.at (static_cast<size_t> (workerIndex)) += matchCount;

                                           if (isQuiet)
                                               shouldStop.store (true, std::memory_order_relaxed);
                                           else if (not output.empty())
                                               workerOutputs.at (static_cast<size_t> (workerIndex)).emplace_back (std::string { displayPath }, std::move (output));
                                       }
                                   },
                                   [&workerFailures] (int workerIndex, std::string_view unreadableDisplayPath)
                                   {
                                       const auto displayString { jam::Strings::fromView (unreadableDisplayPath) };
                                       workerFailures.at (static_cast<size_t> (workerIndex))
                                           .emplace_back (std::string { unreadableDisplayPath },
                                                          TextDocument::getReadFailure (juce::File::getCurrentWorkingDirectory().getChildFile (displayString), displayString));
                                   },
                                   shouldStop);
    }

    const auto explicitCount { static_cast<int> (explicitFiles.size()) };
    jam::Array<juce::Result> statuses;
    statuses.insertMultiple (0, juce::Result::ok(), explicitCount);
    jam::Array<std::string> outputs;
    outputs.resize (explicitCount);
    jam::Array<int> matchCounts;
    matchCounts.resize (explicitCount);
    const bool isShowingPathPrefix { rawPaths.size() > 1 };

    Jobs::run (explicitCount,
               [&explicitFiles, &pattern, &search, isShowingPathPrefix, &statuses, &matchCounts, &outputs] (int index)
               {
                   jam::RegExpAutomaton automaton;
                   const auto& [displayPath, file] { explicitFiles.at (static_cast<size_t> (index)) };
                   auto [status, matchCount, output] { search.runFile (file, displayPath, pattern, automaton, true, isShowingPathPrefix) };
                   statuses.at (index) = std::move (status);
                   matchCounts.at (index) = matchCount;
                   outputs.at (index) = std::move (output);
               });

    int totalMatches { 0 };
    std::vector<std::pair<std::string, std::string>> matchedOutputs;

    for (int index { 0 }; index < explicitCount; ++index)
    {
        auto& [displayPath, file] { explicitFiles.at (static_cast<size_t> (index)) };
        juce::ignoreUnused (file);
        totalMatches += matchCounts.at (index);

        if (statuses.at (index).failed())
            failures.emplace_back (std::move (displayPath), statuses.at (index));
        else if (not outputs.at (index).empty())
            matchedOutputs.emplace_back (std::move (displayPath), std::move (outputs.at (index)));
    }

    for (const int count : workerMatchCounts)
        totalMatches += count;

    for (auto& worker : workerOutputs)
    {
        for (auto& entry : worker)
            matchedOutputs.emplace_back (std::move (entry));
    }

    for (auto& worker : workerFailures)
    {
        for (auto& failure : worker)
            failures.push_back (std::move (failure));
    }

    if (isQuiet and totalMatches > 0)
        return { Stdio::exitSuccess, {}, {} };

    const bool hasFailures { not failures.empty() };
    const int exitCode { hasFailures ? Stdio::exitError : totalMatches > 0 ? Stdio::exitSuccess : Stdio::exitMismatch };
    return { exitCode, isQuiet ? std::string {} : getSortedOutput (std::move (matchedOutputs)), getFailureDiagnostics (std::move (failures)) };
}

static Stdio::Output runStdinFind (const Pattern& pattern,
                                   const Arguments& arguments,
                                   const Search& search,
                                   const std::optional<std::string>& input)
{
    if (Input::isStream (input) and search.isStreamSearch())
    {
        auto [streamResult, streamMatches, streamOutput, readSize] { search.runStream (pattern) };

        if (streamResult.failed())
            return Stdio::getFailure (streamResult.getErrorMessage());

        if (readSize == 0)
            return Stdio::getFailure (text::Diagnostics::failMissingPath);

        return { streamMatches > 0 ? Stdio::exitSuccess : Stdio::exitMismatch, std::move (streamOutput), {} };
    }

    const auto [inputResult, standardInput] { Input::create (input, Input::allLines) };

    if (inputResult.failed())
        return Stdio::getFailure (inputResult.getErrorMessage());

    const auto stdinBytes { standardInput->getBytes() };

    if (stdinBytes.empty())
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    jam::RegExpAutomaton automaton;
    auto [searchResult, matchCount, output] { search.run (stdinBytes,
                                                          Id::stdinPath,
                                                          pattern,
                                                          automaton,
                                                          true,
                                                          false) };

    if (searchResult.failed())
        return Stdio::getFailure (searchResult.getErrorMessage());

    const int exitCode { matchCount > 0 ? Stdio::exitSuccess : Stdio::exitMismatch };
    return { exitCode, std::move (output), {} };
}

static Stdio::Output runListFiles (const Arguments& arguments)
{
    const auto& positionals { arguments.getPositionals() };
    jam::Strings rawPaths;
    rawPaths.addArray (positionals, 0, -1);

    if (rawPaths.size() == 0)
        rawPaths.add (Id::currentDirectory);

    auto [pathFailures, files, isExplicitFlags] { Paths::getFiles (rawPaths,
                                                                   arguments.getValues (Id::includeOption),
                                                                   arguments.getValues (Id::excludeOption),
                                                                   arguments.contains (Id::hiddenOption),
                                                                   arguments.contains (Id::noIgnoreOption)) };
    juce::ignoreUnused (isExplicitFlags);
    const bool hasFailures { not pathFailures.empty() };
    std::string diagnostics { getFailureDiagnostics (std::move (pathFailures)) };
    std::string output;

    if (not arguments.contains (Id::quietOption))
    {
        for (const auto& [displayPath, file] : files)
        {
            juce::ignoreUnused (file);
            jam::Format::appendBytes (output, displayPath);
            output.push_back (static_cast<char> (Chars::newline));
        }
    }

    const int exitCode { hasFailures ? Stdio::exitError : files.empty() ? Stdio::exitMismatch : Stdio::exitSuccess };
    return { exitCode, std::move (output), std::move (diagnostics) };
}

static Stdio::Output runWithValidatedOptions (const Arguments& arguments,
                                              const jam::Strings& positionals,
                                              int beforeContext,
                                              int afterContext,
                                              int afterLine,
                                              juce::Range<int> lineRange,
                                              const std::optional<std::string>& input)
{
    const auto isPatternOption { arguments.contains (Id::patternOption) };
    const int firstPathIndex { isPatternOption ? 0 : 1 };
    jam::Strings patterns;

    if (isPatternOption)
        patterns = arguments.getValues (Id::patternOption);
    else
        patterns.add (positionals.getView (0));

    const auto [patternResult, pattern] { Pattern::create (patterns,
                                                           arguments.contains (Id::regexOption),
                                                           arguments.contains (Id::ignoreCaseOption),
                                                           arguments.contains (Id::exactOption),
                                                           arguments.contains (Id::wordOption)) };

    if (patternResult.failed())
        return Stdio::getFailure (patternResult.getErrorMessage());

    const Search search { arguments, beforeContext, afterContext, afterLine, lineRange };
    jam::Strings rawPaths;
    rawPaths.addArray (positionals, firstPathIndex, -1);

    if (rawPaths.size() == 0 and not Stdio::isTerminalInput (input))
        return runStdinFind (*pattern, arguments, search, input);

    if (rawPaths.size() == 0)
        rawPaths.add (Id::currentDirectory);

    return runFilesFind (rawPaths, *pattern, arguments, search);
}

static Stdio::Output runWithValidatedArguments (const Arguments& arguments, const std::optional<std::string>& input)
{
    const auto& positionals { arguments.getPositionals() };

    if (arguments.contains (Id::filesOption))
        return runListFiles (arguments);

    if (positionals.size() < 1 and not arguments.contains (Id::patternOption))
        return Stdio::getFailure (text::Diagnostics::failFindRequiresPattern);

    const auto [contextResult, contextDefault] { arguments.getInteger (Id::contextOption, 0, 0) };
    const auto [beforeResult, requestedBeforeContext] { arguments.getInteger (Id::beforeContextOption, 0, contextDefault) };
    const auto [afterContextResult, requestedAfterContext] { arguments.getInteger (Id::afterContextOption, 0, contextDefault) };
    const auto [afterResult, afterLine] { arguments.getInteger (Id::afterOption, 0, 0) };
    const auto [linesResult, lineRange] { arguments.getLineRangeFilter() };

    if (contextResult.failed() or beforeResult.failed() or afterContextResult.failed() or afterResult.failed() or linesResult.failed())
    {
        const auto& failedResult { contextResult.failed() ? contextResult
                                 : beforeResult.failed() ? beforeResult
                                 : afterContextResult.failed() ? afterContextResult
                                 : afterResult.failed() ? afterResult
                                 : linesResult };
        return Stdio::getFailure (failedResult.getErrorMessage());
    }

    const auto isContextSuppressed { arguments.contains (Id::quietOption) or arguments.contains (Id::filesWithMatchesOption) or arguments.contains (Id::countOption) or arguments.contains (Id::onlyMatchingOption) };
    const int beforeContext { isContextSuppressed ? 0 : requestedBeforeContext };
    const int afterContext { isContextSuppressed ? 0 : requestedAfterContext };

    return runWithValidatedOptions (arguments, positionals, beforeContext, afterContext, afterLine, lineRange, input);
}

Arguments Find::getArguments (const jam::Strings& argumentList)
{
    const std::vector<std::string_view> flagOptions
    {
        Id::regexOption, Id::ignoreCaseOption, Id::exactOption, Id::invertOption,
        Id::countOption, Id::quietOption, Id::lineNumberOption, Id::noLineNumberOption,
        Id::hashOption, Id::firstOption, Id::hiddenOption, Id::noIgnoreOption,
        Id::filesWithMatchesOption, Id::onlyMatchingOption, Id::wordOption, Id::filesOption
    };
    const std::vector<std::string_view> valueOptions
    {
        Id::contextOption, Id::afterOption, Id::linesOption, Id::includeOption, Id::excludeOption,
        Id::afterContextOption, Id::beforeContextOption, Id::patternOption
    };

    return Arguments { argumentList, flagOptions, valueOptions, {} };
}

Stdio::Output Find::run (const Arguments& arguments, const std::optional<std::string>& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus().getErrorMessage());

    return runWithValidatedArguments (arguments, input);
}
