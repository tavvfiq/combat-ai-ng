#pragma once

#include "RE/N/NiPoint3.h"

namespace CombatAI
{
    // Action types that can be executed
    enum class ActionType : std::uint8_t
    {
        None = 0,
        Retreat, // 1
        Strafe, // 2
        Bash, // 3
        PowerAttack, // 4
        SprintAttack, // 5
        Attack, // 6
        Jump, // 7
        Dodge, // 8
        Backoff, //9
        Advancing //10
    };

    // Decision result containing action to take
    struct DecisionResult
    {
        ActionType action = ActionType::None;
        float priority = 0.0f; // Higher = more important (float for granular control)
        RE::NiPoint3 direction = RE::NiPoint3(0.0f, 0.0f, 0.0f); // For movement actions
        float intensity = 1.0f; // 0.0 to 1.0, for movement speed/intensity
    };
}
