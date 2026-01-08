#include "pch.h"
#include "Humanizer.h"
#include <random>

namespace CombatAI
{
    namespace
    {
        // Thread-safe random number generator
        std::random_device g_rd;
        std::mt19937 g_gen(g_rd());
    }

    bool Humanizer::CanReact(RE::Actor* a_actor, float a_deltaTime)
    {
        if (!a_actor) {
            return false;
        }

        auto& state = m_reactionStates[a_actor];

        // Initialize delay if not set
        if (state.reactionDelay == 0.0f) {
            InitializeReactionDelay(a_actor);
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

    bool Humanizer::ShouldMakeMistake(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        std::uint16_t level = a_actor->GetLevel();
        float mistakeChance = CalculateMistakeChance(level);

        if (mistakeChance <= 0.0f) {
            return false;
        }

        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(g_gen) < mistakeChance;
    }

    bool Humanizer::IsOnCooldown(RE::Actor* a_actor, const std::string& a_actionName)
    {
        if (!a_actor) {
            return true; // Safe default
        }

        auto it = m_cooldownStates.find(a_actor);
        if (it == m_cooldownStates.end()) {
            return false;
        }

        auto& cooldowns = it->second.cooldowns;
        auto cooldownIt = cooldowns.find(a_actionName);
        if (cooldownIt == cooldowns.end()) {
            return false;
        }

        return cooldownIt->second > 0.0f;
    }

    void Humanizer::MarkActionUsed(RE::Actor* a_actor, const std::string& a_actionName)
    {
        if (!a_actor) {
            return;
        }

        auto& cooldownState = m_cooldownStates[a_actor];
        
        if (a_actionName == "bash") {
            cooldownState.cooldowns["bash"] = m_config.bashCooldownSeconds;
        } else if (a_actionName == "dodge") {
            // Dodge cooldown (TK Dodge may have its own cooldown, but we add ours too)
            cooldownState.cooldowns["dodge"] = 2.0f; // 2 second cooldown for dodge
        }
        // Add more actions as needed
    }

    void Humanizer::Update(float a_deltaTime)
    {
        // Update cooldowns
        for (auto& [actor, cooldownState] : m_cooldownStates) {
            for (auto& [action, time] : cooldownState.cooldowns) {
                if (time > 0.0f) {
                    time -= a_deltaTime;
                    if (time < 0.0f) {
                        time = 0.0f;
                    }
                }
            }
        }

        // Clean up invalid actors (optional - can be expensive)
        // For now, we'll let the map grow. Could add cleanup logic if needed.
    }

    float Humanizer::CalculateMistakeChance(std::uint16_t a_level) const
    {
        if (a_level >= 50) {
            return m_config.level50MistakeChance;
        }

        // Linear interpolation between level 1 and 50
        float t = static_cast<float>(a_level - 1) / 49.0f;
        return m_config.level1MistakeChance * (1.0f - t) + m_config.level50MistakeChance * t;
    }

    void Humanizer::InitializeReactionDelay(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }

        auto& state = m_reactionStates[a_actor];

        // Random delay: base + variance
        std::uniform_real_distribution<float> dist(0.0f, m_config.reactionVarianceMs);
        float variance = dist(g_gen);
        state.reactionDelay = m_config.baseReactionDelayMs + variance;
        state.reactionTimer = 0.0f;
        state.canReact = false;
    }
}
