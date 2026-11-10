#include "pch.h"
#include "k7/skse/audio_probe.h"
#include "k7/skse/form_lookup.h"
#include "k7/common/form_key.h"
#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <atomic>
#include <thread>

// CommonLib declares but does not define this empty interface destructor. Used only by the
// diagnostic adapter below; the adapter never destroys or modifies an engine-owned descriptor.
RE::BSISoundDescriptor::~BSISoundDescriptor() = default;

namespace k7::skse::audio_probe
{
    namespace
    {
        using Clock = std::chrono::steady_clock;
        class FileDescriptor final : public RE::BSISoundDescriptor
        {
        public:
            RE::BSISoundDescriptor* original{};
            RE::BSResource::ID resource{};
            RE::BSISoundCategory* category{};
            bool replaceResource{ true };
            bool DoResolve(Resolution& resolution) override
            {
                if (!original->DoResolve(resolution)) { return false; }
                if (replaceResource) { resolution.resourceID = resource; }
                resolution.soundCategory = category;
                spdlog::info("audio probe: resolved form={:08X} alternate={:08X} flags={:08X} resource_file={:08X} resource_dir={:08X}",
                    resolution.formID, resolution.alternateFormID, resolution.flags,
                    resolution.resourceID.file, resolution.resourceID.dir);
                return true;
            }
            void Unk_02() override { original->Unk_02(); }
        };
        struct Voice
        {
            RE::BSSoundHandle handle;
            std::string source;
            Clock::time_point lastPoll;
            Clock::duration unmutedWait{};
            Clock::time_point resumeUntil{};
            RE::BGSSoundCategory* category{};
            bool seenPlaying{};
            bool wasMenuMuted{};
        };
        std::vector<Voice> g_voices;
        std::atomic_bool g_pollQueued{};

        void Track(RE::BSSoundHandle handle, std::string source, RE::BGSSoundCategory* category = nullptr)
        {
            const auto valid = handle.IsValid();
            const auto played = valid && handle.Play();
            spdlog::info("audio probe: source={} id={} valid={} Play={}", source, handle.soundID, valid, played);
            if (played) {
                g_voices.push_back(Voice{ handle, std::move(source), Clock::now(), {}, {}, category, false, false });
            }
        }

        // Engine retention is unknown: keep one immutable adapter per resource/category for the
        // process lifetime. Repeated playback must not allocate another descriptor or exhaust a
        // per-playback quota. Never evict while the engine might still reference an adapter.
        std::unordered_map<std::string, std::unique_ptr<FileDescriptor>> g_descriptors;
        std::jthread g_pollThread; // destroyed before handles, adapters and queue flag

        bool Ready(bool overlap)
        {
            Poll();
            if ((!overlap && !g_voices.empty()) || g_voices.size() >= 8) {
                const auto starting = std::ranges::count_if(g_voices, [](const Voice& voice) { return !voice.seenPlaying; });
                spdlog::info("audio probe: request skipped, {} starting/playing handle(s), overlap={}, starting={}", g_voices.size(), overlap, starting);
                return false;
            }
            return true;
        }
    }

    void StartPolling()
    {
        if (g_pollThread.joinable()) { return; }
        // Only the game task touches handles and engine forms. The timer thread schedules at
        // most one outstanding task, including while loading prevents the game queue advancing.
        g_pollThread = std::jthread([](std::stop_token stop) {
            while (!stop.stop_requested()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if (stop.stop_requested()) { break; }
                if (!g_pollQueued.exchange(true)) {
                    SKSE::GetTaskInterface()->AddTask([] {
                        try { Poll(); }
                        catch (const std::exception& error) {
                            spdlog::error("audio probe: polling failure: {}", error.what());
                        }
                        g_pollQueued = false;
                    });
                }
            }
        });
    }

    void Poll()
    {
        std::erase_if(g_voices, [](Voice& voice) {
            const auto now = Clock::now();
            auto* ui = RE::UI::GetSingleton();
            const bool menuMuted = voice.category && ui && ui->GameIsPaused() &&
                voice.category->GetCategoryAttenuation() > 0;
            if (!menuMuted && !voice.wasMenuMuted && !voice.seenPlaying) {
                voice.unmutedWait += now - voice.lastPoll;
            }
            voice.lastPoll = now;
            if (menuMuted != voice.wasMenuMuted) {
                spdlog::info("audio probe: menu attenuation id={} active={}", voice.handle.soundID, menuMuted);
                if (!menuMuted) { voice.resumeUntil = now + std::chrono::seconds(2); }
            }
            voice.wasMenuMuted = menuMuted;
            const bool valid = voice.handle.IsValid();
            const bool playing = valid && voice.handle.IsPlaying();
            if (playing && !voice.seenPlaying) {
                voice.seenPlaying = true;
                spdlog::info("audio probe: playing source={} id={} duration_raw={}", voice.source,
                    voice.handle.soundID, voice.handle.GetDuration());
            }
            if (!playing && (!valid || (!menuMuted && now >= voice.resumeUntil && (voice.seenPlaying || voice.unmutedWait > std::chrono::seconds(5))))) {
                spdlog::info("audio probe: finished source={} id={} observed_playing={} valid={}",
                    voice.source, voice.handle.soundID, voice.seenPlaying, valid);
                auto* manager = RE::BSAudioManager::GetSingleton();
                spdlog::info("audio probe: finish context master={} paused={}",
                    manager ? manager->masterVolume : -1.0F, ui && ui->GameIsPaused());
                if (valid) { voice.handle.Stop(); }
                return true;
            }
            return false;
        });
    }

    void Stop()
    {
        for (auto& voice : g_voices) {
            if (voice.handle.IsValid()) { voice.handle.Stop(); }
        }
        spdlog::info("audio probe: stop {} tracked handle(s)", g_voices.size());
        g_voices.clear();
    }

    void PlayFilePair(std::string path, bool allowOverlap)
    {
        spdlog::info("audio probe: immediate pair begin overlap={}", allowOverlap);
        PlayFileCategory(path, 1, allowOverlap);
        const auto starting = std::ranges::count_if(g_voices, [](const Voice& voice) { return !voice.seenPlaying; });
        spdlog::info("audio probe: immediate pair before second tracked={} starting={}", g_voices.size(), starting);
        PlayFileCategory(std::move(path), 1, allowOverlap);
        spdlog::info("audio probe: immediate pair end tracked={}", g_voices.size());
    }

    void PlayFile(std::string path, bool allowOverlap)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        const std::filesystem::path relative(path);
        if (path.empty() || relative.is_absolute() || path.find(':') != std::string::npos ||
            std::ranges::any_of(relative, [](const auto& part) { return part == ".."; })) {
            spdlog::error("audio probe: invalid relative sound path: {}", path);
            return;
        }
        const auto extension = relative.extension().string();
        if (extension != ".wav" && extension != ".xwm" && extension != ".mp3") {
            spdlog::error("audio probe: expected wav/xwm/mp3: {}", path);
            return;
        }
        const auto resource = std::string("Sound/") + path;
        std::error_code ec;
        if (!std::filesystem::is_regular_file(std::filesystem::path("Data") / resource, ec)) {
            spdlog::error("audio probe: loose test file not found: Data/{}", resource);
            return;
        }
        if (!Ready(allowOverlap)) { return; }
        auto* manager = RE::BSAudioManager::GetSingleton();
        if (!manager) { spdlog::error("audio probe: audio manager unavailable"); return; }
        RE::BSResource::ID id{};
        id.GenerateFromPath(resource.c_str());
        RE::BSSoundHandle handle;
        manager->GetSoundHandleByFile(handle, id, 0x1A, 128);
        // Diagnostic candidate flags/priority. Actual audibility and path convention require an in-game test.
        Track(handle, resource);
    }

    void PlayDescriptor(std::string formKey, bool allowOverlap)
    {
        const auto key = common::ParseFormKey(formKey);
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* form = key && data ? LookupForm(*data, *key) : nullptr;
        // Lookup is const to discourage form edits; the engine playback API accepts a non-const descriptor.
        auto* descriptor = form ? const_cast<RE::BGSSoundDescriptorForm*>(form->As<RE::BGSSoundDescriptorForm>()) : nullptr;
        if (!descriptor) { spdlog::error("audio probe: missing or non-SNDR FormKey: {}", formKey); return; }
        if (!Ready(allowOverlap)) { return; }
        auto* manager = RE::BSAudioManager::GetSingleton();
        if (!manager) { spdlog::error("audio probe: audio manager unavailable"); return; }
        RE::BSSoundHandle handle;
        if (!manager->GetSoundHandle(handle, descriptor)) {
            spdlog::error("audio probe: descriptor resolution failed: {}", formKey);
            return;
        }
        Track(handle, std::move(formKey));
    }

    void PlayFileCategory(std::string path, int category, bool allowOverlap)
    {
        const bool isForm = path.find('|') != std::string::npos;
        std::replace(path.begin(), path.end(), '\\', '/');
        const std::filesystem::path relative(path);
        if (!isForm && (path.empty() || relative.is_absolute() || path.find(':') != std::string::npos ||
            std::ranges::any_of(relative, [](const auto& part) { return part == ".."; }))) {
            spdlog::error("audio probe: invalid relative sound path: {}", path); return;
        }
        const auto resource = std::string("Sound/") + path;
        std::error_code ec;
        if (!isForm && !std::filesystem::is_regular_file(std::filesystem::path("Data") / resource, ec)) {
            spdlog::error("audio probe: loose test file not found: Data/{}", resource); return;
        }
        if (category < 0 || category > 2 || !Ready(allowOverlap)) { return; }
        auto* data = RE::TESDataHandler::GetSingleton();
        auto* manager = RE::BSAudioManager::GetSingleton();
        auto* base = data ? data->LookupForm<RE::BGSSoundDescriptorForm>(0x0C8C74, "Skyrim.esm") : nullptr;
        if (isForm) {
            const auto key = common::ParseFormKey(path);
            const auto* form = key && data ? LookupForm(*data, *key) : nullptr;
            base = form ? const_cast<RE::BGSSoundDescriptorForm*>(form->As<RE::BGSSoundDescriptorForm>()) : nullptr;
        } else if (relative.extension() != ".wav" && relative.extension() != ".xwm") {
            spdlog::error("audio: unsupported sound file: {}", path);
            return;
        }
        const auto id = category == 1 ? 0x0876BBu : 0x064451u;
        auto* categoryForm = category != 0 && data ? data->LookupForm<RE::BGSSoundCategory>(id, "Skyrim.esm") : nullptr;
        if (!manager || !base || (category != 0 && !categoryForm)) {
            spdlog::error("audio probe: descriptor/category unavailable"); return;
        }
        auto* ui = RE::UI::GetSingleton();
        spdlog::info("audio probe: request context category={} master={} paused={}",
            category, manager->masterVolume, ui && ui->GameIsPaused());
        auto* currentCategory = categoryForm;
        for (int depth = 0; currentCategory && depth < 8; ++depth) {
            spdlog::info("audio probe: category depth={} form={:08X} volume={} multiplier={} static={} attenuation={} flags={:08X}",
                depth, currentCategory->GetFormID(), currentCategory->GetCategoryVolume(),
                currentCategory->volumeMult, currentCategory->GetStaticVolumeMultiplier(),
                currentCategory->GetCategoryAttenuation(), currentCategory->flags.underlying());
            currentCategory = currentCategory->parentCategory;
        }
        const auto cacheKey = std::to_string(category) + ":" + resource;
        auto found = g_descriptors.find(cacheKey);
        if (found == g_descriptors.end()) {
            auto adapter = std::make_unique<FileDescriptor>();
            adapter->original = base;
            adapter->resource.GenerateFromPath(resource.c_str());
            adapter->category = categoryForm;
            adapter->replaceResource = !isForm;
            found = g_descriptors.emplace(cacheKey, std::move(adapter)).first;
        }
        RE::BSSoundHandle handle;
        const bool obtained = manager->GetSoundHandle(handle, found->second.get());
        spdlog::info("audio probe: category={} descriptor=Skyrim.esm|0x0C8C74 obtained={}", category, obtained);
        if (obtained) { Track(handle, resource, categoryForm); }
    }
}
