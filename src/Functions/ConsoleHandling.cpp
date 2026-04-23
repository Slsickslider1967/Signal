#include <iostream>
#include <string>
#include <cstdio>
#include <atomic>
#include <array>
#include <thread>
#include <mutex>
#include <vector>
#include <list>

#include "Functions/ConsoleHandling.h"

static std::mutex GConsoleMutex;
static std::vector<std::string> GConsoleLines;
static std::atomic<bool> GConsoleRunning{false};
static std::thread GConsoleThread;
static char GConsoleCommand[256] = "";
static bool GConsoleAutoScroll = true;

namespace Console
{
    // Append one line to the in-app debug console buffer.
    void AppendConsoleLine(const std::string &line)
    {
        std::lock_guard<std::mutex> lock(GConsoleMutex);
        GConsoleLines.push_back(line);
    }

    // Clear all lines currently shown in the debug console.
    void ClearConsoleOutput()
    {
        std::lock_guard<std::mutex> lock(GConsoleMutex);
        GConsoleLines.clear();
    }

    // Report whether the console worker loop is currently active.
    bool IsRunning()
    {
        return GConsoleRunning.load();
    }

    // Return the editable command input buffer used by the UI.
    char *CommandBuffer()
    {
        return GConsoleCommand;
    }

    // Return the static command buffer capacity.
    size_t CommandBufferSize()
    {
        return sizeof(GConsoleCommand);
    }

    // Return the auto-scroll toggle used by the console view.
    bool *AutoScrollFlag()
    {
        return &GConsoleAutoScroll;
    }

    // Take a thread-safe snapshot of console lines for rendering.
    std::vector<std::string> GetConsoleLinesSnapshot()
    {
        std::lock_guard<std::mutex> lock(GConsoleMutex);
        return GConsoleLines;
    }
}