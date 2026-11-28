#include "Write.h"
#include "Arguments.h"
#include "Input.h"
#include "Paths.h"
#include "Stdio.h"
#include "TextDocument.h"
#include "Transaction.h"
#include "generated/Generated.h"

static std::pair<juce::Result, std::unique_ptr<TextDocument>> getOriginalDocument (bool isExisting, const juce::File& targetFile, std::string_view path)
{
    if (isExisting)
        return TextDocument::fromTextFile (targetFile, path, Input::allBytes);

    return { juce::Result::ok(), TextDocument::fromInput (nullptr, path) };
}

static std::unique_ptr<TextDocument> getPlannedDocument (bool isAppending, const TextDocument& originalDocument, std::unique_ptr<Input> standardInput, std::string_view path)
{
    if (isAppending)
    {
        auto document { TextDocument::fromInput (nullptr, path) };
        document->addSource (originalDocument.getBytes());
        document->addSource (standardInput->getBytes());
        return document;
    }

    return TextDocument::fromInput (std::move (standardInput), path);
}

static Stdio::Output runWrite (const Arguments& arguments, std::string_view path, const Input::Source& input)
{
    auto [inputStatus, standardInput] { Input::create (input, Input::allLines) };

    if (inputStatus.failed())
        return Stdio::getFailure (inputStatus);

    if (TextDocument::isBinary (standardInput->getBytes()))
        return Stdio::getFailure (TextDocument::getBinaryFailure (path));

    const auto targetFile { Paths::getLinkedTarget (path) };
    const bool isExisting { targetFile.exists() };
    auto [originalStatus, originalDocument] { getOriginalDocument (isExisting, targetFile, path) };

    if (originalStatus.failed())
        return Stdio::getFailure (originalStatus);

    const bool isAppending { isExisting and arguments.contains (Id::appendOption) };
    auto plannedDocument { getPlannedDocument (isAppending, *originalDocument, std::move (standardInput), path) };

    Transaction transaction { Id::write };
    transaction.addTarget (targetFile);
    const auto addStatus { transaction.addFile (targetFile, std::move (originalDocument), std::move (plannedDocument), isExisting, 1) };

    if (addStatus.failed())
        return Stdio::getFailure (addStatus);

    return transaction.run (arguments);
}

Arguments Write::getArguments (juce::Span<const std::string_view> argumentList)
{
    return Arguments { argumentList, { Id::applyOption, Id::appendOption }, { Id::expectOption }, {} };
}

Stdio::Output Write::run (const Arguments& arguments, const Input::Source& input)
{
    if (arguments.getStatus().failed())
        return Stdio::getFailure (arguments.getStatus());

    if (arguments.getPositionals().size() == 0)
        return Stdio::getFailure (text::Diagnostics::failMissingPath);

    if (arguments.getPositionals().size() > 1)
        return Stdio::getFailure (text::Diagnostics::failUnexpectedArgument, arguments.getPositionals().at (1));

    if (Stdio::isTerminalInput (input))
        return Stdio::getFailure (text::Diagnostics::failWriteRequiresInput);

    return runWrite (arguments, arguments.getPositionals().at (0), input);
}
