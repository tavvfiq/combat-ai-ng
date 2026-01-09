#include "pch.h"
#include "CombatStyleEnhancer.h"

// Undefine Windows macros that conflict with std functions
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef clamp
#undef clamp
#endif

namespace CombatAI
{
    // Helper function to clamp values (avoids Windows macro issues)
    template<typename T>
    constexpr T ClampValue(const T& value, const T& minVal, const T& maxVal)
    {
        return (value < minVal) ? minVal : ((value > maxVal) ? maxVal : value);
    }
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

        // Apply style-specific enhancements based on flags (mutually exclusive styles)
        if (style->flags.any(RE::TESCombatStyle::FLAG::kDuelingStyle)) {
            enhanced = EnhanceForDuelingStyle(a_actor, enhanced, a_state, style);
        } else if (style->flags.any(RE::TESCombatStyle::FLAG::kFlankingStyle)) {
            enhanced = EnhanceForFlankingStyle(a_actor, enhanced, a_state, style);
        }

        // Apply multipliers based on combat style data (affects all actions)
        ApplyStyleMultipliers(style, enhanced, a_state);

        // Determine style type from multipliers and enhance accordingly
        // These can stack with flag-based styles
        if (style->generalData.offensiveMult > style->generalData.defensiveMult * 1.5f) {
            enhanced = EnhanceForAggressiveStyle(a_actor, enhanced, a_state, style);
        } else if (style->generalData.defensiveMult > style->generalData.offensiveMult * 1.5f) {
            enhanced = EnhanceForDefensiveStyle(a_actor, enhanced, a_state, style);
        }

        if (style->generalData.magicScoreMult > 0.5f) {
            enhanced = EnhanceForMagicStyle(a_actor, enhanced, a_state, style);
        }

        if (style->generalData.rangedScoreMult > 0.5f) {
            enhanced = EnhanceForRangedStyle(a_actor, enhanced, a_state, style);
        }

        return enhanced;
    }

    bool CombatStyleEnhancer::HasMeleeWeapon(RE::Actor* a_actor) const
    {
        if (!a_actor) return false;
        auto weapon = a_actor->GetEquippedObject(false);
        if (weapon && weapon->IsWeapon()) {
            auto weap = weapon->As<RE::TESObjectWEAP>();
            return weap && !weap->IsBow() && !weap->IsCrossbow();
        }
        return false;
    }

    bool CombatStyleEnhancer::HasRangedWeapon(RE::Actor* a_actor) const
    {
        if (!a_actor) return false;
        auto weapon = a_actor->GetEquippedObject(false);
        if (weapon && weapon->IsWeapon()) {
            auto weap = weapon->As<RE::TESObjectWEAP>();
            return weap && (weap->IsBow() || weap->IsCrossbow());
        }
        return false;
    }

    bool CombatStyleEnhancer::HasMagicEquipped(RE::Actor* a_actor) const
    {
        if (!a_actor) return false;
        // Check if casting or has spell equipped
        return a_actor->WhoIsCasting() != 0 || a_actor->GetEquippedObject(true) != nullptr;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForDuelingStyle([[maybe_unused]]RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state, RE::TESCombatStyle* a_style)
    {
        DecisionResult result = a_decision;

        // Dueling style: More focused on 1v1, prefers bash interrupts and precise attacks
        if (result.action == ActionType::Bash) {
            result.priority += 1;
        }

        if (result.action == ActionType::Dodge) {
            result.priority += 1;
        }

        // Prefer power attacks when stamina allows (duelists are precise)
        if (result.action == ActionType::PowerAttack && HasMeleeWeapon(a_actor)) {
            result.priority += 1;
        }

        // Less likely to retreat, use combat style fallback threshold
        if (result.action == ActionType::Retreat) {
            float retreatThreshold = a_style ? a_style->closeRangeData.fallbackMult * 0.1f : 0.15f;
            if (a_state.self.healthPercent > retreatThreshold) {
                result.priority -= 1;
            }
        }

        // Don't use sprint attacks (duelists prefer precision)
        if (result.action == ActionType::SprintAttack) {
            result.priority -= 1;
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForFlankingStyle([[maybe_unused]]RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state, RE::TESCombatStyle* a_style)
    {
        DecisionResult result = a_decision;

        // Flanking style: Prefers movement and positioning
        if (result.action == ActionType::Strafe || result.action == ActionType::Dodge) {
            result.priority += 1;
            // Use circle multiplier for intensity if available
            if (a_style) {
                float circleMult = a_style->closeRangeData.circleMult;
                result.intensity = (std::min)(1.0f, result.intensity * (1.0f + circleMult * 0.2f));
            } else {
                result.intensity = (std::min)(1.0f, result.intensity * 1.2f);
            }
        }

        // Prefer sprint attacks for flanking (closing distance quickly)
        if (result.action == ActionType::SprintAttack) {
            result.priority += 1;
        }

        // Less likely to use power attacks (flankers prefer mobility)
        if (result.action == ActionType::PowerAttack) {
            result.priority = (std::max)(0, result.priority - 1);
        }

        return result;
    }

    RE::NiPoint3 CombatStyleEnhancer::CalculateStrafeDirection(const ActorStateData& a_state)
    {
        if (!a_state.target.isValid) {
            return RE::NiPoint3(1.0f, 0.0f, 0.0f);
        }
        RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
        toTarget.z = 0.0f;
        toTarget.Unitize();
        RE::NiPoint3 strafeDir(-toTarget.y, toTarget.x, 0.0f);
        strafeDir.Unitize();
        return strafeDir;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForAggressiveStyle([[maybe_unused]]RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state, RE::TESCombatStyle* a_style)
    {
        DecisionResult result = a_decision;

        // Aggressive style: Less defensive, more offensive
        if (result.action == ActionType::Retreat) {
            // Use combat style fallback multiplier to determine retreat threshold
            float retreatThreshold = a_style ? a_style->closeRangeData.fallbackMult * 0.05f : 0.1f;
            if (a_state.self.healthPercent > retreatThreshold) {
                result.priority -= 1;
            }
        }

        // Boost offensive actions
        if (result.action == ActionType::Bash || result.action == ActionType::Attack || result.action == ActionType::PowerAttack || result.action == ActionType::SprintAttack) {
            result.priority += 1;
        }

        // Prefer power attacks and sprint attacks
        if (result.action == ActionType::PowerAttack || result.action == ActionType::SprintAttack) {
            result.priority += 1;
        }

        // Less likely to dodge/strafe (aggressive = press forward)
        if (result.action == ActionType::Dodge || result.action == ActionType::Strafe) {
            result.priority = (std::max)(0, result.priority - 1);
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForDefensiveStyle([[maybe_unused]]RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state, RE::TESCombatStyle* a_style)
    {
        DecisionResult result = a_decision;

        // Defensive style: More cautious, prefers evasion and retreat
        if (result.action == ActionType::Retreat) {
            result.priority += 1;
        }

        if (result.action == ActionType::Strafe || result.action == ActionType::Dodge || result.action == ActionType::Jump) {
            result.priority += 1;
        }

        // Less likely to use power attacks (defensive = cautious)
        if (result.action == ActionType::PowerAttack || result.action == ActionType::SprintAttack) {
            result.priority = (std::max)(0, result.priority - 1);
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForMagicStyle([[maybe_unused]]RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state, RE::TESCombatStyle* a_style)
    {
        DecisionResult result = a_decision;

        // Only apply if actually using magic
        if (!HasMagicEquipped(a_actor) && !HasRangedWeapon(a_actor)) {
            return result;
        }

        // Magic style: Prefers distance, less melee
        if (result.action == ActionType::Bash) {
            result.priority = (std::max)(0, result.priority - 2); // Strongly discourage bash
        }

        // Discourage melee attacks
        if (result.action == ActionType::Attack || result.action == ActionType::PowerAttack) {
            result.priority = (std::max)(0, result.priority - 1);
        }

        // Prefer strafe/dodge to maintain range
        if (result.action == ActionType::Strafe || result.action == ActionType::Dodge || result.action == ActionType::Retreat) {
            result.priority += 1;
        }

        return result;
    }

    DecisionResult CombatStyleEnhancer::EnhanceForRangedStyle([[maybe_unused]]RE::Actor* a_actor, const DecisionResult& a_decision, [[maybe_unused]]const ActorStateData& a_state, RE::TESCombatStyle* a_style)
    {
        DecisionResult result = a_decision;

        // Only apply if actually using ranged weapon
        if (!HasRangedWeapon(a_actor)) {
            return result;
        }

        // Ranged style: Similar to magic, prefers distance
        if (result.action == ActionType::Bash) {
            result.priority = (std::max)(0, result.priority - 2); // Strongly discourage bash
        }

        // Discourage melee attacks
        if (result.action == ActionType::Attack || result.action == ActionType::PowerAttack || result.action == ActionType::SprintAttack) {
            result.priority = (std::max)(0, result.priority - 1);
        }

        // Prefer strafe, dodge, or jump to maintain optimal range
        if (result.action == ActionType::Strafe || result.action == ActionType::Dodge || result.action == ActionType::Jump || result.action == ActionType::Retreat) {
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
            float bashMult = a_style->meleeData.bashMult;
            if (bashMult > 1.0f) {
                a_decision.priority += 1;
            } else if (bashMult < 0.5f) {
                a_decision.priority = (std::max)(0, a_decision.priority - 1);
            }
        }

        // Apply power attack multipliers (affects PowerAttack action)
        // Use powerAttackBlockingMult as indicator of power attack preference
        if (a_decision.action == ActionType::PowerAttack) {
            float powerAttackBlockingMult = a_style->meleeData.powerAttackBlockingMult;
            if (powerAttackBlockingMult > 1.0f) {
                a_decision.priority += 1;
            } else if (powerAttackBlockingMult < 0.5f) {
                a_decision.priority = (std::max)(0, a_decision.priority - 1);
            }
        }

        // Apply close range multipliers (affects movement actions)
        if ((a_decision.action == ActionType::Strafe || a_decision.action == ActionType::Dodge) && a_state.target.distance < 200.0f) {
            float circleMult = a_style->closeRangeData.circleMult;
            a_decision.intensity *= circleMult;
            a_decision.intensity = ClampValue(a_decision.intensity, 0.0f, 1.0f);
        }

        // Apply fallback multiplier (affects retreat)
        if (a_decision.action == ActionType::Retreat) {
            float fallbackMult = a_style->closeRangeData.fallbackMult;
            if (fallbackMult > 1.0f) {
                a_decision.priority += 1;
            } else if (fallbackMult < 0.5f) {
                a_decision.priority = (std::max)(0, a_decision.priority - 1);
            }
        }

        // Apply avoid threat chance (affects all evasion actions)
        if (a_decision.action == ActionType::Strafe || a_decision.action == ActionType::Dodge || a_decision.action == ActionType::Jump) {
            float avoidThreat = a_style->generalData.avoidThreatChance;
            if (avoidThreat > 0.5f) {
                a_decision.priority += 1;
            } else if (avoidThreat < 0.3f) {
                a_decision.priority = (std::max)(0, a_decision.priority - 1);
            }
        }

        // Apply melee score multiplier (affects Attack and PowerAttack)
        // Higher melee score means more likely to attack
        if (a_decision.action == ActionType::Attack) {
            float meleeScoreMult = a_style->generalData.meleeScoreMult;
            if (meleeScoreMult > 1.0f) {
                a_decision.priority += 1;
            } else if (meleeScoreMult < 0.5f) {
                a_decision.priority = (std::max)(0, a_decision.priority - 1);
            }
        } else if (a_decision.action == ActionType::PowerAttack) {
            // Power attacks also benefit from melee score
            float meleeScoreMult = a_style->generalData.meleeScoreMult;
            if (meleeScoreMult > 1.0f) {
                a_decision.priority += 1;
            }
        }
    }
}
