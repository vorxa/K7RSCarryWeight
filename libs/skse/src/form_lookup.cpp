#include "pch.h"

#include "k7/skse/form_lookup.h"

#include <cctype>

namespace k7::skse
{
    std::string KeywordIndex::Key(std::string_view editorId)
    {
        std::string result(editorId);
        for (auto& c : result) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return result;
    }

    KeywordIndex KeywordIndex::Build(RE::TESDataHandler& handler)
    {
        KeywordIndex index;
        for (auto* keyword : handler.GetFormArray<RE::BGSKeyword>()) {
            if (!keyword) {
                continue;
            }
            const char* editorId = keyword->GetFormEditorID();
            if (!editorId || !*editorId) {
                continue;
            }
            index.entries_[Key(editorId)].push_back(keyword);
            ++index.keywordCount_;
        }
        return index;
    }

    const std::vector<RE::BGSKeyword*>* KeywordIndex::Find(std::string_view editorId) const
    {
        const auto it = entries_.find(Key(editorId));
        return it == entries_.end() ? nullptr : &it->second;
    }

    const RE::TESForm* LookupForm(RE::TESDataHandler& handler, const common::FormKey& key)
    {
        return handler.LookupForm(key.id, key.plugin);
    }

    std::optional<common::FormKey> FormKeyOf(const RE::TESForm& form)
    {
        const auto* file = form.GetFile(0);
        if (!file) {
            return std::nullopt;
        }
        return common::FormKey{ std::string(file->fileName), form.GetLocalFormID() };
    }

    std::string FormText(const RE::TESForm& form)
    {
        const auto* file = form.GetFile(0);
        return std::format("{}|0x{:06X}", file ? file->fileName : "?", form.GetLocalFormID());
    }
}
