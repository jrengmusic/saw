#include "CommandLine.h"
#include "Arguments.h"
#include "Excerpt.h"
#include "Fields.h"
#include "Find.h"
#include "Lines.h"
#include "Patch.h"
#include "Replace.h"
#include "TextDocument.h"
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

static std::string_view getFrame (std::string_view resourceFileName)
{
    const auto [data, size] { BinaryData::fetcher (resourceFileName.data()) };
    assert (data != nullptr);
    return { static_cast<const char*> (data), static_cast<size_t> (size) };
}

static Stdio::Documents getScreenErase()
{
    Stdio::Documents screenErase;
    const auto frame { Stdio::isTerminalOutput() ? getFrame (files::screenErase) : std::string_view {} };

    if (not frame.empty())
        screenErase.add (TextDocument::fromView (frame, {}));

    return screenErase;
}

static void appendBanner (std::string& output)
{
    for (const auto text : map::banner)
    {
        jam::Format::appendBytes (output, text);
        jam::Format::appendUtf8 (output, Chars::newline);
    }
}

static std::string getVersionLine()
{
    std::string line;
    jam::Format::appendBytes (line, ProjectInfo::projectName);
    jam::Format::appendUtf8 (line, Chars::space);
    jam::Format::appendBytes (line, ProjectInfo::versionString);
    jam::Format::appendUtf8 (line, Chars::space);
    jam::Format::appendUtf8 (line, Chars::openParen);
    jam::Format::appendBytes (line, CAST_COMMIT);
    jam::Format::appendUtf8 (line, Chars::closeParen);
    jam::Format::appendUtf8 (line, Chars::newline);
    return line;
}

static Stdio::Output getBannerAndHelp()
{
    auto documents (getScreenErase());
    std::string banner;
    appendBanner (banner);
    jam::Format::appendUtf8 (banner, Chars::newline);
    documents.add (TextDocument::fromText (std::move (banner), {}));
    documents.add (TextDocument::fromView (getFrame (files::sawHelp), {}));
    return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
}

static Stdio::Output runGlobalOption (juce::Span<const std::string_view> arguments)
{
    if (static_cast<int> (arguments.size()) > Arguments::firstOptionIndex)
        return Stdio::getFailure (text::Diagnostics::failFlagUnknown, arguments[static_cast<size_t> (Arguments::firstOptionIndex)]);

    if (Arguments::matchesOption (arguments[static_cast<size_t> (verbArgumentIndex)], Id::versionOption))
        return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (getVersionLine()));

    return getBannerAndHelp();
}

static Stdio::Output runVerb (juce::Span<const std::string_view> arguments, const std::optional<std::string>& input)
{
    const auto verbArgument { arguments[static_cast<size_t> (verbArgumentIndex)] };
    const auto verb { std::find_if (verbs.begin(), verbs.end(),
                                    [&verbArgument] (const auto& entry)
                                    {
                                        const auto& [verbName, helpFileName, getVerbArguments, runVerbArguments] { entry };
                                        juce::ignoreUnused (helpFileName, getVerbArguments, runVerbArguments);
                                        return verbName.compare (verbArgument) == 0;
                                    }) };

    if (verb == verbs.end())
    {
        const auto isFlag { not verbArgument.empty() and verbArgument.at (0) == static_cast<char> (Chars::dash) };
        const auto* failure { isFlag ? text::Diagnostics::failFlagUnknown : text::Diagnostics::failCommandUnknown };
        return Stdio::getFailure (failure, verbArgument);
    }

    const auto& [verbName, helpFileName, getVerbArguments, runVerbArguments] { *verb };
    juce::ignoreUnused (verbName);
    const auto verbArguments { getVerbArguments (arguments) };

    if (verbArguments.contains (Id::helpOption))
    {
        auto documents (getScreenErase());
        documents.add (TextDocument::fromView (getFrame (helpFileName), {}));
        return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
    }

    return runVerbArguments (verbArguments, input);
}

Stdio::Output CommandLine::run (juce::Span<const std::string_view> arguments, const std::optional<std::string>& input)
{
    if (static_cast<int> (arguments.size()) <= verbArgumentIndex)
        return getBannerAndHelp();

    const auto firstArgument { arguments[static_cast<size_t> (verbArgumentIndex)] };

    if (Arguments::matchesOption (firstArgument, Id::helpOption)
        or Arguments::matchesOption (firstArgument, Id::versionOption))
        return runGlobalOption (arguments);

    return runVerb (arguments, input);
}
