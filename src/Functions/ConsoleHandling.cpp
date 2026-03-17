#include <iostream>
#include <string>
#include <cstdio>
#include <atomic>
#include <array>
#include <thread>
#include <mutex>
#include <vector>
#include <list>

#include "../../include/Functions/ConsoleHandling.h"

static std::mutex GConsoleMutex;
static std::vector<std::string> GConsoleLines;
static std::atomic<bool> GConsoleRunning{false};
static std::thread GConsoleThread;
static char GConsoleCommand[256] = "cmake --build build -j4";
static bool GConsoleAutoScroll = true;

namespace Console
{
    void AppendConsoleLine(const std::string &line)
    {
        std::lock_guard<std::mutex> lock(GConsoleMutex);
        GConsoleLines.push_back(line);
    }

    void StartConsoleCommand(const std::string &command)
    {
        if (GConsoleRunning.load())
        {
            AppendConsoleLine("A console command is already running.");
            return;
        }

        GConsoleRunning = true;

        if (GConsoleThread.joinable())
        {
            GConsoleThread.join();
        }

        GConsoleThread = std::thread([command]()
                                     {
        std::string fullCmd = command + " 2>&1";
        FILE *pipe = popen(fullCmd.c_str(), "r");
        if (pipe == nullptr)
        {
            AppendConsoleLine("[error] failed to start command");
            GConsoleRunning = false;
            return;
        }

        std::array<char, 512> buffer{};
        while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr)
        {
            std::string line(buffer.data());

            if (!line.empty() && line.back() == '\n')
            {
                line.pop_back();
            }

            AppendConsoleLine(line);
        }

        int rc = pclose(pipe);
        AppendConsoleLine("[exit] code = " + std::to_string(rc));
        GConsoleRunning = false; });

        GConsoleThread.detach();
    }

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