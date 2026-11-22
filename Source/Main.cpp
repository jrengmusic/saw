#include <JuceHeader.h>
#include "generated/Generated.h"
#include "Stdio.h"
#include "Arguments.h"
#include "Find.h"
#include "Fields.h"
#include "Lines.h"
#include "Replace.h"
#include "Patch.h"
#include "Undo.h"
#ifdef _WIN32
#include <windows.h>
#endif

static constexpr int verbArgumentIndex { 0 };
static constexpr int firstVerbOptionIndex { 1 };

static void writeScreenErase()
{
    if (Stdio::isTerminalOutput())
    {
        juce::MemoryOutputStream erase;
        jam::TerminalEncoder::writeEraseScreen (erase);
        fwrite (erase.getData(), sizeof (char), erase.getDataSize(), stdout);
        fflush (stdout);
    }
}

static void printBanner()
{
    for (const auto& [name, text] : map::banner)
        Stdio::print (text);
}

static void writeHelpPage (const juce::String& resourceFileName)
{
    jam::TerminalPage page { { BinaryData::fetcher }, jam::Strings { files::styleSheet } };
    auto document { jam::MarkdownDocument::parse (BinaryData::getString (resourceFileName), resourceFileName) };
    assert (document != nullptr);
    page.setDocument (std::move (*document));
    juce::MemoryOutputStream frame;
    page.writeToStream (frame);
    fwrite (frame.getData(), sizeof (char), frame.getDataSize(), stdout);
}

static void printBannerAndHelp()
{
    writeScreenErase();
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    printBanner();
    Stdio::print ({});
    writeHelpPage (files::sawHelp);
}

static void printHelp (const juce::String& resourceFileName)
{
    writeScreenErase();
    const juce::ScopedJuceInitialiser_GUI juceInitialiser;
    writeHelpPage (resourceFileName);
}

static void writeVersion()
{
    const auto versionLine { ProjectInfo::projectName
                             + juce::String::charToString (Chars::space)
                             + ProjectInfo::versionString
                             + Chars::space
                             + Chars::openParen + CAST_COMMIT
                             + Chars::closeParen };
    Stdio::print (versionLine);
}

static const jam::HashMap<juce::Identifier, juce::String>& getVerbHelpFiles()
{
    static const auto verbHelpFiles (
        []
        {
            jam::HashMap<juce::Identifier, juce::String> verbHelpFiles;
            verbHelpFiles.try_emplace (Id::find, files::findHelp);
            verbHelpFiles.try_emplace (Id::replace, files::replaceHelp);
            verbHelpFiles.try_emplace (Id::lines, files::linesHelp);
            verbHelpFiles.try_emplace (Id::fields, files::fieldsHelp);
            verbHelpFiles.try_emplace (Id::patch, files::patchHelp);
            verbHelpFiles.try_emplace (Id::undo, files::undoHelp);
            return verbHelpFiles;
        }());

    return verbHelpFiles;
}

static const jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, int (const jam::Strings&)>& getVerbCommands()
{
    static const auto verbCommands (
        []
        {
            jam::Function::Map<juce::Identifier, jam::Function::pointerCapacity, int (const jam::Strings&)> verbCommands;
            verbCommands.add (Id::find, [] (const juce::Identifier&, const jam::Strings& arguments) { return Find::run (arguments); });
            verbCommands.add (Id::replace, [] (const juce::Identifier&, const jam::Strings& arguments) { return Replace::run (arguments); });
            verbCommands.add (Id::lines, [] (const juce::Identifier&, const jam::Strings& arguments) { return Lines::run (arguments); });
            verbCommands.add (Id::fields, [] (const juce::Identifier&, const jam::Strings& arguments) { return Fields::run (arguments); });
            verbCommands.add (Id::patch, [] (const juce::Identifier&, const jam::Strings& arguments) { return Patch::run (arguments); });
            verbCommands.add (Id::undo, [] (const juce::Identifier&, const jam::Strings& arguments) { return Undo::run (arguments); });
            return verbCommands;
        }());

    return verbCommands;
}

static int runGlobalOption (const jam::Strings& arguments)
{
    if (arguments.size() > firstVerbOptionIndex)
    {
        Stdio::printError (juce::String::formatted (text::Diagnostics::failFlagUnknown, arguments.at (firstVerbOptionIndex).toRawUTF8()));
        return Stdio::exitError;
    }

    if (Arguments::matchesOption (arguments.at (verbArgumentIndex), Id::versionOption))
    {
        writeVersion();
        return Stdio::exitSuccess;
    }

    printBannerAndHelp();
    return Stdio::exitSuccess;
}

static int runVerb (const jam::Strings& arguments)
{
    const auto verbArgument { arguments.at (verbArgumentIndex) };
    const juce::Identifier verb { verbArgument.isNotEmpty() ? juce::Identifier { verbArgument } : juce::Identifier {} };
    const auto& verbHelpFiles { getVerbHelpFiles() };

    if (not verbHelpFiles.contains (verb))
    {
        const auto* format { verbArgument.startsWithChar (Chars::dash) ? text::Diagnostics::failFlagUnknown : text::Diagnostics::failCommandUnknown };
        Stdio::printError (juce::String::formatted (format, verbArgument.toRawUTF8()));
        return Stdio::exitError;
    }

    if (Arguments::containsOptionBeforeTerminator (arguments, firstVerbOptionIndex, Id::helpOption))
    {
        printHelp (verbHelpFiles.at (verb));
        return Stdio::exitSuccess;
    }

    return getVerbCommands().get (verb, arguments);
}

static int runCommandLine (int argc, char* argv[])
{
    jam::Strings arguments;

    for (int index { 1 }; index < argc; ++index)
        arguments.add (juce::String::fromUTF8 (argv[index]));

    if (arguments.size() <= verbArgumentIndex)
    {
        printBannerAndHelp();
        return Stdio::exitSuccess;
    }

    const auto firstArgument { arguments.at (verbArgumentIndex) };

    if (Arguments::matchesOption (firstArgument, Id::helpOption)
        or Arguments::matchesOption (firstArgument, Id::versionOption))
        return runGlobalOption (arguments);

    return runVerb (arguments);
}

int main (int argc, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP (CP_UTF8);
#endif

    return runCommandLine (argc, argv);
}
