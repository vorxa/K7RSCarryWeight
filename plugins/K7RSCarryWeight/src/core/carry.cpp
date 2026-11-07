#include "core/carry.h"

#include <algorithm>

namespace k7::rscw::core
{
    int ClampValue(int value, int minTotal) noexcept
    {
        return std::max(value, minTotal);
    }

    float Contribution(std::optional<int> value, int minTotal, float raceBase) noexcept
    {
        if (!value) {
            return 0.0f;
        }
        return static_cast<float>(ClampValue(*value, minTotal)) - raceBase;
    }

    float Delta(float applied, float wanted) noexcept
    {
        return wanted - applied;
    }
}
