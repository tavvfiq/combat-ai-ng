#pragma once

#include "RE/A/Actor.h"
#include "RE/C/CombatController.h"
#include "DecisionResult.h"
#include "ActorStateData.h"

namespace CombatAI
{
    // Enhances combat style behavior instead of suppressing vanilla AI
    class CombatStyleEnhancer
    {
    public:
        CombatStyleEnhancer() = default;
        ~CombatStyleEnhancer() = default;

        // Enhance decision based on combat style
        // Modifies decision priority, action type, and parameters based on NPC's combat style
        DecisionResult EnhanceDecision(RE::Actor* a_actor, const DecisionResult& a_baseDecision, const ActorStateData& a_state);

        // Get combat style for actor
        static RE::TESCombatStyle* GetCombatStyle(RE::Actor* a_actor);

    private:
        // Enhance based on combat style flags and multipliers
        DecisionResult EnhanceForDuelingStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);
        DecisionResult EnhanceForFlankingStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);
        DecisionResult EnhanceForAggressiveStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);
        DecisionResult EnhanceForDefensiveStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);
        DecisionResult EnhanceForMagicStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);
        DecisionResult EnhanceForRangedStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);

        // Apply combat style multipliers
        void ApplyStyleMultipliers(RE::TESCombatStyle* a_style, DecisionResult& a_decision, const ActorStateData& a_state);
    };
}
