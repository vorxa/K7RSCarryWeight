#pragma once

// Include after CommonLibSSE-NG (RE/Skyrim.h): the plugin's precompiled header provides it.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "k7/common/form_key.h"

namespace k7::skse
{
    // Every keyword of the loaded plugins by EditorID. EditorIDs are matched case-insensitively, as the
    // game does; one EditorID may be declared by several plugins, so each maps to all its keywords, in
    // load order.
    class KeywordIndex
    {
    public:
        using Entries = std::unordered_map<std::string, std::vector<RE::BGSKeyword*>>;

        // Keywords without an EditorID are skipped.
        static KeywordIndex Build(RE::TESDataHandler& handler);

        // Lower-case form of an EditorID, the key of Entries().
        static std::string Key(std::string_view editorId);

        // The keywords declared with `editorId`, or null.
        const std::vector<RE::BGSKeyword*>* Find(std::string_view editorId) const;

        // Lower-case EditorID -> keywords; an entry with two or more keywords is a collision.
        const Entries& All() const noexcept { return entries_; }

        std::size_t KeywordCount() const noexcept { return keywordCount_; }
        std::size_t EditorIdCount() const noexcept { return entries_.size(); }

    private:
        Entries     entries_;
        std::size_t keywordCount_{};
    };

    // The form a key names, through the plugin's load-order index; null if the plugin is not loaded or
    // has no such form.
    const RE::TESForm* LookupForm(RE::TESDataHandler& handler, const common::FormKey& key);

    // "Plugin.esp|0x000123": the plugin that defines the form and its id inside that plugin.
    std::string FormText(const RE::TESForm& form);

    // The same as a key; nothing for a form that no plugin defines (created at run time).
    std::optional<common::FormKey> FormKeyOf(const RE::TESForm& form);
}
