#include "pch.h"
#include "Humanizer.h"
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

        // Only process actors in combat - clean up if not in combat
        // Use try-catch to protect against dangling pointers
        try {
            if (!a_actor->IsInCombat()) {
                // Remove reaction state if actor left combat
                auto it = m_reactionStates.find(a_actor);
                if (it != m_reactionStates.end()) {
                    m_reactionStates.erase(it);
                }
                return false;
            }
        } catch (...) {
            // Actor access failed - pointer was dangling
            m_reactionStates.erase(a_actor);
            return false;
        }

        auto& state = m_reactionStates[a_actor];

        // Initialize delay if not set
        if (state.reactionDelay == 0.0f) {
            InitializeReactionDelay(a_actor);
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

        auto it = m_reactionStates.find(a_actor);
        if (it != m_reactionStates.end()) {
            // Reset timer and delay, will be re-initialized on next CanReact call
            it->second.reactionTimer = 0.0f;
            it->second.reactionDelay = 0.0f;
            it->second.canReact = false;
        }
    }

    bool Humanizer::ShouldMakeMistake(RE::Actor* a_actor, ActionType a_action)
    {
        if (!a_actor) {
            return false;
        }

        // Get level - use try-catch to protect against invalid actor access
        std::uint16_t level = 1;
        try {
            level = a_actor->GetLevel();
        } catch (...) {
            // Level access failed - actor may be invalid, use default
            level = 1;
        }
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

        auto it = m_cooldownStates.find(a_actor);
        if (it == m_cooldownStates.end()) {
            return false;
        }

        auto& cooldowns = it->second.cooldowns;
        auto cooldownIt = cooldowns.find(a_action);
        if (cooldownIt == cooldowns.end()) {
            return false;
        }

        return cooldownIt->second > 0.0f;
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

        float cooldown = GetCooldownForAction(a_action);
        if (cooldown <= 0.0f) {
            return; // No cooldown for this action
        }

        auto& cooldownState = m_cooldownStates[a_actor];
        
        // Map Strafe to Dodge cooldown (they share the same cooldown)
        ActionType cooldownKey = (a_action == ActionType::Strafe) ? ActionType::Dodge : a_action;
        cooldownState.cooldowns[cooldownKey] = cooldown;
    }

    void Humanizer::Update(float a_deltaTime)
    {
        // Update cooldowns and clean up expired ones
        auto actorIt = m_cooldownStates.begin();
        while (actorIt != m_cooldownStates.end()) {
            RE::Actor* actor = actorIt->first;
            
            // Validate actor pointer before accessing its cooldown state
            // CRITICAL: Use try-catch because stored pointers can become dangling
            bool isValid = false;
            try {
                isValid = actor && !actor->IsDead() && actor->IsInCombat();
            } catch (...) {
                isValid = false; // Actor access failed - pointer was dangling
            }
            
            if (!isValid) {
                // Actor is invalid, remove entire entry
                actorIt = m_cooldownStates.erase(actorIt);
                continue;
            }
            
            auto& cooldowns = actorIt->second.cooldowns;
            auto it = cooldowns.begin();
            while (it != cooldowns.end()) {
                if (it->second > 0.0f) {
                    it->second -= a_deltaTime;
                    if (it->second <= 0.0f) {
                        // Cooldown expired, remove from map to save memory
                        it = cooldowns.erase(it);
                    } else {
                        ++it;
                    }
                } else {
                    // Already zero or negative, remove it
                    it = cooldowns.erase(it);
                }
            }
            
            // If all cooldowns expired, remove the entire actor entry
            if (cooldowns.empty()) {
                actorIt = m_cooldownStates.erase(actorIt);
            } else {
                ++actorIt;
            }
        }
    }

    void Humanizer::Cleanup()
    {
        // Remove invalid actors from reaction states
        // CRITICAL: Use try-catch because stored pointers can become dangling
        auto reactionIt = m_reactionStates.begin();
        while (reactionIt != m_reactionStates.end()) {
            RE::Actor* actor = reactionIt->first;
            bool isValid = false;
            try {
                isValid = actor && !actor->IsDead() && actor->IsInCombat();
            } catch (...) {
                isValid = false; // Actor access failed - pointer was dangling
            }
            
            if (!isValid) {
                reactionIt = m_reactionStates.erase(reactionIt);
            } else {
                ++reactionIt;
            }
        }

        // Remove invalid actors from cooldown states
        auto cooldownIt = m_cooldownStates.begin();
        while (cooldownIt != m_cooldownStates.end()) {
            RE::Actor* actor = cooldownIt->first;
            bool isValid = false;
            try {
                isValid = actor && !actor->IsDead() && actor->IsInCombat();
            } catch (...) {
                isValid = false; // Actor access failed - pointer was dangling
            }
            
            if (!isValid) {
                cooldownIt = m_cooldownStates.erase(cooldownIt);
            } else {
                ++cooldownIt;
            }
        }
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

        auto& state = m_reactionStates[a_actor];

        // Calculate base delay based on actor level
        // Use try-catch to protect against invalid actor access
        std::uint16_t level = 1;
        try {
            level = a_actor->GetLevel();
        } catch (...) {
            // Level access failed - actor may be invalid, use default
            level = 1;
        }
        float baseDelay = CalculateReactionDelay(level);

        // Random variance: base + variance
        std::uniform_real_distribution<float> dist(0.0f, m_config.reactionVarianceMs);
        float variance = dist(g_gen);
        state.reactionDelay = baseDelay + variance;
        state.reactionTimer = 0.0f;
        state.canReact = false;
    }
}
