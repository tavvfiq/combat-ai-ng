#pragma once

#include "RE/A/Actor.h"
#include "DecisionResult.h"
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
            float level1ReactionDelayMs = 200.0f; // Level 1 NPCs react slower
            float level50ReactionDelayMs = 100.0f; // Level 50 NPCs react faster
            float level1MistakeChance = 0.4f; // 40% at level 1
            float level50MistakeChance = 0.0f; // 0% at level 50
            float bashCooldownSeconds = 3.0f;
            float dodgeCooldownSeconds = 2.0f;
            float jumpCooldownSeconds = 3.0f;
            // Action-specific mistake chance multipliers (applied to base mistake chance)
            float bashMistakeMultiplier = 0.5f; // Bash is easier, lower mistake chance
            float dodgeMistakeMultiplier = 1.5f; // Dodge requires timing, higher mistake chance
            float jumpMistakeMultiplier = 1.5f; // Jump requires timing, higher mistake chance
            float strafeMistakeMultiplier = 1.2f; // Strafe is moderately difficult
            float powerAttackMistakeMultiplier = 1.0f; // Power attack is standard difficulty
            float attackMistakeMultiplier = 0.8f; // Regular attack is easier
            float sprintAttackMistakeMultiplier = 1.2f; // Sprint attack requires timing
            float retreatMistakeMultiplier = 0.3f; // Retreat is easy, very low mistake chance
            float backoffMistakeMultiplier = 0.5f; // Backoff is relatively easy
            float advancingMistakeMultiplier = 0.7f; // Advancing is moderately easy
            float flankingMistakeMultiplier = 1.0f; // Flanking is tactical but requires coordination
        };

        Humanizer() = default;
        ~Humanizer() = default;

        // Check if actor can react (reaction delay)
        bool CanReact(RE::Actor* a_actor, float a_deltaTime);

        // Check if actor should make a mistake (based on level and action type)
        bool ShouldMakeMistake(RE::Actor* a_actor, ActionType a_action);

        // Check if action is on cooldown
        bool IsOnCooldown(RE::Actor* a_actor, ActionType a_action);

        // Mark action as used (start cooldown)
        void MarkActionUsed(RE::Actor* a_actor, ActionType a_action);

        // Reset reaction state (call after action is executed)
        void ResetReactionState(RE::Actor* a_actor);

        // Update internal state (call per frame)
        void Update(float a_deltaTime);

        // Cleanup invalid actors (call periodically)
        void Cleanup();

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
            std::unordered_map<ActionType, float> cooldowns;
        };

        Config m_config;

        // Per-actor reaction states (keyed by FormID for safety - FormIDs don't become invalid like pointers)
        std::unordered_map<RE::FormID, ActorReactionState> m_reactionStates;

        // Per-actor cooldowns (keyed by FormID for safety)
        std::unordered_map<RE::FormID, ActorCooldownState> m_cooldownStates;

        // Calculate mistake probability based on level and action type
        float CalculateMistakeChance(std::uint16_t a_level, ActionType a_action) const;

        // Initialize reaction delay for actor (scales with level)
        void InitializeReactionDelay(RE::Actor* a_actor);

        // Calculate reaction delay based on actor level
        float CalculateReactionDelay(std::uint16_t a_level) const;

        // Get cooldown duration for an action type
        float GetCooldownForAction(ActionType a_action) const;

        // Get mistake chance multiplier for an action type
        float GetMistakeMultiplierForAction(ActionType a_action) const;
    };
}
