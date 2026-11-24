#pragma once
#include <JuceHeader.h>
#include "../Source/generated/Generated.h"

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
     *  The request names the folder and the arguments. The folder must be an absolute path that
     *  can be set as the working folder. Otherwise the call sends a tool error. The result holds
     *  the exit code, the output, and the errors. A text that is not valid UTF-8 is sent as base64.
     *
     *  @param requestId The id of the request to answer.
     *  @param document  The parsed request.
     *  @param arguments The arguments object of the request.
     */
    void runTool (const juce::String& requestId, const jam::JsonDocument& document, const jam::Document::Element& arguments);

    jam::ModelContextServer server { BinaryData::getString (jam::Strings::fromView (files::serverTools)).toStdString(), ProjectInfo::projectName, ProjectInfo::versionString };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ToolServer)
};
