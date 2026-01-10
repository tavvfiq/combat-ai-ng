#pragma once

#include "ActorStateObserver.h"
#include "DecisionMatrix.h"
#include "ActionExecutor.h"
#include "Humanizer.h"
#include "RE/A/Actor.h"
#include <unordered_set>

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
        std::unordered_set<RE::FormID> m_processedActors;

        // Track per-actor processing timers (for throttling) - use FormID as key for safety
        std::unordered_map<RE::FormID, float> m_actorProcessTimers;

        // Processing throttle (don't process every frame)
        float m_processTimer = 0.0f;
        float m_processInterval = 0.1f; // Process every 100ms (loaded from config)
    };
}
