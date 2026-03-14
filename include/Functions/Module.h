#pragma once

#include <string>

#include "WaveForm.h"
#include "MDU/mdu_api.h"

struct DynamicModule
{
    int ID = -1;
    std::string Name;
    bool Active = true;

    std::string SourcePath;
    MDU::MetaData Metadata;

    MDU::Module *Instance = nullptr;
    MDU::DestroyFn Destroy = nullptr;

    int InPins = 0;
    int OutPins = 0;
};