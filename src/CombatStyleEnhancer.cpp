#include "pch.h"
#include "CombatStyleEnhancer.h"

namespace CombatAI
{
    RE::TESCombatStyle* CombatStyleEnhancer::GetCombatStyle(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return nullptr;
        }

        // Get from combat controller first (runtime)
        auto& runtimeData = a_actor->GetActorRuntimeData();
        RE::CombatController* controller = runtimeData.combatController;
        if (controller && controller->combatStyle) {
            return controller->combatStyle;
        }

        // Fallback to actor base
        RE::TESNPC* base = a_actor->GetActorBase();
        if (base) {
            return base->GetCombatStyle();
        }

        return nullptr;
    }

    DecisionResult CombatStyleEnhancer::EnhanceDecision(RE::Actor* a_actor, const DecisionResult& a_baseDecision, const ActorStateData& a_state)
    {
        if (!a_actor || a_baseDecision.action == ActionType::None) {
            return a_baseDecision;
        }

        RE::TESCombatStyle* style = GetCombatStyle(a_actor);
        if (!style) {
            return a_baseDecision; // No combat style, use base decision
        }

        DecisionResult enhanced = a_baseDecision;

        // Apply style-specific enhancements based on flags
        if (style->flags.any(RE::TESCombatStyle::FLAG::kDuelingStyle)) {
            enhanced = EnhanceForDuelingStyle(a_actor, enhanced, a_state);
        }

        if (style->flags.any(RE::TESCombatStyle::FLAG::kFlankingStyle)) {
            enhanced = EnhanceForFlankingStyle(a_actor, enhanced, a_state);
        }

        // Apply multipliers based on combat style data
        ApplyStyleMultipliers(style, enhanced, a_state);

        // Determine style type from multipliers and enhance accordingly
        if (style->generalData.offensiveMult > style->generalData.defensiveMult * 1.5f) {
            enhanced = EnhanceForAggressiveStyle(a_actor, enhanced, a_state);
        } else if (style->generalData.defensiveMult > style->generalData.offensiveMult * 1.5f) {
            enhanced = EnhanceForDefensiveStyle(a_actor, enhanced, a_state);
        }

        if (style->generalData.magicScoreMult > 0.5f) {
            enhanced = EnhanceForMagicStyle(a_actor, enhanced, a_state);
        }

        if (style->generalData.rangedScoreMult > 0.5f) {
            enhanced = EnhanceForRangedStyle(a_actor, enhanced, a_state);
        }

        return enhanced;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForDuelingStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        DecisionResult result = a_decision;

        // Dueling style: More focused on 1v1, prefers bash interrupts
        if (result.action == ActionType::Bash) {
            result.priority += 1; // Increase priority for bash
        }

        // Less likely to retreat, more likely to stand ground
        if (result.action == ActionType::Retreat) {
            // Only retreat at lower health threshold
            if (a_state.self.healthPercent > 0.15f) {
                result.action = ActionType::None; // Cancel retreat
            }
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForFlankingStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        DecisionResult result = a_decision;

        // Flanking style: Prefers movement and positioning
        if (result.action == ActionType::Strafe) {
            result.priority += 1; // Increase priority for strafe/dodge
            result.intensity = std::min(1.0f, result.intensity * 1.2f); // More aggressive movement
        }

        // More likely to use evasion
        if (a_state.target.isBlocking && result.action == ActionType::None) {
            // Force strafe even if not in base decision
            result.action = ActionType::Strafe;
            result.priority = 2;
            result.direction = RE::NiPoint3(1.0f, 0.0f, 0.0f); // Default right
            result.intensity = 0.8f;
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForAggressiveStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        DecisionResult result = a_decision;

        // Aggressive style: Less defensive, more offensive
        if (result.action == ActionType::Retreat) {
            // Only retreat at very low health
            if (a_state.self.healthPercent > 0.1f) {
                result.action = ActionType::None; // Cancel retreat
            }
        }

        // More likely to bash/interrupt
        if (result.action == ActionType::Bash) {
            result.priority += 1;
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForDefensiveStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        DecisionResult result = a_decision;

        // Defensive style: More cautious, prefers evasion and retreat
        if (result.action == ActionType::Retreat) {
            result.priority += 1; // Increase priority for retreat
            // Retreat earlier
            if (a_state.self.staminaPercent < 0.3f || a_state.self.healthPercent < 0.4f) {
                result.action = ActionType::Retreat;
                result.priority = 3;
            }
        }

        if (result.action == ActionType::Strafe) {
            result.priority += 1; // Prefer evasion
        }

        // Less likely to bash (more defensive)
        if (result.action == ActionType::Bash) {
            // Only bash if very close and safe
            if (a_state.target.distance > 100.0f) {
                result.action = ActionType::None; // Cancel bash if too far
            }
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForMagicStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        DecisionResult result = a_decision;

        // Magic style: Prefers distance, less melee
        if (result.action == ActionType::Bash) {
            // Mages less likely to bash
            result.priority = std::max(0, result.priority - 1);
        }

        // More likely to retreat to maintain distance
        if (a_state.target.distance < 150.0f && result.action == ActionType::None) {
            result.action = ActionType::Retreat;
            result.priority = 2;
            result.intensity = 0.7f;
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForRangedStyle(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        DecisionResult result = a_decision;

        // Ranged style: Similar to magic, prefers distance
        if (result.action == ActionType::Bash) {
            // Archers less likely to bash
            result.priority = std::max(0, result.priority - 1);
        }

        // Prefer strafe to maintain optimal range
        if (result.action == ActionType::Strafe) {
            result.priority += 1;
        }

        return result;
    }

    void CombatStyleEnhancer::ApplyStyleMultipliers(RE::TESCombatStyle* a_style, DecisionResult& a_decision, const ActorStateData& a_state)
    {
        if (!a_style) {
            return;
        }

        // Apply melee multipliers
        if (a_decision.action == ActionType::Bash) {
            // Bash multiplier affects bash priority
            float bashMult = a_style->meleeData.bashMult;
            if (bashMult > 1.0f) {
                a_decision.priority += 1;
            } else if (bashMult < 0.5f) {
                a_decision.priority = std::max(0, a_decision.priority - 1);
            }
        }

        // Apply close range multipliers
        if (a_decision.action == ActionType::Strafe && a_state.target.distance < 200.0f) {
            float circleMult = a_style->closeRangeData.circleMult;
            a_decision.intensity *= circleMult;
            a_decision.intensity = std::clamp(a_decision.intensity, 0.0f, 1.0f);
        }

        // Apply fallback multiplier
        if (a_decision.action == ActionType::Retreat) {
            float fallbackMult = a_style->closeRangeData.fallbackMult;
            if (fallbackMult > 1.0f) {
                a_decision.priority += 1;
            } else if (fallbackMult < 0.5f) {
                a_decision.priority = std::max(0, a_decision.priority - 1);
            }
        }

        // Apply avoid threat chance (affects evasion)
        if (a_decision.action == ActionType::Strafe) {
            float avoidThreat = a_style->generalData.avoidThreatChance;
            if (avoidThreat > 0.5f) {
                a_decision.priority += 1;
            }
        }
    }
}
