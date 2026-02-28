#pragma once 

#include "../WaveForm.h"

namespace Audio
{
    void InitAudio();
    void DestroyAudio();
    void PlayAudio(const WaveForm& wave);
}