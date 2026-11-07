#include "pch.h"

#include "k7/skse/menu.h"

#include <atomic>

// The header resolves every function through GetProcAddress when it is first called; including it
// creates no link-time dependency. It needs <atomic>, <filesystem> and RE/ before it (pch.h).
#include <SKSEMenuFramework.h>

namespace k7::skse::menu
{
    namespace
    {
        bool g_available = false;
    }

    void Check(bool missingIsWarning)
    {
        try {
            g_available = GetMenuFrameworkModule() != nullptr;
            if (!g_available) {
                const auto* text = "ui: SKSE Menu Framework is not loaded, the in-game UI is unavailable "
                                   "(the plugin works without it)";
                if (missingIsWarning) {
                    spdlog::warn("{}", text);
                } else {
                    spdlog::info("{}", text);
                }
                return;
            }
            spdlog::info("ui: SKSE Menu Framework found, it reports version {:.2f}, API version {}",
                SKSEMenuFramework::GetMenuFrameworkVersion(), SKSEMenuFramework::GetMenuFrameworkAPIVersion());
        } catch (const std::exception& error) {
            g_available = false;
            spdlog::error("ui: unexpected failure while checking SKSE Menu Framework: {}", error.what());
        }
    }

    bool Available() noexcept
    {
        return g_available;
    }
}
