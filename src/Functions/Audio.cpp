#include <iostream>
#include <SDL2/SDL.h>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>
#include <algorithm>

#include "../../include/Functions/Audio.h"
#include "../../include/WaveForm.h"

namespace Audio
{
    static SDL_AudioDeviceID gDevice = 0;
    static std::atomic<bool> gRunning(false);
    // lock-free active waveform list (use atomic_load/atomic_store helpers)
    static std::shared_ptr<std::vector<WaveForm>> gActiveWaves;
    static int gSampleRate = 44100;
    static bool gInitialized = false;

    // SDL audio callback: fills `stream` with `len` bytes of float samples.
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len)
    {
        const int samples = len / static_cast<int>(sizeof(float));
        float* out = reinterpret_cast<float*>(stream);

        // Zero output by default
        std::fill(out, out + samples, 0.0f);


        // read current active waves atomically (lock-free)
        auto wavesPtr = std::atomic_load(&gActiveWaves);
        if (!wavesPtr || wavesPtr->empty()) return;

        // temporary buffer reused per-wave to avoid allocating in callback
        static thread_local std::vector<float> tmpBuffer;
        if ((int)tmpBuffer.size() < samples) tmpBuffer.resize(samples);

        // mix active waves (modify per-wave Phase stored inside the active vector)
        for (size_t w = 0; w < wavesPtr->size(); ++w)
        {
            WaveForm& wave = (*wavesPtr)[w];
            if (!wave.Enabled) continue;

            wave.SampleRate = gSampleRate;

            GetWaveFormData(wave, tmpBuffer.data(), samples, 0);

            // add to output
            for (int i = 0; i < samples; ++i) out[i] += tmpBuffer[i];
        }

        // soft clipping if necessary
        float peak = 0.0f;
        for (int i = 0; i < samples; ++i)
        {
            float v = out[i];
            if (v < 0.0f) v = -v;
            if (v > peak) peak = v;
        }
        if (peak > 0.999f)
        {
            float scale = 0.9f / peak;
            for (int i = 0; i < samples; ++i) out[i] *= scale;
        }
    }

    void Init()
    {
        if (gInitialized) return;
        if (SDL_Init(SDL_INIT_AUDIO) < 0)
        {
            std::cerr << "Failed to initialize SDL audio: " << SDL_GetError() << std::endl;
            return;
        }

        SDL_AudioSpec spec;
        spec.freq = gSampleRate;
        spec.format = AUDIO_F32SYS;
        spec.channels = 1;
        spec.samples = 12000; 
        spec.callback = AudioCallback;
        spec.userdata = nullptr;

        SDL_AudioSpec obtained;
        gDevice = SDL_OpenAudioDevice(nullptr, 0, &spec, &obtained, 0);
        if (gDevice == 0)
        {
            std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
            gInitialized = false;
            return;
        }

        // adopt the device's actual sample rate
        gSampleRate = (obtained.freq > 0) ? obtained.freq : spec.freq;
        // initialize empty active waves list if not set
        if (!std::atomic_load(&gActiveWaves))
            std::atomic_store(&gActiveWaves, std::make_shared<std::vector<WaveForm>>());
        gRunning.store(true);
        SDL_PauseAudioDevice(gDevice, 0);
        gInitialized = true;
    }

    void Close()
    {
        if (!gInitialized) return;

        gRunning.store(false);

        if (gDevice != 0)
        {
            SDL_CloseAudioDevice(gDevice);
            gDevice = 0;
        }

        // clear any waveform state by swapping in an empty vector
        std::atomic_store(&gActiveWaves, std::make_shared<std::vector<WaveForm>>());

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_Quit();
        gInitialized = false;
    }

    void Play(const WaveForm& wave)
    {
        if (!gInitialized) Init();
        if (!gInitialized) return;

        // prepare a new vector based on current active waves, then atomically swap
        auto curr = std::atomic_load(&gActiveWaves);
        std::shared_ptr<std::vector<WaveForm>> next;
        if (curr) next = std::make_shared<std::vector<WaveForm>>(*curr);
        else next = std::make_shared<std::vector<WaveForm>>();

        WaveForm copy = wave;
        copy.SampleRate = gSampleRate;
        copy.Enabled = true;
        next->push_back(copy);

        std::atomic_store(&gActiveWaves, next);
    }

    void SetWaveForms(const std::vector<WaveForm>& waves)
    {
        if (!gInitialized) Init();
        // prepare new vector and preserve existing per-wave Phase where WaveID matches
        auto curr = std::atomic_load(&gActiveWaves);
        auto next = std::make_shared<std::vector<WaveForm>>(waves);
        for (auto& w : *next) { w.SampleRate = gSampleRate; }

        if (curr)
        {
            // preserve phase by matching WaveID
            for (auto& newW : *next)
            {
                for (const auto& oldW : *curr)
                {
                    if (oldW.WaveID == newW.WaveID)
                    {
                        newW.Phase = oldW.Phase;
                        break;
                    }
                }
            }
        }

        std::atomic_store(&gActiveWaves, next);
    }
}