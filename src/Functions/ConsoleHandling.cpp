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
    void AppendConsoleLine(const std::string &line)
    {
        std::lock_guard<std::mutex> lock(GConsoleMutex);
        GConsoleLines.push_back(line);
    }

    // For calling in  Handler.cpp
    void ClearConsoleOutput()
    {
        std::lock_guard<std::mutex> lock(GConsoleMutex);
        GConsoleLines.clear();
    }

    bool IsRunning()
    {
        return GConsoleRunning.load();
    }

    char *CommandBuffer()
    {
        return GConsoleCommand;
    }

    size_t CommandBufferSize()
    {
        return sizeof(GConsoleCommand);
    }

    bool *AutoScrollFlag()
    {
        return &GConsoleAutoScroll;
    }

    std::vector<std::string> GetConsoleLinesSnapshot()
    {
        std::lock_guard<std::mutex> lock(GConsoleMutex);
        return GConsoleLines;
    }
}