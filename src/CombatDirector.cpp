#include "pch.h"
#include "CombatDirector.h"
#include "Config.h"
#include "PrecisionIntegration.h"
#include "ActorUtils.h"
#include "Logger.h"
#include <sstream>

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
            // Check if BFCO plugin is loaded
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (dataHandler) {
                auto bfcoPlugin = dataHandler->LookupModByName("SCSI-ACTbfco-Main.esp");
                if (bfcoPlugin) {
                    m_executor.EnableBFCO(true);
                }
            } else {
                m_executor.EnableBFCO(false);
            }
        }
        
        // Set processing interval
        m_processInterval = config.GetGeneral().processingInterval;
        
        // Initialize humanizer with config values
        Humanizer::Config humanizerConfig;
        humanizerConfig.baseReactionDelayMs = config.GetHumanizer().baseReactionDelayMs;
        humanizerConfig.reactionVarianceMs = config.GetHumanizer().reactionVarianceMs;
        humanizerConfig.level1ReactionDelayMs = config.GetHumanizer().level1ReactionDelayMs;
        humanizerConfig.level50ReactionDelayMs = config.GetHumanizer().level50ReactionDelayMs;
        humanizerConfig.level1MistakeChance = config.GetHumanizer().level1MistakeChance;
        humanizerConfig.level50MistakeChance = config.GetHumanizer().level50MistakeChance;
        humanizerConfig.bashCooldownSeconds = config.GetHumanizer().bashCooldownSeconds;
        humanizerConfig.dodgeCooldownSeconds = config.GetHumanizer().dodgeCooldownSeconds;
        humanizerConfig.jumpCooldownSeconds = config.GetHumanizer().jumpCooldownSeconds;
        humanizerConfig.bashMistakeMultiplier = config.GetHumanizer().bashMistakeMultiplier;
        humanizerConfig.dodgeMistakeMultiplier = config.GetHumanizer().dodgeMistakeMultiplier;
        humanizerConfig.jumpMistakeMultiplier = config.GetHumanizer().jumpMistakeMultiplier;
        humanizerConfig.strafeMistakeMultiplier = config.GetHumanizer().strafeMistakeMultiplier;
        humanizerConfig.powerAttackMistakeMultiplier = config.GetHumanizer().powerAttackMistakeMultiplier;
        humanizerConfig.attackMistakeMultiplier = config.GetHumanizer().attackMistakeMultiplier;
        humanizerConfig.sprintAttackMistakeMultiplier = config.GetHumanizer().sprintAttackMistakeMultiplier;
        humanizerConfig.retreatMistakeMultiplier = config.GetHumanizer().retreatMistakeMultiplier;
        humanizerConfig.backoffMistakeMultiplier = config.GetHumanizer().backoffMistakeMultiplier;
        humanizerConfig.advancingMistakeMultiplier = config.GetHumanizer().advancingMistakeMultiplier;
        humanizerConfig.flankingMistakeMultiplier = config.GetHumanizer().flankingMistakeMultiplier;
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
        // For movement actions, allow continuous execution even during reaction delay
        // (we check this after getting the decision to see if it's a movement action)
        bool canReact = m_humanizer.CanReact(a_actor, a_deltaTime);
        
        // Gather state first to check decision type
        ActorStateData state = m_observer.GatherState(a_actor, a_deltaTime);
        DecisionResult decision = m_decisionMatrix.Evaluate(a_actor, state);
        
        // Allow movement actions to execute continuously (bypass reaction delay)
        bool isMovementAction = (decision.action == ActionType::Strafe || 
                                decision.action == ActionType::Retreat || 
                                decision.action == ActionType::Advancing || 
                                decision.action == ActionType::Backoff ||
                                decision.action == ActionType::Flanking);
        
        if (!canReact && !isMovementAction) {
            return; // Not a movement action and can't react yet
        }

        // State and decision already gathered above for movement action check

        // Check if should make mistake (humanizer)
        if (decision.action != ActionType::None && m_humanizer.ShouldMakeMistake(a_actor, decision.action)) {
            // Make a mistake - don't execute the action
            return;
        }

        // Check cooldowns (only actions with cooldowns will return true)
        if (m_humanizer.IsOnCooldown(a_actor, decision.action)) {
            return; // On cooldown
        }

        auto& config = Config::GetInstance();
        // Execute decision
        if (decision.action != ActionType::None) {
            bool success = m_executor.Execute(a_actor, decision, state);

            if (success) {
                // Mark action as used (start cooldown)
                m_humanizer.MarkActionUsed(a_actor, decision.action);

                // For movement actions (Strafe, Retreat, Advancing, Backoff), don't reset reaction state
                // They need to be continuously applied, not just once
                // (isMovementAction already declared above)
                if (!isMovementAction) {
                    // Reset reaction state for next reaction (only for non-movement actions)
                    m_humanizer.ResetReactionState(a_actor);
                }
                // For movement actions, keep reaction state active so we can continuously apply movement

                // Track actor by FormID
                auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
                if (formIDOpt.has_value()) {
                    m_processedActors.insert(formIDOpt.value());
                }
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
        // With FormID keys, cleanup is much simpler
        // FormIDs are stable identifiers - they don't become invalid
        // We rely on lazy cleanup: entries are removed when actors leave combat
        // (checked in ProcessActor when actor is not in combat)
        // This avoids expensive LookupByID() calls that could crash
        
        // Note: We don't need to do aggressive cleanup here since:
        // 1. FormIDs don't become invalid (unlike pointers)
        // 2. Entries are cleaned up lazily when actors leave combat
        // 3. Avoiding LookupByID() prevents crashes from invalid FormIDs or deleted forms
        
        // Clean up Humanizer state (it also uses lazy cleanup)
        m_humanizer.Cleanup();
        
        // If we want to be more aggressive, we could add a size limit and remove oldest entries
        // But for now, lazy cleanup is safer and more efficient
    }

    bool CombatDirector::ShouldProcessActor(RE::Actor* a_actor, float a_deltaTime)
    {
        if (!a_actor) {
            return false;
        }

        // Validate actor using safe wrappers - protects against transitional states
        // Actor is passed directly from hook, but could become invalid at any time
        
        // Quick validation - if actor is dead or not in combat, skip early
        if (ActorUtils::SafeIsDead(a_actor) || !ActorUtils::SafeIsInCombat(a_actor)) {
            return false;
        }

        // Skip player
        if (ActorUtils::SafeIsPlayerRef(a_actor)) {
            return false;
        }

        // Only process NPCs, not creatures
        // Check for ActorTypeNPC keyword (NPCs have this, creatures don't)
        if (!ActorUtils::SafeHasKeywordString(a_actor, "ActorTypeNPC")) {
            return false;
        }

        // Double-check: explicitly exclude creatures
        if (ActorUtils::SafeHasKeywordString(a_actor, "ActorTypeCreature")) {
            return false;
        }

        // Additional check: use CalculateCachedOwnerIsNPC as fallback
        // This should match the keyword check, but provides extra safety
        if (!ActorUtils::SafeCalculateCachedOwnerIsNPC(a_actor)) {
            return false;
        }

        // Check if we should only process combat actors
        auto& config = Config::GetInstance();
        if (config.GetPerformance().onlyProcessCombatActors) {
            // Only process actors in combat (already checked above, but check again for consistency)
            if (!ActorUtils::SafeIsInCombat(a_actor)) {
                return false;
            }
        }

        // Check if AI is enabled
        if (!ActorUtils::SafeIsAIEnabled(a_actor)) {
            return false;
        }

        // Throttle processing: only process at configured interval
        // Use FormID as key for safety
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return false; // Can't get FormID, actor is invalid
        }
        RE::FormID formID = formIDOpt.value();
        
        auto timerIt = m_actorProcessTimers.find(formID);
        if (timerIt == m_actorProcessTimers.end()) {
            // First time processing this actor, initialize timer
            m_actorProcessTimers[formID] = 0.0f;
            return true;
        }
        
        timerIt->second += a_deltaTime;
        
        // Only process if interval has passed
        if (timerIt->second < m_processInterval) {
            return false; // Skip processing this frame
        }
        
        // Reset timer for next interval
        timerIt->second = 0.0f;

        return true;
    }
}
