#include "pch.h"

#include "reload.h"

#include "config_service.h"
#include "race_lookup.h"
#include "survival.h"

namespace k7::rscw
{
    void Reload(std::string_view a_reason)
    {
        config::Load(a_reason);
        races::Resolve(config::Get(), config::GetSources(), a_reason);
        survival::Resolve(config::Get(), config::GetSources(), a_reason);
    }
}
