#include <JuceHeader.h>
#include "CommandLine.h"
#include "Stdio.h"
#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>
#endif

static jam::Strings getCommandLineArguments (int argc, char* argv[])
{
    jam::Strings arguments;

#ifdef _WIN32
    juce::ignoreUnused (argc, argv);
    int argumentCount { 0 };
    const std::unique_ptr<LPWSTR, decltype (&LocalFree)> wideArguments { CommandLineToArgvW (GetCommandLineW(), &argumentCount), &LocalFree };
    assert (wideArguments != nullptr);

    for (int index { 1 }; index < argumentCount; ++index)
        arguments.add (juce::String (wideArguments.get()[index]));
#else
    for (int index { 1 }; index < argc; ++index)
        arguments.add (std::string_view { argv[index] });
#endif

    return arguments;
}

int main (int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP (CP_UTF8);
#endif

    return Stdio::write (CommandLine::run (getCommandLineArguments (argc, argv), std::nullopt));
}
