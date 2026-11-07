#include "k7/common/toml_reader.h"

#include <cmath>

namespace k7::common
{
    int LineOf(const toml::node& node) noexcept
    {
        return static_cast<int>(node.source().begin.line);
    }

    void TomlReader::Report(Severity severity, int line, std::string message) const
    {
        diagnostics_.push_back(Diagnostic{ severity, std::string(file_), line, std::move(message) });
    }

    void TomlReader::Error(const toml::node& node, std::string_view key, std::string_view what) const
    {
        Report(Severity::Error, LineOf(node), std::format("{}: {}", Name(key), what));
    }

    const toml::node* TomlReader::Find(std::string_view key)
    {
        known_.emplace(key);
        return table_.get(key);
    }

    bool TomlReader::Bool(std::string_view key, bool& out)
    {
        const auto* node = Find(key);
        if (!node) {
            return false;
        }
        if (const auto* value = node->as_boolean()) {
            out = value->get();
            return true;
        }
        Error(*node, key, "expected true or false");
        return false;
    }

    bool TomlReader::Float(std::string_view key, float& out, float min, float max)
    {
        const auto* node = Find(key);
        if (!node) {
            return false;
        }
        double number{};
        if (const auto* value = node->as_floating_point()) {
            number = value->get();
        } else if (const auto* integer = node->as_integer()) {
            number = static_cast<double>(integer->get());
        } else {
            Error(*node, key, "expected a number");
            return false;
        }
        if (!std::isfinite(number) || std::fabs(number) > std::numeric_limits<float>::max()) {
            Error(*node, key, "number out of range");
            return false;
        }
        if (number < min || number > max) {
            Error(*node, key, RangeText(min, max));
            return false;
        }
        out = static_cast<float>(number);
        return true;
    }

    bool TomlReader::Int(std::string_view key, int& out, int min)
    {
        const auto* node = Find(key);
        if (!node) {
            return false;
        }
        const auto* value = node->as_integer();
        if (!value) {
            Error(*node, key, "expected a whole number");
            return false;
        }
        const auto number = value->get();
        if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max()) {
            Error(*node, key, "number out of range");
            return false;
        }
        if (number < min) {
            Error(*node, key, RangeText(static_cast<float>(min), std::numeric_limits<float>::max()));
            return false;
        }
        out = static_cast<int>(number);
        return true;
    }

    bool TomlReader::String(std::string_view key, std::string& out)
    {
        const auto* node = Find(key);
        if (!node) {
            return false;
        }
        if (const auto* value = node->as_string()) {
            out = value->get();
            return true;
        }
        Error(*node, key, "expected a string");
        return false;
    }

    const toml::table* TomlReader::SubTable(std::string_view key)
    {
        const auto* node = Find(key);
        if (!node) {
            return nullptr;
        }
        if (const auto* sub = node->as_table()) {
            return sub;
        }
        Error(*node, key, "expected a table");
        return nullptr;
    }

    const toml::array* TomlReader::Array(std::string_view key)
    {
        const auto* node = Find(key);
        if (!node) {
            return nullptr;
        }
        if (const auto* array = node->as_array()) {
            return array;
        }
        Error(*node, key, "expected an array");
        return nullptr;
    }

    void TomlReader::ReportUnknown() const
    {
        for (const auto& [key, node] : table_) {
            if (!known_.contains(std::string(key.str()))) {
                Report(Severity::Warning, LineOf(node), std::format("unknown key '{}'", Name(key.str())));
            }
        }
    }

    std::string TomlReader::RangeText(float min, float max)
    {
        if (max >= std::numeric_limits<float>::max()) {
            return std::format("must not be less than {}", min);
        }
        return std::format("must be between {} and {}", min, max);
    }

    std::string TomlReader::Name(std::string_view key) const
    {
        return path_.empty() ? std::string(key) : std::format("{}.{}", path_, key);
    }
}
