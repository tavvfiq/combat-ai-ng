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
        
        // Load config values
        auto& config = Config::GetInstance();
        
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
        if (!ShouldProcessActor(a_actor)) {
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
        DecisionResult decision = m_decisionMatrix.Evaluate(state);

        // Enhance decision based on combat style (instead of suppressing vanilla AI)
        decision = m_styleEnhancer.EnhanceDecision(a_actor, decision, state);

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
        } else if (decision.action == ActionType::Strafe) {
            if (m_humanizer.IsOnCooldown(a_actor, "dodge")) {
                return; // Dodge on cooldown
            }
        }

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
                }

                // Track actor
                m_processedActors.insert(a_actor);
            }
        }
    }

    void CombatDirector::Update(float a_deltaTime)
    {
        // Update humanizer
        m_humanizer.Update(a_deltaTime);

        // Update processing timer
        m_processTimer += a_deltaTime;
        if (m_processTimer > m_processInterval) {
            m_processTimer = 0.0f;
        }

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
    }

    bool CombatDirector::ShouldProcessActor(RE::Actor* a_actor)
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

        return true;
    }
}
