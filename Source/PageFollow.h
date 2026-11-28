#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

/** @brief Single-process JUCE application that runs `saw page --follow` and `--follow-name`.
 *
 *  Prints the initial page output of one file, then watches its parent directory with
 *  `jam::File::Watcher` and streams appended bytes through `Stdio::write` until the output
 *  bound is reached or the process exits.
 */
class PageFollow : public juce::JUCEApplicationBase,
                   private jam::File::Watcher::Listener
{
public:
    /** @brief Constructs an idle follow application. */
    PageFollow() = default;

    /** @brief Checks whether the command line invokes `page` with `--follow` or `--follow-name`.
     *
     *  @param arguments The command-line arguments without the program path.
     *
     *  @return True when the follow application should handle this invocation.
     */
    static bool isRequested (juce::Span<const std::string_view> arguments);

    /** @brief Returns the program name. */
    const juce::String getApplicationName() override;
    /** @brief Returns the program version string. */
    const juce::String getApplicationVersion() override;
    /** @brief Allows concurrent saw processes. */
    bool moreThanOneInstanceAllowed() override;
    /** @brief Parses the command line and starts following the target file.
     *
     *  @param commandLineParameters Unused; arguments are read from `getCommandLineParameterArray`.
     */
    void initialise (const juce::String& commandLineParameters) override;
    /** @brief Shuts down the follow application. */
    void shutdown() override;
    /** @brief Ignores secondary instance notifications.
     *
     *  @param commandLineParameters Unused.
     */
    void anotherInstanceStarted (const juce::String& commandLineParameters) override;
    /** @brief Exits the message loop on quit request. */
    void systemRequestedQuit() override;
    /** @brief No-op on suspend. */
    void suspended() override;
    /** @brief No-op on resume. */
    void resumed() override;
    /** @brief No-op for unhandled exception hook. */
    void unhandledException (const std::exception*, const juce::String&, int) override;
    /** @brief Handles file updates, creations, and renames from the directory watcher.
     *
     *  @param file  The changed file path.
     *  @param event The filesystem event kind.
     */
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

private:
    /** @brief Prints the initial page output, opens the target stream, and starts watching. */
    void startFollowing (const Arguments& arguments, const juce::File& file);
    /** @brief Reopens the target file at offset 0 after a create or rename event.
     *
     *  @return True when the file opened successfully.
     */
    bool reopen();
    /** @brief Reads and writes any bytes appended past the current offset. */
    void writeAppended();
    /** @brief Reads the newly appended bytes from the open stream and advances the offset.
     *
     *  @return The bytes read since the previous offset.
     */
    std::string readAppendedBytes();
    /** @brief Applies the output bound, writes the output, and quits if finished.
     *
     *  @param output The command output to write.
     *
     *  @return True when following should continue.
     */
    bool writeOutput (Stdio::Output output);

    /** @brief Resolved target file being followed. */
    juce::File target;
    /** @brief Open input stream positioned at offset. */
    std::unique_ptr<juce::FileInputStream> stream;
    /** @brief Byte offset already written from the current file. */
    juce::int64 offset { 0 };
    /** @brief Remaining byte budget before the output bound stops the process. */
    size_t remainingBytes { Stdio::outputLimitBytes };
    /** @brief Count of lines printed so far for the output-limit trailer. */
    int printedLines { 0 };
    /** @brief True when `--follow-name` reopens on create, rename, or truncation. */
    bool isFollowingName { false };
    /** @brief Directory watcher delivering events for the target's parent folder. */
    jam::File::Watcher watcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PageFollow)
};
