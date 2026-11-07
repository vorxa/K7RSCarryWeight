#include "core/recount.h"

#include "core/carry.h"

namespace k7::rscw::core
{
    RecountResult Recount(const RecountInput& input) noexcept
    {
        RecountResult result;
        result.contribution = input.applied;

        if (input.beast) {
            result.kind = RecountKind::SkipBeast;
            return result;
        }
        if (input.enabled && input.race) {
            if (input.sex == Sex::Unknown) {
                result.kind = RecountKind::SkipUnknown;
                return result;
            }
            const int configured = input.sex == Sex::Male ? input.race->male : input.race->female;
            if (input.gamePenalty > 0.0f) {
                result.value = ClampValue(configured - input.survivalPenalty, input.minTotal);
                result.contribution = Contribution(result.value, input.minTotal, input.raceBase) + input.gamePenalty;
            } else {
                result.value = ClampValue(configured, input.minTotal);
                result.contribution = Contribution(configured, input.minTotal, input.raceBase);
            }
        } else {
            result.contribution = 0.0f;
        }
        result.delta = Delta(input.applied, result.contribution);
        return result;
    }
}
