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
    
    // Filter callback for rack processing
    static FilterCallback gFilterCallback = nullptr;
    static void* gFilterUserData = nullptr;

    // SDL audio callback: fills `stream` with `len` bytes of float samples.
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len)
    {
        const int sampleCount = len / static_cast<int>(sizeof(float));
        float* outputSamples = reinterpret_cast<float*>(stream);

        // Zero output by default
        std::fill(outputSamples, outputSamples + sampleCount, 0.0f);


        // read current active waves atomically (lock-free)
        auto activeWavesSnapshot = std::atomic_load(&gActiveWaves);
        if (!activeWavesSnapshot || activeWavesSnapshot->empty()) return;

        // temporary buffer reused per-wave to avoid allocating in callback
        static thread_local std::vector<float> waveSampleBuffer;
        if ((int)waveSampleBuffer.size() < sampleCount) waveSampleBuffer.resize(sampleCount);

        // mix active waves (modify per-wave Phase stored inside the active vector)
        for (size_t waveIndex = 0; waveIndex < activeWavesSnapshot->size(); ++waveIndex)
        {
            WaveForm& wave = (*activeWavesSnapshot)[waveIndex];
            if (!wave.Enabled) continue;

            wave.SampleRate = gSampleRate;

            GetWaveFormData(wave, waveSampleBuffer.data(), sampleCount, 0);

            // add to output
            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
                outputSamples[sampleIndex] += waveSampleBuffer[sampleIndex];
        }
        
        if (gFilterCallback)
        {
            gFilterCallback(outputSamples, sampleCount, gFilterUserData);
        }

        // soft clipping if necessary
        float peak = 0.0f;
        for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
        {
            float absoluteSample = outputSamples[sampleIndex];
            if (absoluteSample < 0.0f) absoluteSample = -absoluteSample;
            if (absoluteSample > peak) peak = absoluteSample;
        }
        if (peak > 0.999f)
        {
            float scale = 0.9f / peak;
            for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
                outputSamples[sampleIndex] *= scale;
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

        SDL_AudioSpec requestedSpec;
        requestedSpec.freq = gSampleRate;
        requestedSpec.format = AUDIO_F32SYS;
        requestedSpec.channels = 1;
        requestedSpec.samples = 12000; 
        requestedSpec.callback = AudioCallback;
        requestedSpec.userdata = nullptr;

        SDL_AudioSpec obtainedSpec;
        gDevice = SDL_OpenAudioDevice(nullptr, 0, &requestedSpec, &obtainedSpec, 0);
        if (gDevice == 0)
        {
            std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
            gInitialized = false;
            return;
        }

        // adopt the device's actual sample rate
        gSampleRate = (obtainedSpec.freq > 0) ? obtainedSpec.freq : requestedSpec.freq;
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
        auto currentWaves = std::atomic_load(&gActiveWaves);
        std::shared_ptr<std::vector<WaveForm>> updatedWaves;
        if (currentWaves) updatedWaves = std::make_shared<std::vector<WaveForm>>(*currentWaves);
        else updatedWaves = std::make_shared<std::vector<WaveForm>>();

        WaveForm waveCopy = wave;
        waveCopy.SampleRate = gSampleRate;
        waveCopy.Enabled = true;
        updatedWaves->push_back(waveCopy);

        std::atomic_store(&gActiveWaves, updatedWaves);
    }

    void SetWaveForms(const std::vector<WaveForm>& waves)
    {
        if (!gInitialized) Init();
        // prepare new vector and preserve existing per-wave Phase where WaveID matches
        auto currentWaves = std::atomic_load(&gActiveWaves);
        auto updatedWaves = std::make_shared<std::vector<WaveForm>>(waves);
        for (auto& updatedWave : *updatedWaves) { updatedWave.SampleRate = gSampleRate; }

        if (currentWaves)
        {
            // preserve phase by matching WaveID
            for (auto& updatedWave : *updatedWaves)
            {
                for (const auto& existingWave : *currentWaves)
                {
                    if (existingWave.WaveID == updatedWave.WaveID)
                    {
                        updatedWave.Phase = existingWave.Phase;
                        break;
                    }
                }
            }
        }

        std::atomic_store(&gActiveWaves, updatedWaves);
    }

    void WriteAudio(float* buffer, int numSamples)
    {
        if (!gInitialized) return;
        if (!buffer || numSamples <= 0) return;
        std::fill(buffer, buffer + numSamples, 0.0f);
    }
    
    void SetFilterCallback(FilterCallback callback, void* userData)
    {
        gFilterCallback = callback;
        gFilterUserData = userData;
    }
}