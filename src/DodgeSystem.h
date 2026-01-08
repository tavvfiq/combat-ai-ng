#pragma once

#include "RE/A/Actor.h"
#include "ActorStateData.h"

namespace CombatAI
{
    // TK Dodge integration for NPC evasion
    class DodgeSystem
    {
    public:
        struct Config
        {
            float dodgeStaminaCost = 10.0f;
            float iFrameDuration = 0.3f;
            bool enableStepDodge = false;
            bool enableDodgeAttackCancel = true;
        };

        DodgeSystem() = default;
        ~DodgeSystem() = default;

        // Check if actor can dodge
        bool CanDodge(RE::Actor* a_actor);

        // Execute dodge in a specific direction
        bool ExecuteDodge(RE::Actor* a_actor, const RE::NiPoint3& a_dodgeDirection);

        // Execute dodge away from target (for evasion)
        bool ExecuteEvasionDodge(RE::Actor* a_actor, const ActorStateData& a_state);

        // Configuration
        void SetConfig(const Config& a_config) { m_config = a_config; }
        const Config& GetConfig() const { return m_config; }

    private:
        // Determine dodge direction based on target position
        std::string DetermineDodgeDirection(RE::Actor* a_actor, const RE::NiPoint3& a_dodgeDirection);

        // Check if actor is currently dodging
        bool IsDodging(RE::Actor* a_actor);

        Config m_config;
    };
}
