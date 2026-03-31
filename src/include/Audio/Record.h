#pragma once 

namespace Record 
{
    void OpenWavForRecording(const std::string& path = "");
    void SetWavPath(const std::string& path);
    void StartRecording();
    void StopRecording();
    void SaveLastRecording();
}