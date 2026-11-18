#include "k7/common/file_lock.h"
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <cwctype>
#include <string>

namespace k7::common::io
{
    FileLock::FileLock(const std::filesystem::path& path)
    {
        std::error_code ec;
        auto absolute = std::filesystem::weakly_canonical(path, ec);
        if (ec) { return; }
        auto name = absolute.native();
        for (auto& c : name) { c = static_cast<wchar_t>(std::towlower(c)); }
        // Deterministic across processes, unlike std::hash implementations with per-process seeds.
        unsigned long long hash = 14695981039346656037ull;
        for (wchar_t c : name) { hash = (hash ^ static_cast<unsigned short>(c)) * 1099511628211ull; }
        const auto mutexName = L"Local\\K7ConfigWriter-" + std::to_wstring(hash);
        handle_ = CreateMutexW(nullptr, FALSE, mutexName.c_str());
        if (handle_) {
            const auto result = WaitForSingleObject(handle_, INFINITE);
            acquired_ = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        }
    }
    FileLock::~FileLock()
    {
        if (acquired_) { ReleaseMutex(handle_); }
        if (handle_) { CloseHandle(handle_); }
    }
}
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace k7::common::io
{
    FileLock::FileLock(const std::filesystem::path& path)
    {
        auto lockPath = path;
        lockPath += ".lock";
        const int fd = open(lockPath.c_str(), O_CREAT | O_RDWR, 0600);
        if (fd >= 0) { handle_ = reinterpret_cast<void*>(static_cast<intptr_t>(fd) + 1); acquired_ = flock(fd, LOCK_EX) == 0; }
    }
    FileLock::~FileLock()
    {
        if (handle_) { const int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle_) - 1); if (acquired_) { flock(fd, LOCK_UN); } close(fd); }
    }
}
#endif
