#pragma once
#include <JuceHeader.h>
#include "generated/Generated.h"

/** @brief MCP server on standard input and output, with one tool that runs saw command lines.
 *
 *  The tool `run` runs one saw command line in a folder. The server holds no state between calls,
 *  except the process working folder that each call sets. Closing the input ends the process.
 */
class ToolServer : public juce::JUCEApplicationBase
{
public:
    /** @brief Creates the server, registers the tool `run`, and quits when the input closes. */
    ToolServer();

    /** @brief Returns the application name, which is the project name.
     *
     *  @return The project name.
     */
    const juce::String getApplicationName() override;

    /** @brief Returns the application version.
     *
     *  @return The version string of the project.
     */
    const juce::String getApplicationVersion() override;

    /** @brief Allows more than one instance of the application.
     *
     *  @return True.
     */
    bool moreThanOneInstanceAllowed() override;

    /** @brief Starts the server on standard input and output. */
    void initialise (const juce::String&) override;

    /** @brief Does nothing. */
    void shutdown() override;

    /** @brief Does nothing. */
    void anotherInstanceStarted (const juce::String&) override;

    /** @brief Quits the application when the system asks for it. */
    void systemRequestedQuit() override;

    /** @brief Does nothing when the application is suspended. */
    void suspended() override;

    /** @brief Does nothing when the application is resumed. */
    void resumed() override;

    /** @brief Does nothing on an unhandled exception. */
    void unhandledException (const std::exception*, const juce::String&, int) override;

private:
    /** @brief Runs one command line from a `run` request, and sends the result to the client.
     *
     *  The request names the folder and the arguments, and optionally the standard input. Without it, the null device is the input.
     *  The folder must be an absolute path that can be set as the working folder. Otherwise the call sends a tool error. The result holds
     *  the exit code, the output, and the errors. The bytes of the output documents are read in
     *  place and sent as one text. When that text is not valid UTF-8, the bytes are sent as one
     *  base64 text instead. The failures are sent as diagnostic lines, under the same rule.
     *
     *  @param requestId The id of the request to answer.
     *  @param document  The parsed request.
     *  @param arguments The arguments object of the request.
     */
    void runTool (const juce::String& requestId, const jam::JsonDocument& document, const jam::Document::Element& arguments);

    /** @brief The MCP server on standard input and output, with the tool table built from the server tool definitions. */
    jam::ModelContextServer server { std::string { jam::Strings::getView (BinaryData::getString (jam::Strings::fromView (files::serverTools))) }, ProjectInfo::projectName, ProjectInfo::versionString };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolServer)
};
