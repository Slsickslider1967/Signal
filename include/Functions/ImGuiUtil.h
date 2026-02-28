#pragma once 
#include "WaveForm.h"

namespace ImGuiUtil
{
    void Render();
    void Begin();
    void End();

    void Oscilloscope(WaveForm &wave, const char *label);
}