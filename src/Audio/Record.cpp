
#include <iostream>
#include <cassert>
#include <string>
#include <chrono>
#include <filesystem>
#include <fstream>

#include <SDL2/SDL.h>

#include "Record.h"
#include "ConsoleHandling.h"

namespace Record
{    
    struct WavWriter 
    {
        std::ofstream file;
        uint32_t dataChunkPos = 0;
        uint32_t dataSize = 0;
        int sampleRate = 44100;
        int numChannels = 1;
        int bitsPerSample = 16;
        std::string filePath;

        WavWriter() = default;
        ~WavWriter() { close(); }

        bool open(const std::string &path, int rate = 44100, int channels = 1, int bits = 16) {
            close();
            filePath = path;
            sampleRate = rate;
            numChannels = channels;
            bitsPerSample = bits;
            dataSize = 0;
            file.open(filePath, std::ios::binary);
            if (!file.is_open()) return false;
            writeHeader();
            return true;
        }

        void close() {
            if (file.is_open()) {
                finalize();
                file.close();
            }
            dataSize = 0;
            filePath.clear();
        }

        bool isOpen() const { return file.is_open(); }

        void writeHeader() {
            file.seekp(0, std::ios::beg);
            file.write("RIFF", 4);
            uint32_t chunkSize = 36;
            file.write(reinterpret_cast<const char *>(&chunkSize), 4);
            file.write("WAVE", 4);
            file.write("fmt ", 4);
            uint32_t subchunk1Size = 16;
            file.write(reinterpret_cast<const char *>(&subchunk1Size), 4);
            uint16_t audioFormat = 1;
            file.write(reinterpret_cast<const char *>(&audioFormat), 2);
            uint16_t channels = numChannels;
            file.write(reinterpret_cast<const char *>(&channels), 2);
            uint32_t rate = sampleRate;
            file.write(reinterpret_cast<const char *>(&rate), 4);
            uint32_t byteRate = sampleRate * numChannels * bitsPerSample / 8;
            file.write(reinterpret_cast<const char *>(&byteRate), 4);
            uint16_t blockAlign = numChannels * bitsPerSample / 8;
            file.write(reinterpret_cast<const char *>(&blockAlign), 2);
            uint16_t bps = bitsPerSample;
            file.write(reinterpret_cast<const char *>(&bps), 2);
            file.write("data", 4);
            dataChunkPos = static_cast<uint32_t>(file.tellp());
            uint32_t dataChunkSize = 0;
            file.write(reinterpret_cast<const char *>(&dataChunkSize), 4);
        }

        void finalize() {
            if (!file.is_open()) return;
            file.seekp(4, std::ios::beg);
            uint32_t chunkSize = 36 + dataSize;
            file.write(reinterpret_cast<const char *>(&chunkSize), 4);
            file.seekp(dataChunkPos, std::ios::beg);
            file.write(reinterpret_cast<const char *>(&dataSize), 4);
            file.flush();
        }

        void WriteSamples(const float *data, size_t count) {
            if (!file.is_open()) return;
            for (size_t i = 0; i < count; ++i) {
                float sample = data[i];
                // Clamp to [-1, 1]
                if (sample > 1.0f) sample = 1.0f;
                if (sample < -1.0f) sample = -1.0f;
                int16_t pcm = static_cast<int16_t>(sample * 32767.0f);
                file.write(reinterpret_cast<const char *>(&pcm), sizeof(int16_t));
                dataSize += sizeof(int16_t);
            }
        }
    };

    static WavWriter Global_WavWriter;
    static std::vector<float> CachedRecording;
    static bool isRecording = false;
    static std::string WavPath;

    void OpenWavForRecording(const std::string &path)
    {
        WavPath = path;
        if (WavPath.empty())
        {
            WavPath = std::string(getenv("HOME") ? getenv("HOME") : "") + "/Documents/Signal/Recordings/recording_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".wav";
        }
        CachedRecording.clear();
        isRecording = false;
        Console::AppendConsoleLine("[info] Ready to record to: " + WavPath);
    }

    void SetWavPath(const std::string &path)
    {
        WavPath = path;
        Console::AppendConsoleLine("[info] Save path set to: " + WavPath);
    }

    void StartRecording()
    {
        if (WavPath.empty())
        {
            OpenWavForRecording("");
        }
        CachedRecording.clear();
        isRecording = true;
        Console::AppendConsoleLine("[info] Recording started.");
    }

    void StopRecording()
    {
        isRecording = false;
        Console::AppendConsoleLine("[info] Recording stopped. Not yet saved.");
    }

    void SaveLastRecording()
    {
        if (CachedRecording.empty() || WavPath.empty())
        {
            Console::AppendConsoleLine("[warning] No recording to save.");
            return;
        }

        std::string Error = "[info] Saving recording to: " + WavPath;
        Console::AppendConsoleLine(Error);
        std::cerr << Error << std::endl;

        size_t lastSlash = WavPath.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            std::string dirPath = WavPath.substr(0, lastSlash);
            std::error_code ec;
            std::filesystem::create_directories(dirPath, ec);
            if (ec)
            {
                std::string errorMessage = "Failed to create directory: " + dirPath + " (" + ec.message() + ")";
                std::cerr << "[ERROR] " << errorMessage << std::endl;
                Console::AppendConsoleLine("[ERROR] " + errorMessage);
                return;
            }

            std::string successMessage = "Directory created or already exists: " + dirPath;
            std::cerr << "[info] " << successMessage << std::endl;
            Console::AppendConsoleLine("[info] " + successMessage);
        }

        if (Global_WavWriter.open(WavPath))
        {
            Global_WavWriter.WriteSamples(CachedRecording.data(), CachedRecording.size());
            Global_WavWriter.close();
            Console::AppendConsoleLine("[info] Recording saved to: " + WavPath);
        }
        else
        {
            Console::AppendConsoleLine("[ERROR] Failed to save recording to: " + WavPath);
            std::cerr << "[ERROR] Failed to save recording to: " + WavPath << std::endl;
        }
        CachedRecording.clear();
        WavPath.clear();
    }

    // Call this from your audio callback when isRecording is true
    void RecordSamples(const float* data, size_t count)
    {
        if (isRecording)
        {
            CachedRecording.insert(CachedRecording.end(), data, data + count);
        }
    }
}
