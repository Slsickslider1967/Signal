#include <iostream>
#include <SDL2/SDL.h>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <memory>
#include <chrono>
#include <algorithm>

#include "Audio/Audio.h"
#include "Audio/Record.h"
#include "WaveForm.h"

namespace Audio
{
    static SDL_AudioDeviceID GlobalDevice = 0;
    static std::atomic<bool> isAudioActive(false);
    static std::shared_ptr<std::vector<WaveForm>> GlobalActiveWaves;
    static int GlobalSampleRate = 44100;
    static bool isInitilized = false;
    
    static FilterCallback GlobalFilterCallback = nullptr;
    static void* GlobalFilterUserData = nullptr;

    // SDL audio callback: mix active waves, run post-filter, and optionally record output.
    static void SDLCALL AudioCallback(void* userdata, Uint8* stream, int len)
    {
            static int callCount = 0;
            ++callCount;
            if (callCount % 100 == 0) {
                std::cerr << "[debug] AudioCallback called (every 100th call)\n";
            }
    
        const int sampleCount = len / static_cast<int>(sizeof(float));
        float* outputSamples = reinterpret_cast<float*>(stream);

        // Zero output by default
        std::fill(outputSamples, outputSamples + sampleCount, 0.0f);


        // read current active waves atomically (lock-free)
        auto activeWavesSnapshot = std::atomic_load(&GlobalActiveWaves);

        if (activeWavesSnapshot && !activeWavesSnapshot->empty())
        {
            // temporary buffer reused per-wave to avoid allocating in callback
            static thread_local std::vector<float> waveSampleBuffer;
            if ((int)waveSampleBuffer.size() < sampleCount)
            {
                waveSampleBuffer.resize(sampleCount);
            }

            // Mix each enabled oscillator into the shared output stream for this callback block.
            for (size_t waveIndex = 0; waveIndex < activeWavesSnapshot->size(); ++waveIndex)
            {
                WaveForm& wave = (*activeWavesSnapshot)[waveIndex];
                if (!wave.Enabled)
                {
                    continue;
                }

                wave.SampleRate = GlobalSampleRate;

                GetWaveFormData(wave, waveSampleBuffer.data(), sampleCount, 0);

                // add to output
                for (int sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
                {
                    outputSamples[sampleIndex] += waveSampleBuffer[sampleIndex];
                }
            }
        }
        
        if (GlobalFilterCallback)
        {
            GlobalFilterCallback(outputSamples, sampleCount, GlobalFilterUserData);
        }

        // Normalize the block if peak goes too high to avoid hard clipping artifacts.
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

        // Capture the exact post-processed playback stream for WAV recording.
        Record::RecordSamples(outputSamples, static_cast<size_t>(sampleCount));

        // Debug: print sum of output buffer after all processing
        float sum = 0.0f;
        for (int i = 0; i < sampleCount; ++i) {
            sum += outputSamples[i];
        }
        if (callCount % 100 == 0) {
            std::cerr << "[debug] AudioCallback output buffer sum: " << sum << "\n";
        }
    }

    // Initialize SDL audio and open the playback device with float samples.
    void Init()
    {
        if (isInitilized) return;
        if (SDL_Init(SDL_INIT_AUDIO) < 0)
        {
            std::cerr << "Failed to initialize SDL audio: " << SDL_GetError() << std::endl;
            return;
        }

        SDL_AudioSpec requestedSpec;
        requestedSpec.freq = GlobalSampleRate;
        requestedSpec.format = AUDIO_F32SYS;
        requestedSpec.channels = 1;
        requestedSpec.samples = 12000; 
        requestedSpec.callback = AudioCallback;
        requestedSpec.userdata = nullptr;

        SDL_AudioSpec obtainedSpec;
        GlobalDevice = SDL_OpenAudioDevice(nullptr, 0, &requestedSpec, &obtainedSpec, 0);
        if (GlobalDevice == 0)
        {
            std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
            isInitilized = false;
            return;
        }

        // adopt the device's actual sample rate
        GlobalSampleRate = (obtainedSpec.freq > 0) ? obtainedSpec.freq : requestedSpec.freq;
        // initialize empty active waves list if not set
        if (!std::atomic_load(&GlobalActiveWaves))
            std::atomic_store(&GlobalActiveWaves, std::make_shared<std::vector<WaveForm>>());
        isAudioActive.store(true);
        SDL_PauseAudioDevice(GlobalDevice, 0);
        isInitilized = true;
    }

    // Close the playback device and clear all active waveform state.
    void Close()
    {
        if (!isInitilized) return;

        isAudioActive.store(false);

        if (GlobalDevice != 0)
        {
            SDL_CloseAudioDevice(GlobalDevice);
            GlobalDevice = 0;
        }

        // clear any waveform state by swapping in an empty vector
        std::atomic_store(&GlobalActiveWaves, std::make_shared<std::vector<WaveForm>>());

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        SDL_Quit();
        isInitilized = false;
    }

    // Add one waveform voice to the active playback list.
    void Play(const WaveForm& wave)
    {
        if (!isInitilized) Init();
        if (!isInitilized) return;

        // prepare a new vector based on current active waves, then atomically swap
        auto currentWaves = std::atomic_load(&GlobalActiveWaves);
        std::shared_ptr<std::vector<WaveForm>> updatedWaves;
        if (currentWaves) updatedWaves = std::make_shared<std::vector<WaveForm>>(*currentWaves);
        else updatedWaves = std::make_shared<std::vector<WaveForm>>();

        WaveForm waveCopy = wave;
        waveCopy.SampleRate = GlobalSampleRate;
        waveCopy.Enabled = true;
        updatedWaves->push_back(waveCopy);

        std::atomic_store(&GlobalActiveWaves, updatedWaves);
    }

    // Replace active voices while preserving phase continuity by matching WaveID.
    void SetWaveForms(const std::vector<WaveForm>& waves)
    {
        if (!isInitilized) Init();
        // prepare new vector and preserve existing per-wave Phase where WaveID matches
        auto currentWaves = std::atomic_load(&GlobalActiveWaves);
        auto updatedWaves = std::make_shared<std::vector<WaveForm>>(waves);
        for (auto& updatedWave : *updatedWaves) { updatedWave.SampleRate = GlobalSampleRate; }

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

        std::atomic_store(&GlobalActiveWaves, updatedWaves);
    }

    // Fill a caller-provided buffer with silence when audio is initialized.
    void WriteAudio(float* buffer, int numSamples)
    {
        if (!isInitilized) return;
        if (!buffer || numSamples <= 0) return;
        std::fill(buffer, buffer + numSamples, 0.0f);
    }
    
    // Register a post-mix processing callback invoked inside the audio thread.
    void SetFilterCallback(FilterCallback callback, void* userData)
    {
        GlobalFilterCallback = callback;
        GlobalFilterUserData = userData;
    }
}