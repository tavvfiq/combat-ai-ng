#include "pch.h"
#include "CombatDirector.h"
#include "Config.h"
#include "PrecisionIntegration.h"

namespace CombatAI
{
    void CombatDirector::Initialize()
    {
        LOG_INFO("CombatDirector initialized");
        
        // Initialize Precision integration (if enabled in config)
        auto& config = Config::GetInstance();
        if (config.GetModIntegrations().enablePrecisionIntegration) {
            PrecisionIntegration::GetInstance().Initialize();
        }

        if (config.GetModIntegrations().enableBFCOIntegration) {
            m_executor.EnableBFCO(true);
        }
        
        // Set processing interval
        m_processInterval = config.GetGeneral().processingInterval;
        
        // Initialize humanizer with config values
        Humanizer::Config humanizerConfig;
        humanizerConfig.baseReactionDelayMs = config.GetHumanizer().baseReactionDelayMs;
        humanizerConfig.reactionVarianceMs = config.GetHumanizer().reactionVarianceMs;
        humanizerConfig.level1MistakeChance = config.GetHumanizer().level1MistakeChance;
        humanizerConfig.level50MistakeChance = config.GetHumanizer().level50MistakeChance;
        humanizerConfig.bashCooldownSeconds = config.GetHumanizer().bashCooldownSeconds;
        m_humanizer.SetConfig(humanizerConfig);
    }

    void CombatDirector::ProcessActor(RE::Actor* a_actor, float a_deltaTime)
    {
        if (!ShouldProcessActor(a_actor, a_deltaTime)) {
            return;
        }

        // Update humanizer
        m_humanizer.Update(a_deltaTime);

        // Check if actor can react (reaction delay)
        if (!m_humanizer.CanReact(a_actor, a_deltaTime)) {
            return;
        }

        // Gather state
        ActorStateData state = m_observer.GatherState(a_actor, a_deltaTime);

        // Make decision
        DecisionResult decision = m_decisionMatrix.Evaluate(a_actor, state);

        // Check if should make mistake (humanizer)
        if (decision.action != ActionType::None && m_humanizer.ShouldMakeMistake(a_actor)) {
            // Make a mistake - don't execute the action
            return;
        }

        // Check cooldowns
        if (decision.action == ActionType::Bash) {
            if (m_humanizer.IsOnCooldown(a_actor, "bash")) {
                return; // On cooldown
            }
        } else if (decision.action == ActionType::Dodge) {
            if (m_humanizer.IsOnCooldown(a_actor, "dodge")) {
                return; // Dodge on cooldown
            }
        } else if (decision.action == ActionType::Jump) {
            if (m_humanizer.IsOnCooldown(a_actor, "jump")) {
                return; // Jump on cooldown
            }
        }

        auto& config = Config::GetInstance();
        // Execute decision
        if (decision.action != ActionType::None) {
            bool success = m_executor.Execute(a_actor, decision, state);

            if (success) {
                // Mark action as used (start cooldown)
                if (decision.action == ActionType::Bash) {
                    m_humanizer.MarkActionUsed(a_actor, "bash");
                } else if (decision.action == ActionType::Strafe) {
                    // Dodge cooldown is handled by TK Dodge system
                    m_humanizer.MarkActionUsed(a_actor, "dodge");
                } else if (decision.action == ActionType::Jump) {
                    m_humanizer.MarkActionUsed(a_actor, "jump");
                }

                // Track actor
                m_processedActors.insert(a_actor);
            }
        }
    }

    void CombatDirector::Update(float a_deltaTime)
    {
        // Periodic cleanup (from config)
        static float cleanupTimer = 0.0f;
        auto& config = Config::GetInstance();
        float cleanupInterval = config.GetPerformance().cleanupInterval;
        cleanupTimer += a_deltaTime;
        if (cleanupTimer > cleanupInterval) {
            Cleanup();
            cleanupTimer = 0.0f;
        }
    }

    void CombatDirector::Cleanup()
    {
        // Remove invalid actors from tracking
        auto it = m_processedActors.begin();
        while (it != m_processedActors.end()) {
            RE::Actor* actor = *it;
            if (!actor || actor->IsDead() || !actor->IsInCombat()) {
                it = m_processedActors.erase(it);
            } else {
                ++it;
            }
        }

        // Also clean up orphaned timers (actors removed from processedActors but timer still exists)
        auto timerIt = m_actorProcessTimers.begin();
        while (timerIt != m_actorProcessTimers.end()) {
            RE::Actor* actor = timerIt->first;
            if (!actor || actor->IsDead() || !actor->IsInCombat()) {
                timerIt = m_actorProcessTimers.erase(timerIt);
            } else {
                ++timerIt;
            }
        }
    }

    bool CombatDirector::ShouldProcessActor(RE::Actor* a_actor, float a_deltaTime)
    {
        if (!a_actor) {
            return false;
        }

        // Skip player
        if (a_actor->IsPlayerRef()) {
            return false;
        }

        // Only process NPCs, not creatures
        // Check for ActorTypeNPC keyword (NPCs have this, creatures don't)
        if (!a_actor->HasKeywordString("ActorTypeNPC")) {
            return false;
        }

        // Double-check: explicitly exclude creatures
        if (a_actor->HasKeywordString("ActorTypeCreature")) {
            return false;
        }

        // Additional check: use CalculateCachedOwnerIsNPC as fallback
        // This should match the keyword check, but provides extra safety
        if (!a_actor->CalculateCachedOwnerIsNPC()) {
            return false;
        }

        // Check if we should only process combat actors
        auto& config = Config::GetInstance();
        if (config.GetPerformance().onlyProcessCombatActors) {
            // Only process actors in combat
            if (!a_actor->IsInCombat()) {
                return false;
            }
        }

        // Only process alive actors
        if (a_actor->IsDead()) {
            return false;
        }

        // Check if AI is enabled
        if (!a_actor->IsAIEnabled()) {
            return false;
        }

        // Throttle processing: only process at configured interval
        auto timerIt = m_actorProcessTimers.find(a_actor);
        if (timerIt == m_actorProcessTimers.end()) {
            // First time processing this actor, initialize timer
            m_actorProcessTimers[a_actor] = 0.0f;
            return true;
        }
        
        m_actorProcessTimers[a_actor] += a_deltaTime;
        
        // Only process if interval has passed
        if (m_actorProcessTimers[a_actor] < m_processInterval) {
            return false; // Skip processing this frame
        }
        
        // Reset timer for next interval
        m_actorProcessTimers[a_actor] = 0.0f;

        return true;
    }
}
