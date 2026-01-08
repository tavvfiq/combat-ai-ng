#pragma once

#include "ActorStateData.h"
#include "DecisionResult.h"

namespace CombatAI
{
    // Evaluates state data and makes tactical decisions
    class DecisionMatrix
    {
    public:
        DecisionMatrix() = default;
        ~DecisionMatrix() = default;

        // Evaluate state and return decision
        DecisionResult Evaluate(const ActorStateData& a_state);

    private:
        // Priority 1: Interrupt (Counter-Play)
        DecisionResult EvaluateInterrupt(const ActorStateData& a_state);

        // Priority 2: Evasion (Tactical)
        DecisionResult EvaluateEvasion(const ActorStateData& a_state);

        // Priority 3: Survival (Fallback)
        DecisionResult EvaluateSurvival(const ActorStateData& a_state);

        // Helper: Calculate strafe direction (perpendicular to target)
        RE::NiPoint3 CalculateStrafeDirection(const ActorStateData& a_state);
    };
}
