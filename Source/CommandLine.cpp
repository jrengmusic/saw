#include "CommandLine.h"
#include "Arguments.h"
#include "Excerpt.h"
#include "Fields.h"
#include "Find.h"
#include "Json.h"
#include "Lines.h"
#include "Page.h"
#include "Patch.h"
#include "Replace.h"
#include "TextDocument.h"
#include "Undo.h"
#include "Write.h"
#include "generated/Generated.h"

static constexpr int verbArgumentIndex { 0 };

static std::string_view getResource (std::string_view resourceFileName)
{
    const auto [data, size] { BinaryData::fetcher (resourceFileName.data()) };
    assert (data != nullptr);
    return { static_cast<const char*> (data), static_cast<size_t> (size) };
}

static Stdio::Documents getScreenErase()
{
    Stdio::Documents screenErase;
    const auto frame { Stdio::isTerminalOutput() ? getResource (files::screenErase) : std::string_view {} };

    if (not frame.empty())
        screenErase.add (TextDocument::fromView (frame, {}));

    return screenErase;
}

static Stdio::Documents getHelpDocuments (std::string_view frameFileName, std::string_view documentFileName)
{
    auto documents (getScreenErase());
    documents.add (TextDocument::fromView (getResource (Stdio::isTerminalOutput() ? frameFileName : documentFileName), {}));
    return documents;
}

static void appendBanner (std::string& output)
{
    for (const auto text : map::banner)
    {
        jam::Format::appendBytes (output, text);
        jam::Format::appendUtf8 (output, Chars::newline);
    }
}

static Stdio::Output getBannerAndHelp()
{
    if (Stdio::isTerminalOutput())
    {
        auto documents (getScreenErase());
        std::string banner;
        appendBanner (banner);
        jam::Format::appendUtf8 (banner, Chars::newline);
        documents.add (TextDocument::fromText (std::move (banner), {}));
        documents.add (TextDocument::fromView (getResource (files::sawHelp), {}));
        return Stdio::getOutput (Stdio::exitSuccess, std::move (documents));
    }

    return Stdio::getOutput (Stdio::exitSuccess, getHelpDocuments (files::sawHelp, files::sawDocument));
}

static Stdio::Output runGlobalOption (juce::Span<const std::string_view> arguments)
{
    if (static_cast<int> (arguments.size()) > Arguments::firstOptionIndex)
        return Stdio::getFailure (text::Diagnostics::failFlagUnknown, arguments[static_cast<size_t> (Arguments::firstOptionIndex)]);

    if (Arguments::matchesOption (arguments[static_cast<size_t> (verbArgumentIndex)], Id::versionOption))
        return Stdio::getOutput (Stdio::exitSuccess, Stdio::getDocuments (Stdio::getVersionLine()));

    return getBannerAndHelp();
}

static Stdio::Output runVerb (juce::Span<const std::string_view> arguments, const Input::Source& input)
{
    const auto verbArgument { arguments[static_cast<size_t> (verbArgumentIndex)] };
    static const std::array verbs
    {
        std::make_tuple (Id::find, files::findHelp, files::findDocument, &Find::getArguments, &Find::run),
        std::make_tuple (Id::replace, files::replaceHelp, files::replaceDocument, &Replace::getArguments, &Replace::run),
        std::make_tuple (Id::lines, files::linesHelp, files::linesDocument, &Lines::getArguments, &Lines::run),
        std::make_tuple (Id::fields, files::fieldsHelp, files::fieldsDocument, &Fields::getArguments, &Fields::run),
        std::make_tuple (Id::patch, files::patchHelp, files::patchDocument, &Patch::getArguments, &Patch::run),
        std::make_tuple (Id::write, files::writeHelp, files::writeDocument, &Write::getArguments, &Write::run),
        std::make_tuple (Id::json, files::jsonHelp, files::jsonDocument, &Json::getArguments, &Json::run),
        std::make_tuple (jam::Strings::getView (Id::page.toString()), files::pageHelp, files::pageDocument, &Page::getArguments, &Page::run),
        std::make_tuple (Id::undo, files::undoHelp, files::undoDocument, &Undo::getArguments, &Undo::run),
        std::make_tuple (Id::excerpt, files::excerptHelp, files::excerptDocument, &Excerpt::getArguments, &Excerpt::run)
    };

    const auto verb { std::find_if (verbs.begin(), verbs.end(),
                                    [&verbArgument] (const auto& entry)
                                    {
                                        const auto& [verbName, helpFileName, documentFileName, getVerbArguments, runVerbArguments] { entry };
                                        juce::ignoreUnused (helpFileName, documentFileName, getVerbArguments, runVerbArguments);
                                        return verbName.compare (verbArgument) == 0;
                                    }) };

    if (verb == verbs.end())
    {
        const auto isFlag { not verbArgument.empty() and verbArgument.at (0) == static_cast<char> (Chars::dash) };
        const auto* failure { isFlag ? text::Diagnostics::failFlagUnknown : text::Diagnostics::failCommandUnknown };
        return Stdio::getFailure (failure, verbArgument);
    }

    const auto& [verbName, helpFileName, documentFileName, getVerbArguments, runVerbArguments] { *verb };
    juce::ignoreUnused (verbName);
    const auto verbArguments { getVerbArguments (arguments) };

    if (verbArguments.contains (Id::helpOption))
        return Stdio::getOutput (Stdio::exitSuccess, getHelpDocuments (helpFileName, documentFileName));

    return runVerbArguments (verbArguments, input);
}

Stdio::Output CommandLine::run (juce::Span<const std::string_view> arguments, const Input::Source& input)
{
    if (static_cast<int> (arguments.size()) <= verbArgumentIndex)
        return getBannerAndHelp();

    const auto firstArgument { arguments[static_cast<size_t> (verbArgumentIndex)] };

    if (Arguments::matchesOption (firstArgument, Id::helpOption)
        or Arguments::matchesOption (firstArgument, Id::versionOption))
        return runGlobalOption (arguments);

    return runVerb (arguments, input);
}
