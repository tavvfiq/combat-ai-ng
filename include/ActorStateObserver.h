#pragma once

#include "ActorStateData.h"
#include "RE/A/Actor.h"
#include <unordered_map>

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

        // Cleanup cached data for an actor
        void Cleanup(RE::Actor* a_actor);

    private:
        // Gather self state
        SelfState GatherSelfState(RE::Actor* a_actor);

        // Gather target state
        TargetState GatherTargetState(RE::Actor* a_actor, RE::Actor* a_target);

        // Gather combat context (multiple enemies, etc.)
        CombatContext GatherCombatContext(RE::Actor* a_actor, float a_currentTime);

        // Scan for nearby actors (enemies and allies) - expensive operation, done in one pass
        void ScanForNearbyActors(RE::Actor* a_actor, CombatContext& a_context);

        // Helper: Get actor value percentage
        float GetActorValuePercent(RE::Actor* a_actor, RE::ActorValue a_value);

        // Helper: Check if target is power attacking
        bool IsPowerAttacking(RE::Actor* a_target);

        // Helper: Get weapon reach (approximation)
        float GetWeaponReach(RE::Actor* a_actor);

        // Caching for combat context (expensive to calculate)
        struct CachedCombatContext
        {
            CombatContext context;
            float lastUpdateTime = 0.0f;
        };
        std::unordered_map<RE::Actor*, CachedCombatContext> m_combatContextCache;
        static constexpr float COMBAT_CONTEXT_UPDATE_INTERVAL = 1.0f; // Update every 1 second
    };
}
