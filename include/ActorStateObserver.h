#pragma once

#include "ActorStateData.h"
#include "DecisionResult.h"
#include "ThreadSafeMap.h"
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

        // Cleanup cached data for an actor
        void Cleanup(RE::Actor* a_actor);

        // Notify that an action was executed (for temporal tracking)
        void NotifyActionExecuted(RE::Actor* a_actor, ActionType a_action);

    private:
        // Gather self state
        SelfState GatherSelfState(RE::Actor* a_actor);

        // Gather target state
        TargetState GatherTargetState(RE::Actor* a_actor, RE::Actor* a_target);

        // Gather combat context (multiple enemies, etc.)
        CombatContext GatherCombatContext(RE::Actor* a_actor, float a_currentTime);

        // Scan for nearby actors (enemies and allies) - expensive operation, done in one pass
        void ScanForNearbyActors(RE::Actor* a_actor, CombatContext& a_context, RE::Actor* a_primaryTarget);

        // Helper: Get actor value percentage
        float GetActorValuePercent(RE::Actor* a_actor, RE::ActorValue a_value);

        // Helper: Check if target is power attacking
        bool IsPowerAttacking(RE::Actor* a_target);

        // Helper: Get weapon reach (approximation)
        float GetWeaponReach(RE::Actor* a_actor);

        // Gather temporal state (time-based tracking)
        TemporalState GatherTemporalState(RE::Actor* a_actor, RE::Actor* a_target, float a_deltaTime);

        // Caching for combat context (expensive to calculate)
        struct CachedCombatContext
        {
            CombatContext context;
            float lastUpdateTime = 0.0f;
        };
        // Thread-safe to prevent crashes from concurrent access
        ThreadSafeMap<RE::FormID, CachedCombatContext> m_combatContextCache;
        static constexpr float COMBAT_CONTEXT_UPDATE_INTERVAL = 5.0f; // Update every 1 second

        // Temporal state tracking per actor
        struct ActorTemporalData
        {
            // Self temporal tracking
            float timeSinceLastAttack = 999.0f;
            float timeSinceLastDodge = 999.0f;
            float timeSinceLastAction = 999.0f;
            float blockingDuration = 0.0f;
            float attackingDuration = 0.0f;
            float idleDuration = 0.0f;
            float timeSinceLastPowerAttack = 999.0f;
            float timeSinceLastSprintAttack = 999.0f;
            float timeSinceLastBash = 999.0f;
            float timeSinceLastFeint = 999.0f;
            
            // Previous states for duration tracking
            bool wasBlocking = false;
            bool wasAttacking = false;
            bool wasIdle = false;
            RE::ATTACK_STATE_ENUM previousAttackState = RE::ATTACK_STATE_ENUM::kNone;
        };

        // Target temporal tracking per actor-target pair
        struct TargetTemporalData
        {
            float timeSinceLastAttack = 999.0f;
            float blockingDuration = 0.0f;
            float castingDuration = 0.0f;
            float drawingDuration = 0.0f;
            float attackingDuration = 0.0f;
            float idleDuration = 0.0f;
            float timeSinceLastPowerAttack = 999.0f;
            
            // Previous states for duration tracking
            bool wasBlocking = false;
            bool wasAttacking = false;
            bool wasCasting = false;
            bool wasDrawing = false;
            bool wasIdle = false;
            RE::ATTACK_STATE_ENUM previousAttackState = RE::ATTACK_STATE_ENUM::kNone;
        };

        // Thread-safe temporal state storage (keyed by FormID)
        ThreadSafeMap<RE::FormID, ActorTemporalData> m_actorTemporalData;
        ThreadSafeMap<RE::FormID, TargetTemporalData> m_targetTemporalData; // Keyed by target FormID, but we'll use actor-target pair
    };
}
