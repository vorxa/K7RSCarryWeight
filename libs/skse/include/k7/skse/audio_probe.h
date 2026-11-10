#pragma once
#include <string>
namespace k7::skse::audio_probe
{
    // Diagnostic only. Call on the game thread. Paths are relative to Data/Sound.
    void PlayFile(std::string path, bool allowOverlap);
    void PlayFileCategory(std::string path, int category, bool allowOverlap);
    void PlayFilePair(std::string path, bool allowOverlap);
    void PlayDescriptor(std::string formKey, bool allowOverlap);
    void Poll();
    void Stop();
    void StartPolling();
}
