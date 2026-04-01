#pragma once 

#include <cstddef>
#include <string>

namespace Record 
{
    void OpenWavForRecording(const std::string& path = "");
    void SetWavPath(const std::string& path);
    void StartRecording();
    void StopRecording();
    void SaveLastRecording();
    void RecordSamples(const float* data, size_t count);
}