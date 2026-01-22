#pragma once

#include "ActorStateData.h"
#include "DecisionResult.h"
#include "ThreadSafeMap.h"
#include "pch.h"

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
            
            // Parry feedback data
            bool lastParrySuccess = false; // Whether last parry attempt was successful
            float lastParryEstimatedDuration = 0.0f; // Estimated duration used for last parry attempt
            float timeSinceLastParryAttempt = 999.0f; // Time since last parry attempt
            int parrySuccessCount = 0; // Number of successful parries (for learning)
            int parryAttemptCount = 0; // Total number of parry attempts (for learning)
            
            // Timed block feedback data
            bool lastTimedBlockSuccess = false; // Whether last timed block attempt was successful
            float lastTimedBlockEstimatedDuration = 0.0f; // Estimated duration used for last timed block attempt
            float timeSinceLastTimedBlockAttempt = 999.0f; // Time since last timed block attempt
            int timedBlockSuccessCount = 0; // Number of successful timed blocks (for learning)
            int timedBlockAttemptCount = 0; // Total number of timed block attempts (for learning)
            
            // Guard Counter feedback data
            bool lastGuardCounterSuccess = false; // Whether last guard counter attempt was successful
            float timeSinceLastGuardCounterAttempt = 999.0f; // Time since last guard counter attempt
            int guardCounterSuccessCount = 0; // Number of successful guard counters (hit)
            int guardCounterAttemptCount = 0; // Total number of guard counter attempts
            int guardCounterFailedCount = 0; // Number of failed guard counters (missed/blocked)
            int guardCounterMissedOpportunityCount = 0; // Number of times guard counter window expired without attempt
            float guardCounterSuccessRate = 0.0f; // Success rate (0.0-1.0)
            
            // Attack defense feedback data (when NPC's attacks are parried/timed blocked)
            bool lastAttackParried = false; // Whether last attack was parried
            bool lastAttackTimedBlocked = false; // Whether last attack was timed blocked
			bool lastAttackHit = false;
			bool lastAttackMissed = false;
            float timeSinceLastParriedAttack = 999.0f; // Time since last parried attack
            float timeSinceLastTimedBlockedAttack = 999.0f; // Time since last timed blocked attack
			float timeSinceLastHitAttack = 999.0f;
			float timeSinceLastMissedAttack = 999.0f;
            int parriedAttackCount = 0; // Number of attacks that were parried
            int timedBlockedAttackCount = 0; // Number of attacks that were timed blocked
			int hitAttackCount = 0;
			int missedAttackCount = 0;
            int totalAttackCount = 0; // Total number of attacks attempted
            float parryRate = 0.0f; // Percentage of attacks parried (0.0-1.0)
            float timedBlockRate = 0.0f; // Percentage of attacks timed blocked (0.0-1.0)
			float hitRate = 0.0f;
			float missRate = 0.0f;
            float totalDefenseRate = 0.0f; // Combined parry + timed block rate
            
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
            
            // Parry timing data
            float attackStartTime = -1.0f; // When current attack started (relative time, -1 if not attacking)
            float estimatedAttackDuration = 0.0f; // Estimated total duration of current attack
            float timeUntilAttackHits = 999.0f; // Estimated time until attack hits (999 if not attacking or already hit)
            bool isPowerAttack = false; // Track if current attack is a power attack
            
            // Previous states for duration tracking
            bool wasBlocking = false;
            bool wasAttacking = false;
            bool wasCasting = false;
            bool wasDrawing = false;
            bool wasIdle = false;
            RE::ATTACK_STATE_ENUM previousAttackState = RE::ATTACK_STATE_ENUM::kNone;
        };
        
        // Helper: Estimate attack duration based on weapon and attack type
        float EstimateAttackDuration(RE::Actor* a_target, bool a_isPowerAttack);

        // Thread-safe temporal state storage (keyed by FormID)
        ThreadSafeMap<RE::FormID, ActorTemporalData> m_actorTemporalData;
        ThreadSafeMap<RE::FormID, TargetTemporalData> m_targetTemporalData; // Keyed by target FormID, but we'll use actor-target pair
    };
}
