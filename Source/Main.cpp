#include <JuceHeader.h>
#include "Arguments.h"
#include "CommandLine.h"
#include "Stdio.h"
#include "ToolServer.h"
#include "generated/Generated.h"
#if JUCE_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

#if JUCE_WINDOWS
static jam::Strings getCommandLineArguments()
{
    jam::Strings arguments;
    int argumentCount { 0 };
    const std::unique_ptr<LPWSTR, decltype (&LocalFree)> wideArguments { CommandLineToArgvW (GetCommandLineW(), &argumentCount), &LocalFree };
    assert (wideArguments != nullptr);

    for (int index { 1 }; index < argumentCount; ++index)
        arguments.add (juce::String (wideArguments.get()[index]));

    return arguments;
}
#endif

static bool isMcp (juce::Span<const std::string_view> arguments)
{
    return static_cast<int> (arguments.size()) == Arguments::firstOptionIndex and Arguments::matchesOption (arguments[0], Id::mcpOption);
}

static juce::JUCEApplicationBase* createToolServer()
{
    return new ToolServer();
}

static int runToolServer (const char* programPath)
{
    const char* toolArguments[] { programPath };

    juce::JUCEApplicationBase::createInstance = &createToolServer;
    return juce::JUCEApplicationBase::main (static_cast<int> (std::size (toolArguments)), toolArguments);
}

int main (int argc, char* argv[])
{
#if JUCE_WINDOWS
    SetConsoleOutputCP (CP_UTF8);
    juce::ignoreUnused (argc);

    const auto ownedArguments (getCommandLineArguments());
    jam::Array<std::string_view> arguments;
    arguments.ensureStorageAllocated (ownedArguments.size());

    for (int index { 0 }; index < ownedArguments.size(); ++index)
        arguments.add (ownedArguments.getView (index));
#else
    jam::Array<std::string_view> arguments;
    arguments.ensureStorageAllocated (argc - 1);

    for (int index { 1 }; index < argc; ++index)
        arguments.add (std::string_view { argv[index] });
#endif

    const juce::Span<const std::string_view> argumentSpan { arguments.data(), static_cast<size_t> (arguments.size()) };

    if (isMcp (argumentSpan))
        return runToolServer (argv[0]);

    return Stdio::write (CommandLine::run (argumentSpan, std::nullopt));
}
