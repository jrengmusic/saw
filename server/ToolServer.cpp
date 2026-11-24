#include "ToolServer.h"
#include "../Source/CommandLine.h"

static constexpr std::string_view toolName { "run" };
static constexpr std::string_view directoryMember { "directory" };
static constexpr std::string_view standardInputMember { "stdin" };
static constexpr std::string_view exitCodeMember { "exitCode" };
static constexpr std::string_view outputMember { "output" };
static constexpr std::string_view outputBase64Member { "outputBase64" };
static constexpr std::string_view errorsMember { "errors" };
static constexpr std::string_view errorsBase64Member { "errorsBase64" };
static constexpr std::string_view directoryError { "directory is not an absolute folder path" };

ToolServer::ToolServer()
{
    server.addTool (toolName, [this] (const juce::Identifier&, const juce::String& requestId, const jam::JsonDocument& document, const jam::Document::Element& arguments)
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

void ToolServer::runTool (const juce::String& requestId, const jam::JsonDocument& document, const jam::Document::Element& arguments)
{
    const auto directory { jam::Strings::fromView (document.getString (document.get (arguments, directoryMember))) };

    if (juce::File::isAbsolutePath (directory) and juce::File { directory }.setAsCurrentWorkingDirectory())
    {
        const auto& argumentTokens { *document.getChildByToken (arguments, document.get (arguments, mcp::Member::arguments))->get<jam::Document::Tokens> (Id::tokens) };
        jam::Strings commandArguments;

        for (int index { 0 }; index < argumentTokens.size(); ++index)
            commandArguments.add (document.getString (argumentTokens.at (index)));

        const std::optional<std::string> standardInput { document.contains (arguments, standardInputMember) ? document.getString (document.get (arguments, standardInputMember)) : std::string {} };
        const auto commandResult { CommandLine::run (commandArguments, standardInput) };

        const auto appendText { [] (std::string& members, std::string_view plainName, std::string_view encodedName, std::string_view text)
        {
            if (juce::CharPointer_UTF8::isValidString (text.data(), static_cast<int> (text.size())))
            {
                jam::JsonWriter::appendKey (members, plainName);
                jam::JsonWriter::appendString (members, text);
            }
            else
            {
                const auto encodedText { juce::Base64::toBase64 (text.data(), text.size()) };
                jam::JsonWriter::appendKey (members, encodedName);
                jam::JsonWriter::appendString (members, jam::Strings::getView (encodedText));
            }
        } };

        std::string json;

        jam::JsonWriter::appendObject (json, [&commandResult, &appendText] (std::string& members)
        {
            const auto& [exitCode, standardOutput, standardError] { commandResult };
            jam::JsonWriter::appendKey (members, exitCodeMember);
            jam::Format::appendDecimal (members, exitCode);
            appendText (members, outputMember, outputBase64Member, standardOutput);
            appendText (members, errorsMember, errorsBase64Member, standardError);
        });

        server.sendResponse (requestId, json);
    }
    else
    {
        server.sendToolError (requestId, directoryError);
    }
}
