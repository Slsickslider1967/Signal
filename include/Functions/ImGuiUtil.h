#pragma once 
#include "WaveForm.h"

namespace ImGuiUtil
{
    void Render();
    void Begin();
    void End();

    void Oscilloscope(WaveForm &wave, const char *label);
    void PlotSignal(const float *samples, int sampleCount, const char *label, float minY = -1.05f, float maxY = 1.05f);
}