#pragma once

#include <filesystem>

namespace k7::common::io
{
    // Serializes cooperating writers for one destination across processes.
    class FileLock
    {
    public:
        explicit FileLock(const std::filesystem::path& path);
        ~FileLock();
        FileLock(const FileLock&) = delete;
        FileLock& operator=(const FileLock&) = delete;
        explicit operator bool() const noexcept { return acquired_; }
    private:
        void* handle_{};
        bool acquired_{};
    };
}
