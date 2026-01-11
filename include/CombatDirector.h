#pragma once

#include "ActorStateObserver.h"
#include "DecisionMatrix.h"
#include "ActionExecutor.h"
#include "Humanizer.h"
#include "ThreadSafeMap.h"
#include "RE/A/Actor.h"

namespace CombatAI
{
    // Singleton manager for all combat AI logic
    class CombatDirector
    {
    public:
        static CombatDirector& GetInstance()
        {
            static CombatDirector instance;
            return instance;
        }

        CombatDirector(const CombatDirector&) = delete;
        CombatDirector& operator=(const CombatDirector&) = delete;

        // Initialize the director
        void Initialize();

        // Process an actor (called from hook)
        void ProcessActor(RE::Actor* a_actor, float a_deltaTime);

        // Update (called per frame, for cooldowns, etc.)
        void Update(float a_deltaTime);

        // Cleanup (remove invalid actors)
        void Cleanup();

    private:
        CombatDirector() = default;
        ~CombatDirector() = default;

        // Check if actor should be processed
        bool ShouldProcessActor(RE::Actor* a_actor, float a_deltaTime);

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
        
        // Minimum time before processing newly spawned actors (safety delay)
        static constexpr float SPAWN_WARMUP_DELAY = 0.5f; // 500ms delay
    };
}
