#pragma once

#include "RE/N/NiPoint3.h"

namespace CombatAI
{
    // Action types that can be executed
    enum class ActionType : std::uint8_t
    {
        None = 0,
        Bash, // 1
        Strafe, // 2
        Retreat, // 3
        PowerAttack, // 4
        Attack, // 5
        SprintAttack, // 6
        Jump, // 7
        Dodge // 8
    };

    // Decision result containing action to take
    struct DecisionResult
    {
        ActionType action = ActionType::None;
        int priority = 0; // Higher = more important
        RE::NiPoint3 direction = RE::NiPoint3(0.0f, 0.0f, 0.0f); // For movement actions
        float intensity = 1.0f; // 0.0 to 1.0, for movement speed/intensity
    };
}
