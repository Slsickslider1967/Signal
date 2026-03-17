#pragma once 

#include <string>
#include <vector>

namespace Console
{
    void AppendConsoleLine(const std::string &line);
    void ClearConsoleOutput();
    bool IsRunning();
    char *CommandBuffer();
    size_t CommandBufferSize();
    bool *AutoScrollFlag();
    std::vector<std::string> GetConsoleLinesSnapshot();
}