#pragma once

#include "ActionExecutor.h"
#include "ActorStateObserver.h"
#include "DecisionMatrix.h"
#include "DecisionResult.h"
#include "Humanizer.h"
#include "ParryFeedbackTracker.h"
#include "ThreadSafeMap.h"
#include "pch.h"
#include <mutex>
#include <random>
#include <unordered_map>

namespace CombatAI
{
    // Singleton manager for all combat AI logic
    class CombatDirector
    {
      public:
        static CombatDirector &GetInstance()
        {
            static CombatDirector instance;
            return instance;
        }

        CombatDirector(const CombatDirector &) = delete;
        CombatDirector &operator=(const CombatDirector &) = delete;

        // Initialize the director
        void Initialize();

        // Register mod callback listeners (for EldenParry integration)
        void RegisterModCallbacks();

        // Process an actor (called from hook)
        void ProcessActor(RE::Actor *a_actor, float a_deltaTime);

        // Update (called per frame, for cooldowns, etc.)
        void Update(float a_deltaTime);

        // Cleanup (remove invalid actors)
        void Cleanup();

        // Called when player hits an NPC — grants a short retaliation slot if
        // the NPC is currently locked out by the pacing system
        void OnNPCHit(RE::Actor *a_victim);

      private:
        CombatDirector() = default;
        ~CombatDirector() = default;

        // Check if actor should be processed
        bool ShouldProcessActor(RE::Actor *a_actor, float a_deltaTime);

        // Components
        ActorStateObserver m_observer;
        DecisionMatrix m_decisionMatrix;
        ActionExecutor m_executor;
        Humanizer m_humanizer;

        // Track processed actors (for cleanup) - use FormID as key for safety
        // Thread-safe to prevent crashes from concurrent access
        ThreadSafeSet<RE::FormID> m_processedActors;

        // Track per-actor processing timers (for throttling) - use FormID as key for safety
        // Thread-safe to prevent crashes from concurrent access
        ThreadSafeMap<RE::FormID, float> m_actorProcessTimers;

        // Track actor spawn times to delay processing newly spawned actors
        // Prevents processing actors before they're fully initialized
        // Thread-safe to prevent crashes from concurrent access
        ThreadSafeMap<RE::FormID, float> m_actorSpawnTimes;

        // Processing throttle (don't process every frame)
        float m_processTimer = 0.0f;
        float m_processInterval = 0.1f; // Process every 100ms (loaded from config)

        // Combat pacing: timed attack slot system.
        // Each NPC that claims a slot holds it for a random duration (SlotWindowMin–Max seconds).
        // Key = attacker FormID → (target FormID, time when slot expires).
        // Protected by m_slotMutex.
        struct SlotEntry
        {
            RE::FormID targetID = 0;
            float expiryTime = 0.0f; // compared against m_elapsedTime
        };
        std::unordered_map<RE::FormID, SlotEntry> m_slotHolders;
        mutable std::mutex m_slotMutex;
        float m_elapsedTime = 0.0f; // monotonically increasing game time

        // Random engine for slot window durations
        std::mt19937 m_rng{std::random_device{}()};
        std::uniform_real_distribution<float> m_slotDist{3.0f, 7.0f}; // updated from config

        // Minimum time before processing newly spawned actors (safety delay)
        static constexpr float SPAWN_WARMUP_DELAY = 0.5f; // 500ms delay
    };
} // namespace CombatAI
