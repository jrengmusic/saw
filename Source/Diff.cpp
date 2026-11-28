#include "Diff.h"
#include "LineDiff.h"
#include "Paths.h"
#include "TextDocument.h"
#include "generated/Generated.h"

static std::pair<juce::Result, std::unique_ptr<Input>> getSideInput (std::string_view path, const Input::Source& input)
{
    if (path.compare (Id::stdinArgument) == 0)
        return Input::create (input, Input::allLines);

    return TextDocument::getInput (Paths::getLinkedTarget (path), path, Input::allBytes);
}

static juce::Result getFileStatus (const juce::File& file, std::string_view path)
{
    if (file.existsAsFile())
        return juce::Result::ok();

    return TextDocument::getReadFailure (file, path);
}

static Stdio::Output getBytesQuietOutput (std::string_view firstPath, std::string_view secondPath, const Input::Source& input)
{
    const auto [firstStatus, firstInput] { getSideInput (firstPath, input) };

    if (firstStatus.failed())
        return Stdio::getFailure (firstStatus);

    const auto [secondStatus, secondInput] { getSideInput (secondPath, input) };

    if (secondStatus.failed())
        return Stdio::getFailure (secondStatus);

    const bool isIdentical { firstInput->getBytes().compare (secondInput->getBytes()) == 0 };
    return Stdio::getOutput (isIdentical ? Stdio::exitSuccess : Stdio::exitMismatch);
}

static Stdio::Output getQuietOutput (std::string_view firstPath, std::string_view secondPath, const Input::Source& input)
{
    const bool isBothFiles { firstPath.compare (Id::stdinArgument) != 0 and secondPath.compare (Id::stdinArgument) != 0 };

    if (isBothFiles)
    {
        const auto firstFile { Paths::getLinkedTarget (firstPath) };
        const auto secondFile { Paths::getLinkedTarget (secondPath) };
        const auto firstStatus { getFileStatus (firstFile, firstPath) };

        if (firstStatus.failed())
            return Stdio::getFailure (firstStatus);

        const auto secondStatus { getFileStatus (secondFile, secondPath) };

        if (secondStatus.failed())
            return Stdio::getFailure (secondStatus);

        const bool isIdentical { firstFile.hasIdenticalContentTo (secondFile) };
        return Stdio::getOutput (isIdentical ? Stdio::exitSuccess : Stdio::exitMismatch);
    }

    return getBytesQuietOutput (firstPath, secondPath, input);
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getStandardInputDocument (const Input::Source& input)
{
    auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return { inputStatus, nullptr };

    if (TextDocument::isBinary (standardInput->getBytes()))
        return { TextDocument::getBinaryFailure (Id::stdinArgument), nullptr };

    return { juce::Result::ok(), TextDocument::fromInput (std::move (standardInput), Id::stdinArgument) };
}

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getTextDocument (std::string_view path, const Input::Source& input)
{
    if (path.compare (Id::stdinArgument) == 0)
        return getStandardInputDocument (input);

    return TextDocument::fromTextFile (Paths::getLinkedTarget (path), path, Input::allBytes);
}

static Stdio::Output getTextOutput (std::string_view firstPath, std::string_view secondPath, const Input::Source& input)
{
    const auto [originalStatus, original] { getTextDocument (firstPath, input) };

    if (originalStatus.failed())
        return Stdio::getFailure (originalStatus);

    const auto [targetStatus, target] { getTextDocument (secondPath, input) };

    if (targetStatus.failed())
        return Stdio::getFailure (targetStatus);

    std::string diffText;
    LineDiff (*original, *target).appendUnifiedDiff (diffText, *original, target->getBytes(), target->getDisplayPath(), Stdio::newline);

    if (diffText.empty())
        return Stdio::getOutput (Stdio::exitSuccess);

    return Stdio::getOutput (Stdio::exitMismatch, Stdio::getDocuments (std::move (diffText)));
}

static Stdio::Output getDiffOutput (const Arguments& arguments, std::string_view firstPath, std::string_view secondPath, const Input::Source& input)
{
    const bool isFirstStandard { firstPath.compare (Id::stdinArgument) == 0 };
    const bool isSecondStandard { secondPath.compare (Id::stdinArgument) == 0 };

    if (isFirstStandard and isSecondStandard)
        return Stdio::getFailure (text::Diagnostics::failUnexpectedArgument, secondPath);

    if ((isFirstStandard or isSecondStandard) and Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    if (arguments.contains (Id::quietOption))
        return getQuietOutput (firstPath, secondPath, input);

    return getTextOutput (firstPath, secondPath, input);
}

Arguments Diff::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList, { Id::quietOption }, {}, {} };
}

Stdio::Output Diff::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    if (arguments.getPositionals().size() < 2)
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    if (arguments.getPositionals().size() > 2)
        return Stdio::getFailure (text::Diagnostics::failUnexpectedArgument, arguments.getPositionals().at (2));

    return getDiffOutput (arguments, arguments.getPositionals().at (0), arguments.getPositionals().at (1), input);
}
