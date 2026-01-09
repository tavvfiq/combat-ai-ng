#pragma once

#include "ActorStateData.h"
#include "RE/A/Actor.h"

// Forward declaration
namespace CombatAI { class PrecisionIntegration; }

namespace CombatAI
{
    // Gathers state data for actors and their targets
    class ActorStateObserver
    {
    public:
        ActorStateObserver() = default;
        ~ActorStateObserver() = default;

        // Gather complete state data for an actor
        ActorStateData GatherState(RE::Actor* a_actor, float a_deltaTime);

    private:
        // Gather self state
        SelfState GatherSelfState(RE::Actor* a_actor);

        // Gather target state
        TargetState GatherTargetState(RE::Actor* a_actor, RE::Actor* a_target);

        // Helper: Get actor value percentage
        float GetActorValuePercent(RE::Actor* a_actor, RE::ActorValue a_value);

        // Helper: Check if target is power attacking
        bool IsPowerAttacking(RE::Actor* a_target);

        // Helper: Get weapon reach (approximation)
        float GetWeaponReach(RE::Actor* a_actor);
    };
}
