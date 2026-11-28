#include "ToolServer.h"
#include "CommandLine.h"

ToolServer::ToolServer()
{
    server.addTool (Id::run, [this] (const juce::Identifier&, const juce::String& requestId, const jam::JsonDocument& document, const jam::Document::Element& arguments)
    {
        runTool (requestId, document, arguments);
    });

    server.onInputClosed = [this]
    {
        quit();
    };
}

const juce::String ToolServer::getApplicationName()
{
    return ProjectInfo::projectName;
}

const juce::String ToolServer::getApplicationVersion()
{
    return ProjectInfo::versionString;
}

bool ToolServer::moreThanOneInstanceAllowed()
{
    return true;
}

void ToolServer::initialise (const juce::String&)
{
    server.start();
}

void ToolServer::shutdown()
{
}

void ToolServer::anotherInstanceStarted (const juce::String&)
{
}

void ToolServer::systemRequestedQuit()
{
    quit();
}

void ToolServer::suspended()
{
}

void ToolServer::resumed()
{
}

void ToolServer::unhandledException (const std::exception*, const juce::String&, int)
{
}

template <typename Function>
static bool isValidText (const Function& applyText)
{
    bool isValid { true };
    applyText ([&isValid] (std::string_view text) { isValid = isValid and jam::Utf8::isValid (text); });
    return isValid;
}

template <typename Function>
static void setText (jam::ModelContextServer::Content& content, std::string_view plainName, std::string_view encodedName, const Function& applyText, bool isValid)
{
    if (isValid)
        content.setText (plainName, applyText);
    else
        content.setBase64 (encodedName, applyText);
}

void ToolServer::runTool (const juce::String& requestId, const jam::JsonDocument& document, const jam::Document::Element& arguments)
{
    const auto directory { document.getString (document.get (arguments, jam::Strings::getView (Id::directory.toString()))) };

    const auto directoryPath { jam::Strings::fromView (directory) };

    if (juce::File::isAbsolutePath (directoryPath) and juce::File { directoryPath }.setAsCurrentWorkingDirectory())
    {
        const auto& argumentTokens { *document.getChildByToken (arguments, document.get (arguments, mcp::Member::arguments))->get<jam::Document::Tokens> (Id::tokens) };
        jam::Array<std::string_view> commandArguments;
        commandArguments.ensureStorageAllocated (argumentTokens.size());

        for (int index { 0 }; index < argumentTokens.size(); ++index)
            commandArguments.add (document.getString (argumentTokens.at (index)));

        const juce::Span<const std::string_view> span { commandArguments.data(), static_cast<size_t> (commandArguments.size()) };
        const Input::Source standardInput { document.contains (arguments, Id::standardInput) ? Input::Source { document.getString (document.get (arguments, Id::standardInput)) } : Input::Source { Input::NullDevice {} } };
        const auto commandResult { CommandLine::run (span, standardInput) };

        const auto& [exitCode, documents, failures, errorDocuments] { commandResult };
        static constexpr std::array<std::string_view, 3> statusValues { Id::okValue, Id::mismatchValue, Id::errorValue };
        const auto statusText { statusValues.at (static_cast<size_t> (juce::jlimit (Stdio::exitSuccess, Stdio::exitError, exitCode))) };
        const auto applyOutput { [&documents = documents] (const auto& appendText)
        {
            for (const auto& outputDocument : documents)
                appendText (outputDocument->getBytes());
        } };
        const auto applyErrors { [&failures = failures, &errorDocuments = errorDocuments] (const auto& appendText)
        {
            for (const auto& errorDocument : errorDocuments)
                appendText (errorDocument->getBytes());

            for (const auto& failure : failures)
                Stdio::applyToPieces (failure, appendText);
        } };
        const bool isOutputValid { isValidText (applyOutput) };
        const bool areErrorsValid { isValidText (applyErrors) };

        server.sendResponse (requestId, [statusText, &applyOutput, &applyErrors, isOutputValid, areErrorsValid] (jam::ModelContextServer::Content& content)
        {
            content.setText (Id::status, [statusText] (const auto& appendText) { appendText (statusText); });
            setText (content, Id::output, Id::outputBase64, applyOutput, isOutputValid);
            setText (content, Id::errors, Id::errorsBase64, applyErrors, areErrorsValid);
        });
    }
    else
    {
        server.sendToolError (requestId, text::Diagnostics::failDirectory);
    }
}
