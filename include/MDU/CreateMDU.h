#pragma once

#include <filesystem>

namespace MDU
{
    void CreateTemplateMDU(const std::filesystem::path &targetPath = {});
}