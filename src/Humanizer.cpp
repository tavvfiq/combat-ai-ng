#include "pch.h"
#include "Humanizer.h"
#include "ActorUtils.h"
#include <random>

namespace CombatAI
{
    namespace
    {
        // Thread-local random number generators (one per thread)
        thread_local std::random_device g_rd;
        thread_local std::mt19937 g_gen(g_rd());
    }

    bool Humanizer::CanReact(RE::Actor* a_actor, float a_deltaTime)
    {
        if (!a_actor) {
            return false;
        }

        // Get FormID safely - use FormID as key instead of raw pointer
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return false; // Can't get FormID, actor is invalid
        }
        RE::FormID formID = formIDOpt.value();

        // Validate FormID before using it (should not be 0 or invalid)
        // IMPORTANT: Check FormID BEFORE any map operations to prevent crashes
        if (formID == RE::FormID(0)) {
            return false; // Invalid FormID
        }

        // Only process actors in combat - clean up if not in combat
        if (!ActorUtils::SafeIsInCombat(a_actor)) {
            // Remove reaction state if actor left combat
            m_reactionStates.Erase(formID);
            return false;
        }
        
        // Use thread-safe map operations - get or create default state
        auto* statePtr = m_reactionStates.GetOrCreateDefault(formID);
        if (!statePtr) {
            return false; // Failed to get or create
        }
        
        if (!statePtr) {
            return false; // Failed to get state
        }
        auto& state = *statePtr;

        // Initialize delay if not set
        // Re-validate actor before accessing it (could become invalid)
        if (state.reactionDelay == 0.0f) {
            // Validate actor is still valid before initializing
            if (!ActorUtils::SafeIsInCombat(a_actor)) {
                // Actor left combat, clean up and exit
                m_reactionStates.Erase(formID);
                return false;
            }
            InitializeReactionDelay(a_actor);
            // Re-get state after initialization
            statePtr = m_reactionStates.GetMutable(formID);
            if (!statePtr) {
                return false; // State was removed or not found
            }
            state = *statePtr;
        }

        // Re-validate actor before accessing state (actor could become invalid)
        if (!ActorUtils::SafeIsInCombat(a_actor)) {
            // Actor left combat, clean up and exit
            m_reactionStates.Erase(formID);
            return false;
        }

        // If already can react, return true (don't reset here - reset after action is executed)
        if (state.canReact) {
            return true;
        }

        // Update timer
        state.reactionTimer += a_deltaTime * 1000.0f; // Convert to milliseconds

        // Check if delay has passed
        if (state.reactionTimer >= state.reactionDelay) {
            state.canReact = true;
            return true;
        }

        return false;
    }

    void Humanizer::ResetReactionState(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }

        // Get FormID safely - use FormID as key instead of raw pointer
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return; // Can't get FormID, actor is invalid
        }
        RE::FormID formID = formIDOpt.value();

        // Validate FormID before using it
        if (formID == RE::FormID(0)) {
            return; // Invalid FormID
        }

        // Use thread-safe map operations
        auto* statePtr = m_reactionStates.GetMutable(formID);
        if (statePtr) {
            // Reset timer and delay, will be re-initialized on next CanReact call
            statePtr->reactionTimer = 0.0f;
            statePtr->reactionDelay = 0.0f;
            statePtr->canReact = false;
        }
    }

    bool Humanizer::ShouldMakeMistake(RE::Actor* a_actor, ActionType a_action)
    {
        if (!a_actor) {
            return false;
        }

        // Get level safely using wrapper
        std::uint16_t level = ActorUtils::SafeGetLevel(a_actor);
        float mistakeChance = CalculateMistakeChance(level, a_action);

        if (mistakeChance <= 0.0f) {
            return false;
        }

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(g_gen) < mistakeChance;
    }

    bool Humanizer::IsOnCooldown(RE::Actor* a_actor, ActionType a_action)
    {
        if (!a_actor) {
            return true; // Safe default
        }

        // Get FormID safely - use FormID as key instead of raw pointer
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return true; // Can't get FormID, actor is invalid - safe default
        }
        RE::FormID formID = formIDOpt.value();

        // Validate FormID before using it
        if (formID == RE::FormID(0)) {
            return true; // Invalid FormID - safe default (on cooldown)
        }

        // Use thread-safe map operations
        auto* cooldownStatePtr = m_cooldownStates.GetMutable(formID);
        if (!cooldownStatePtr) {
            return false;
        }

        // Check nested cooldown map (also thread-safe)
        auto cooldownOpt = cooldownStatePtr->cooldowns.Find(a_action);
        if (!cooldownOpt.has_value()) {
            return false;
        }

        return cooldownOpt.value() > 0.0f;
    }

    float Humanizer::GetCooldownForAction(ActionType a_action) const
    {
        switch (a_action) {
            case ActionType::Bash:
                return m_config.bashCooldownSeconds;
            case ActionType::Dodge:
                // Strafe uses dodge cooldown (TK Dodge system)
                return m_config.dodgeCooldownSeconds;
            case ActionType::Jump:
                return m_config.jumpCooldownSeconds;
            default:
                // Actions without cooldown return 0
                return 0.0f;
        }
    }

    void Humanizer::MarkActionUsed(RE::Actor* a_actor, ActionType a_action)
    {
        if (!a_actor) {
            return;
        }

        // Get FormID safely - use FormID as key instead of raw pointer
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return; // Can't get FormID, actor is invalid
        }
        RE::FormID formID = formIDOpt.value();

        float cooldown = GetCooldownForAction(a_action);
        if (cooldown <= 0.0f) {
            return; // No cooldown for this action
        }

        // Validate FormID before using it
        if (formID == RE::FormID(0)) {
            return; // Invalid FormID
        }

        // Use thread-safe map operations - get or create default
        auto* cooldownStatePtr = m_cooldownStates.GetOrCreateDefault(formID);
        if (!cooldownStatePtr) {
            return; // Failed to get or create
        }
        
        // Map Strafe to Dodge cooldown (they share the same cooldown)
        ActionType cooldownKey = (a_action == ActionType::Strafe) ? ActionType::Dodge : a_action;
        // Use thread-safe nested map
        cooldownStatePtr->cooldowns.Emplace(cooldownKey, cooldown);
    }

    void Humanizer::RecoverFromCorruption()
    {
        // Emergency recovery: clear all maps if they're corrupted
        // This is a last resort when maps become unusable
        // Thread-safe wrappers handle this safely
        m_reactionStates.Clear();
        m_cooldownStates.Clear();
    }

    void Humanizer::Update(float a_deltaTime)
    {
        // Update cooldowns and clean up expired ones
        // Use thread-safe iteration with write lock
        m_cooldownStates.WithWriteLock([&](auto& cooldownStatesMap) {
            auto actorIt = cooldownStatesMap.begin();
            while (actorIt != cooldownStatesMap.end()) {
                auto& cooldownState = actorIt->second;
                
                // Update nested cooldown map (also thread-safe)
                cooldownState.cooldowns.WithWriteLock([&](auto& cooldownsMap) {
                    auto it = cooldownsMap.begin();
                    while (it != cooldownsMap.end()) {
                        if (it->second > 0.0f) {
                            it->second -= a_deltaTime;
                            if (it->second <= 0.0f) {
                                // Cooldown expired, remove from map to save memory
                                it = cooldownsMap.erase(it);
                            } else {
                                ++it;
                            }
                        } else {
                            // Already zero or negative, remove it
                            it = cooldownsMap.erase(it);
                        }
                    }
                });
                
                // If all cooldowns expired, remove the entire actor entry
                cooldownState.cooldowns.WithReadLock([&](const auto& cooldownsMap) {
                    if (cooldownsMap.empty()) {
                        actorIt = cooldownStatesMap.erase(actorIt);
                    } else {
                        ++actorIt;
                    }
                });
            }
        });
    }

    void Humanizer::Cleanup()
    {
        // With FormID keys, cleanup is much simpler
        // FormIDs are stable identifiers - they don't become invalid
        // We rely on lazy cleanup: entries are removed when actors leave combat
        // (checked in CanReact() when actor is not in combat)
        // This avoids expensive LookupByID() calls that could crash
        
        // Note: We don't need to do anything here since:
        // 1. FormIDs don't become invalid (unlike pointers)
        // 2. Entries are cleaned up lazily in CanReact() when actors leave combat
        // 3. Expired cooldowns are cleaned up in Update()
        // 4. Avoiding LookupByID() prevents crashes from invalid FormIDs or deleted forms
        
        // If we want to be more aggressive, we could add a size limit and remove oldest entries
        // But for now, lazy cleanup is safer and more efficient
    }

    float Humanizer::GetMistakeMultiplierForAction(ActionType a_action) const
    {
        switch (a_action) {
            case ActionType::Bash:
                return m_config.bashMistakeMultiplier;
            case ActionType::Dodge:
                return m_config.dodgeMistakeMultiplier;
            case ActionType::Jump:
                return m_config.jumpMistakeMultiplier;
            case ActionType::Strafe:
                return m_config.strafeMistakeMultiplier;
            case ActionType::PowerAttack:
                return m_config.powerAttackMistakeMultiplier;
            case ActionType::Attack:
                return m_config.attackMistakeMultiplier;
            case ActionType::SprintAttack:
                return m_config.sprintAttackMistakeMultiplier;
            case ActionType::Retreat:
                return m_config.retreatMistakeMultiplier;
            case ActionType::Backoff:
                return m_config.backoffMistakeMultiplier;
            case ActionType::Advancing:
                return m_config.advancingMistakeMultiplier;
            case ActionType::Flanking:
                return m_config.flankingMistakeMultiplier;
            default:
                return 1.0f; // Default multiplier for unknown actions
        }
    }

    float Humanizer::CalculateMistakeChance(std::uint16_t a_level, ActionType a_action) const
    {
        // Calculate base mistake chance based on level
        float baseMistakeChance;
        if (a_level >= 50) {
            baseMistakeChance = m_config.level50MistakeChance;
        } else {
            // Linear interpolation between level 1 and 50
            float t = static_cast<float>(a_level - 1) / 49.0f;
            baseMistakeChance = m_config.level1MistakeChance * (1.0f - t) + m_config.level50MistakeChance * t;
        }

        // Apply action-specific multiplier
        float multiplier = GetMistakeMultiplierForAction(a_action);
        return baseMistakeChance * multiplier;
    }

    float Humanizer::CalculateReactionDelay(std::uint16_t a_level) const
    {
        if (a_level >= 50) {
            return m_config.level50ReactionDelayMs;
        }

        // Linear interpolation between level 1 and 50
        float t = static_cast<float>(a_level - 1) / 49.0f;
        return m_config.level1ReactionDelayMs * (1.0f - t) + m_config.level50ReactionDelayMs * t;
    }

    void Humanizer::InitializeReactionDelay(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }

        // Get FormID safely - use FormID as key instead of raw pointer
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return; // Can't get FormID, actor is invalid
        }
        RE::FormID formID = formIDOpt.value();

        // Re-validate actor is still in combat before accessing
        if (!ActorUtils::SafeIsInCombat(a_actor)) {
            // Actor left combat, don't initialize
            return;
        }

        // Validate FormID before using it
        if (formID == RE::FormID(0)) {
            return; // Invalid FormID
        }

        // Use thread-safe map operations - get or create default
        auto* statePtr = m_reactionStates.GetOrCreateDefault(formID);
        if (!statePtr) {
            return; // Failed to get or create
        }

        // Calculate base delay based on actor level
        // Use wrapper to safely get level - validate actor again before accessing
        if (!ActorUtils::SafeIsInCombat(a_actor)) {
            // Actor left combat during initialization, clean up
            m_reactionStates.Erase(formID);
            return;
        }
        
        std::uint16_t level = ActorUtils::SafeGetLevel(a_actor);
        float baseDelay = CalculateReactionDelay(level);

        // Random variance: base + variance
        std::uniform_real_distribution<float> dist(0.0f, m_config.reactionVarianceMs);
        float variance = dist(g_gen);
        statePtr->reactionDelay = baseDelay + variance;
        statePtr->reactionTimer = 0.0f;
        statePtr->canReact = false;
    }
}
