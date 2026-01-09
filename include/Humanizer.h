#pragma once

#include "RE/A/Actor.h"
#include <chrono>
#include <unordered_map>

namespace CombatAI
{
    // Manages organic feel: reaction latency, mistakes, cooldowns
    class Humanizer
    {
    public:
        struct Config
        {
            float baseReactionDelayMs = 150.0f;
            float reactionVarianceMs = 100.0f;
            float level1MistakeChance = 0.4f; // 40% at level 1
            float level50MistakeChance = 0.0f; // 0% at level 50
            float bashCooldownSeconds = 3.0f;
        };

        Humanizer() = default;
        ~Humanizer() = default;

        // Check if actor can react (reaction delay)
        bool CanReact(RE::Actor* a_actor, float a_deltaTime);

        // Check if actor should make a mistake (based on level)
        bool ShouldMakeMistake(RE::Actor* a_actor);

        // Check if action is on cooldown
        bool IsOnCooldown(RE::Actor* a_actor, const std::string& a_actionName);

        // Mark action as used (start cooldown)
        void MarkActionUsed(RE::Actor* a_actor, const std::string& a_actionName);

        // Update internal state (call per frame)
        void Update(float a_deltaTime);

        // Configuration
        void SetConfig(const Config& a_config) { m_config = a_config; }
        const Config& GetConfig() const { return m_config; }

    private:
        struct ActorReactionState
        {
            float reactionTimer = 0.0f;
            float reactionDelay = 0.0f;
            bool canReact = false;
        };

        struct ActorCooldownState
        {
            std::unordered_map<std::string, float> cooldowns;
        };

        Config m_config;

        // Per-actor reaction states
        std::unordered_map<RE::Actor*, ActorReactionState> m_reactionStates;

        // Per-actor cooldowns
        std::unordered_map<RE::Actor*, ActorCooldownState> m_cooldownStates;

        // Calculate mistake probability based on level
        float CalculateMistakeChance(std::uint16_t a_level) const;

        // Initialize reaction delay for actor
        void InitializeReactionDelay(RE::Actor* a_actor);
    };
}
