#pragma once

#include <cctype>
#include <string_view>

namespace k7::common
{
    // ASCII case-insensitive comparison, the way the game compares EditorIDs and plugin names.
    inline bool EqualsIgnoreCase(std::string_view a, std::string_view b) noexcept
    {
        if (a.size() != b.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) {
                return false;
            }
        }
        return true;
    }

    inline bool StartsWithIgnoreCase(std::string_view text, std::string_view prefix) noexcept
    {
        return text.size() >= prefix.size() && EqualsIgnoreCase(text.substr(0, prefix.size()), prefix);
    }
}
