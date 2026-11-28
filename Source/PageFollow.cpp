#include "PageFollow.h"
#include "Page.h"
#include "Paths.h"
#include "generated/Generated.h"

static constexpr size_t singleFilePositionalCount { 1 };

static std::pair<juce::Result, juce::File> getFollowTarget (const Arguments& arguments)
{
    const auto positionals (arguments.getPositionals (0));
    const bool isSingleFile { positionals.size() == singleFilePositionalCount and positionals.front().compare (Id::stdinArgument) != 0 };

    if (isSingleFile)
        return { juce::Result::ok(), Paths::getLinkedTarget (positionals.front()) };

    const std::string_view followOptionName { Arguments::getOptionName (arguments.getLastFlag ({ Id::followOption, Id::followNameOption })) };
    return { Stdio::fail (text::Diagnostics::failFollowUnavailable, followOptionName), juce::File {} };
}

bool PageFollow::isRequested (juce::Span<const std::string_view> arguments)
{
    if (arguments.empty())
        return false;

    const bool isPageVerb { jam::Strings::getView (Id::page.toString()).compare (arguments[0]) == 0 };
    const auto pageArguments { Page::getArguments (arguments) };
    return isPageVerb and (pageArguments.contains (Id::followOption) or pageArguments.contains (Id::followNameOption));
}

const juce::String PageFollow::getApplicationName()
{
    return ProjectInfo::projectName;
}

const juce::String PageFollow::getApplicationVersion()
{
    return ProjectInfo::versionString;
}

bool PageFollow::moreThanOneInstanceAllowed()
{
    return true;
}

void PageFollow::initialise (const juce::String&)
{
    jam::Strings commandLineStorage;

    for (const auto& argument : juce::JUCEApplicationBase::getCommandLineParameterArray())
        commandLineStorage.add (argument);

    jam::Array<std::string_view> views;

    for (int index { 0 }; index < commandLineStorage.size(); ++index)
        views.add (commandLineStorage.getView (index));

    const Arguments arguments { Page::getArguments (juce::Span<const std::string_view> { views.data(), static_cast<size_t> (views.size()) }) };
    const auto [targetResult, targetFile] { getFollowTarget (arguments) };

    if (targetResult.failed())
        writeOutput (Stdio::getFailure (targetResult));
    else
        startFollowing (arguments, targetFile);
}

void PageFollow::shutdown()
{
}

void PageFollow::anotherInstanceStarted (const juce::String&)
{
}

void PageFollow::systemRequestedQuit()
{
    quit();
}

void PageFollow::suspended()
{
}

void PageFollow::resumed()
{
}

void PageFollow::unhandledException (const std::exception*, const juce::String&, int)
{
}

void PageFollow::startFollowing (const Arguments& arguments, const juce::File& file)
{
    target = file;
    isFollowingName = arguments.contains (Id::followNameOption);
    remainingBytes = arguments.contains (Id::noLimitOption) ? std::numeric_limits<size_t>::max() : Stdio::outputLimitBytes;

    if (writeOutput (Page::run (arguments, Input::Source { Input::StandardInput {} })))
    {
        stream = std::make_unique<juce::FileInputStream> (target);

        if (stream->openedOk())
        {
            offset = stream->getTotalLength();
            watcher.addFolder (target.getParentDirectory());
            watcher.addListener (this);
        }
        else
        {
            writeOutput (Stdio::getFailure (stream->getStatus()));
        }
    }
}

void PageFollow::fileChanged (const juce::File& file, jam::File::Watcher::Event event)
{
    if (file == target)
    {
        const bool isReopening { isFollowingName and (event == jam::File::Watcher::fileCreated or event == jam::File::Watcher::fileRenamedNewName) };

        if (isReopening)
            reopen();

        if (isReopening or event == jam::File::Watcher::fileUpdated)
            writeAppended();
    }
}

void PageFollow::reopen()
{
    stream = std::make_unique<juce::FileInputStream> (target);
    offset = 0;

    if (not stream->openedOk())
        writeOutput (Stdio::getFailure (stream->getStatus()));
}

void PageFollow::writeAppended()
{
    if (isFollowingName and stream->getTotalLength() < offset)
        offset = 0;

    if (stream->getTotalLength() > offset)
        writeOutput (Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (readAppendedBytes())));
}

std::string PageFollow::readAppendedBytes()
{
    std::string bytes;
    bytes.resize (static_cast<size_t> (stream->getTotalLength() - offset));
    stream->setPosition (offset);
    const int count { stream->read (bytes.data(), static_cast<int> (bytes.size())) };
    bytes.resize (static_cast<size_t> (count));
    offset += count;
    return bytes;
}

bool PageFollow::writeOutput (Stdio::Output output)
{
    const auto [limitedOutput, bytesLeft, lines, omittedLines] { Stdio::getLimitedOutput (std::move (output), remainingBytes, printedLines) };
    remainingBytes = bytesLeft;
    printedLines = lines;
    const int exitCode { Stdio::write (limitedOutput) };
    const bool isFinished { exitCode != Stdio::exitSuccess or omittedLines > 0 };

    if (isFinished)
    {
        watcher.removeListener (this);
        juce::JUCEApplicationBase::setApplicationReturnValue (exitCode);
        quit();
    }

    return not isFinished;
}
