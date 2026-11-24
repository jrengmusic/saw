#include "ToolServer.h"
#ifdef _WIN32
#include <windows.h>
#endif

static juce::JUCEApplicationBase* createToolServer()
{
    return new ToolServer();
}

static int runToolServer (const char* programPath)
{
    const char* toolArguments[] { programPath };

    juce::JUCEApplicationBase::createInstance = &createToolServer;
    return juce::JUCEApplicationBase::main (static_cast<int> (std::size (toolArguments)), toolArguments);
}

int main (int, char* argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP (CP_UTF8);
#endif

    return runToolServer (argv[0]);
}
