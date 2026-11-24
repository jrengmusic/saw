#include "CommandLine.h"
#include "Arguments.h"
#include "Excerpt.h"
#include "Fields.h"
#include "Find.h"
#include "Lines.h"
#include "Patch.h"
#include "Replace.h"
#include "Undo.h"
#include "generated/Generated.h"

static constexpr int verbArgumentIndex { 0 };

static constexpr std::array verbs
{
    std::make_tuple (Id::find, files::findHelp, &Find::getArguments, &Find::run),
    std::make_tuple (Id::replace, files::replaceHelp, &Replace::getArguments, &Replace::run),
    std::make_tuple (Id::lines, files::linesHelp, &Lines::getArguments, &Lines::run),
    std::make_tuple (Id::fields, files::fieldsHelp, &Fields::getArguments, &Fields::run),
    std::make_tuple (Id::patch, files::patchHelp, &Patch::getArguments, &Patch::run),
    std::make_tuple (Id::undo, files::undoHelp, &Undo::getArguments, &Undo::run),
    std::make_tuple (Id::excerpt, files::excerptHelp, &Excerpt::getArguments, &Excerpt::run)
};

static std::string getFrame (std::string_view resourceFileName)
{
    const auto [data, size] { BinaryData::fetcher (resourceFileName.data()) };
    assert (data != nullptr);
    return { static_cast<const char*> (data), static_cast<size_t> (size) };
}

static std::string getScreenErase()
{
    return Stdio::isTerminalOutput() ? getFrame (files::screenErase) : std::string {};
}

static std::string getBanner()
{
    std::string banner;

    for (const auto text : map::banner)
        banner += Stdio::getLine (jam::Strings::fromView (text));

    return banner;
}

static juce::String getVersionLine()
{
    return ProjectInfo::projectName
           + juce::String::charToString (Chars::space)
           + ProjectInfo::versionString
           + Chars::space
           + Chars::openParen + CAST_COMMIT
           + Chars::closeParen;
}

static Stdio::Output getBannerAndHelp()
{
    return { Stdio::exitSuccess, getScreenErase() + getBanner() + Stdio::getLine ({}) + getFrame (files::sawHelp), {} };
}

static Stdio::Output runGlobalOption (const jam::Strings& arguments)
{
    if (arguments.size() > Arguments::firstOptionIndex)
        return Stdio::getFailure (Stdio::getFormatted (text::Diagnostics::failFlagUnknown, arguments.at (Arguments::firstOptionIndex).toRawUTF8()));

    if (Arguments::matchesOption (arguments.getView (verbArgumentIndex), Id::versionOption))
        return { Stdio::exitSuccess, Stdio::getLine (getVersionLine()), {} };

    return getBannerAndHelp();
}

static Stdio::Output runVerb (const jam::Strings& arguments, const std::optional<std::string>& input)
{
    const auto verbArgument { arguments.at (verbArgumentIndex) };
    const auto verb { std::find_if (verbs.begin(), verbs.end(),
                                    [&verbArgument] (const auto& entry)
                                    {
                                        return std::get<0> (entry).compare (jam::Strings::getView (verbArgument)) == 0;
                                    }) };

    if (verb == verbs.end())
    {
        const auto* format { verbArgument.startsWithChar (Chars::dash) ? text::Diagnostics::failFlagUnknown : text::Diagnostics::failCommandUnknown };
        return Stdio::getFailure (Stdio::getFormatted (format, verbArgument.toRawUTF8()));
    }

    const auto verbArguments { std::get<2> (*verb) (arguments) };

    if (verbArguments.contains (Id::helpOption))
        return { Stdio::exitSuccess, getScreenErase() + getFrame (std::get<1> (*verb)), {} };

    return std::get<3> (*verb) (verbArguments, input);
}

Stdio::Output CommandLine::run (const jam::Strings& arguments, const std::optional<std::string>& input)
{
    if (arguments.size() <= verbArgumentIndex)
        return getBannerAndHelp();

    const auto firstArgument { arguments.getView (verbArgumentIndex) };

    if (Arguments::matchesOption (firstArgument, Id::helpOption)
        or Arguments::matchesOption (firstArgument, Id::versionOption))
        return runGlobalOption (arguments);

    return runVerb (arguments, input);
}
