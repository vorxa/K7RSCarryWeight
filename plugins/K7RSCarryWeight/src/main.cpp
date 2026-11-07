#include "pch.h"

#include "carry_service.h"
#include "config_service.h"
#include "k7/skse/logging.h"
#include "k7/skse/menu.h"
#include "race_lookup.h"
#include "reload.h"
#include "survival.h"
#include "ui/lang_service.h"
#include "ui/menu.h"
#include "ui/state.h"

namespace
{
    constexpr auto kPluginName = "K7RSCarryWeight";


    void OnSkseMessage(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
        case SKSE::MessagingInterface::kDataLoaded:
            spdlog::info("data loaded");
            // Specification, section 13.1: the menu is optional, its absence is only an info line.
            k7::skse::menu::Check(false);
            k7::rscw::lang::Load();
            k7::rscw::ui::Init();
            k7::rscw::ui::RegisterMenu();
            k7::rscw::races::Resolve(k7::rscw::config::Get(), k7::rscw::config::GetSources(), "data loaded");
            k7::rscw::survival::Resolve(k7::rscw::config::Get(), k7::rscw::config::GetSources(), "data loaded");
            k7::rscw::carry::RegisterEvents();
            k7::rscw::survival::StartPolling();
            break;
        case SKSE::MessagingInterface::kPostLoadGame:
            // The co-save is read by now (Revert, then Load, then this message: specification, section 7).
            k7::rscw::Reload("save loaded");
            k7::rscw::carry::Recount("save loaded");
            k7::rscw::survival::OnGameLoaded();
            break;
        case SKSE::MessagingInterface::kNewGame:
            k7::rscw::Reload("new game");
            k7::rscw::carry::OnNewGame();
            k7::rscw::survival::OnGameLoaded();
            break;
        default:
            break;
        }
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    k7::skse::logging::Init(kPluginName);

    const auto runtime = a_skse->RuntimeVersion().string("."sv);
    spdlog::info("{} {} ({}) loaded, Skyrim runtime {}", kPluginName, K7_PLUGIN_VERSION, K7_BUILD_ID, runtime);

    // Specification, section 10: the DLL loads in VR but does nothing there.
    if (REL::Module::IsVR()) {
        spdlog::warn("Skyrim VR is not supported, the plugin does nothing");
        return true;
    }

    k7::rscw::config::Load("startup");
    k7::rscw::carry::RegisterSerialization();
    SKSE::GetMessagingInterface()->RegisterListener(OnSkseMessage);
    return true;
}
