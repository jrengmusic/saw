#pragma once
#include <JuceHeader.h>
#include "Arguments.h"
#include "Stdio.h"

class PageFollow : public juce::JUCEApplicationBase,
                   private jam::File::Watcher::Listener
{
public:
    PageFollow() = default;

    static bool isRequested (juce::Span<const std::string_view> arguments);

    const juce::String getApplicationName() override;
    const juce::String getApplicationVersion() override;
    bool moreThanOneInstanceAllowed() override;
    void initialise (const juce::String& commandLineParameters) override;
    void shutdown() override;
    void anotherInstanceStarted (const juce::String& commandLineParameters) override;
    void systemRequestedQuit() override;
    void suspended() override;
    void resumed() override;
    void unhandledException (const std::exception*, const juce::String&, int) override;
    void fileChanged (const juce::File& file, jam::File::Watcher::Event event) override;

private:
    void startFollowing (const Arguments& arguments, const juce::File& file);
    void reopen();
    void writeAppended();
    std::string readAppendedBytes();
    bool writeOutput (Stdio::Output output);

    juce::File target;
    std::unique_ptr<juce::FileInputStream> stream;
    juce::int64 offset { 0 };
    size_t remainingBytes { Stdio::outputLimitBytes };
    int printedLines { 0 };
    bool isFollowingName { false };
    jam::File::Watcher watcher;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PageFollow)
};
