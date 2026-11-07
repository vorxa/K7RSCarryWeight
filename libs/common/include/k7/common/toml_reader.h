#pragma once

#include <format>
#include <limits>
#include <set>
#include <string>
#include <string_view>

#include <toml++/toml.hpp>

#include "k7/common/diagnostics.h"

namespace k7::common
{
    // 1-based line of a node in its file.
    int LineOf(const toml::node& node) noexcept;

    // Reads typed values out of one TOML table and remembers which keys were asked for, so that the rest
    // can be reported as unknown. A wrong type or an out-of-range value is reported as an error with the
    // file and line, and the value is left unchanged; nothing throws.
    class TomlReader
    {
    public:
        // `file` is the name shown in diagnostics; `path` is the dotted table name prefixed to keys
        // ("slots.general"), empty for the root table.
        TomlReader(const toml::table& table, std::string_view file, std::string path, Diagnostics& diagnostics) :
            table_(table), file_(file), path_(std::move(path)), diagnostics_(diagnostics)
        {}

        const toml::table& Table() const noexcept { return table_; }

        void Report(Severity severity, int line, std::string message) const;
        void Error(const toml::node& node, std::string_view key, std::string_view what) const;

        // Returns the node of a key, or null if absent.
        const toml::node* Find(std::string_view key);

        bool Bool(std::string_view key, bool& out);
        // Integers are accepted for float fields: `size = 5` is the same as `size = 5.0`.
        bool Float(std::string_view key, float& out, float min = std::numeric_limits<float>::lowest(),
            float max = std::numeric_limits<float>::max());
        bool Int(std::string_view key, int& out, int min = std::numeric_limits<int>::min());
        bool String(std::string_view key, std::string& out);

        // Enum given as a string; `parse` returns an empty optional for an unknown spelling.
        template <class E, class Parse>
        bool Enum(std::string_view key, E& out, Parse parse, std::string_view allowed)
        {
            const auto* node = Find(key);
            if (!node) {
                return false;
            }
            const auto* value = node->as_string();
            if (!value) {
                Error(*node, key, std::format("expected a string, one of: {}", allowed));
                return false;
            }
            const auto parsed = parse(value->get());
            if (!parsed) {
                Error(*node, key, std::format("unknown value '{}', expected one of: {}", value->get(), allowed));
                return false;
            }
            out = *parsed;
            return true;
        }

        const toml::table* SubTable(std::string_view key);
        const toml::array* Array(std::string_view key);

        // Marks keys as handled elsewhere (nested tables processed by the caller).
        void Known(std::string_view key) { known_.emplace(key); }

        void ReportUnknown() const;

    private:
        static std::string RangeText(float min, float max);
        std::string        Name(std::string_view key) const;

        const toml::table&    table_;
        std::string_view      file_;
        std::string           path_;
        Diagnostics&          diagnostics_;
        std::set<std::string> known_;
    };
}
