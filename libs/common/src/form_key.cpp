#include "k7/common/form_key.h"

#include <cctype>
#include <charconv>
#include <format>

namespace k7::common
{
    namespace
    {
        constexpr std::uint32_t kMaxLocalId = 0x00FFFFFF;

        char Lower(char c) noexcept
        {
            return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }

    std::optional<FormKey> ParseFormKey(std::string_view text) noexcept
    {
        const auto bar = text.rfind('|');
        if (bar == std::string_view::npos || bar == 0) {
            return std::nullopt;
        }

        auto digits = text.substr(bar + 1);
        if (digits.size() > 2 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X')) {
            digits.remove_prefix(2);
        }
        if (digits.empty()) {
            return std::nullopt;
        }

        std::uint32_t value{};
        const auto    result = std::from_chars(digits.data(), digits.data() + digits.size(), value, 16);
        if (result.ec != std::errc{} || result.ptr != digits.data() + digits.size() || value > kMaxLocalId) {
            return std::nullopt;
        }

        FormKey key;
        key.plugin = std::string(text.substr(0, bar));
        key.id     = value;
        return key;
    }

    std::string ToString(const FormKey& key)
    {
        return std::format("{}|0x{:06X}", key.plugin, key.id);
    }

    bool Equivalent(const FormKey& a, const FormKey& b) noexcept
    {
        if (a.id != b.id || a.plugin.size() != b.plugin.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.plugin.size(); ++i) {
            if (Lower(a.plugin[i]) != Lower(b.plugin[i])) {
                return false;
            }
        }
        return true;
    }
}
