#include "pch.h"

#include "k7/skse/language.h"

#include "k7/common/lang.h"

namespace k7::skse
{
    std::string GameLanguage()
    {
        auto*       settings = RE::INISettingCollection::GetSingleton();
        const auto* setting = settings ? settings->GetSetting("sLanguage:General") : nullptr;
        const char* value = setting ? setting->GetString() : nullptr;
        return common::lang::NormalizeLanguage(value ? value : "");
    }
}
