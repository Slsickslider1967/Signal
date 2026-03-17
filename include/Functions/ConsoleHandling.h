#pragma once 

#include <string>
#include <vector>

namespace Console
{
    void AppendConsoleLine(const std::string &line);
    void StartConsoleCommand(const std::string &command);
    void ClearConsoleOutput();
    bool IsRunning();
    char *CommandBuffer();
    size_t CommandBufferSize();
    bool *AutoScrollFlag();
    std::vector<std::string> GetConsoleLinesSnapshot();
}