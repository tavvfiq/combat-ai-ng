#include "pch.h"
#include "DecisionMatrix.h"
#include "Config.h"
#include <cmath>

namespace CombatAI
{
    DecisionResult DecisionMatrix::Evaluate(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        std::vector<DecisionResult> possibleDecisions;

        // Evaluate all potential actions
        DecisionResult survivalResult = EvaluateSurvival(a_actor, a_state);
        if (survivalResult.action != ActionType::None) {
            possibleDecisions.push_back(survivalResult);
        }

        DecisionResult interruptResult = EvaluateInterrupt(a_actor, a_state);
        if (interruptResult.action != ActionType::None) {
            possibleDecisions.push_back(interruptResult);
        }

        DecisionResult evasionResult = EvaluateEvasion(a_actor, a_state);
        if (evasionResult.action != ActionType::None) {
            possibleDecisions.push_back(evasionResult);
        }

        DecisionResult offenseResult = EvaluateOffense(a_actor, a_state);
        if (offenseResult.action != ActionType::None) {
            possibleDecisions.push_back(offenseResult);
        }

        // If no decisions, return None
        if (possibleDecisions.empty()) {
            return DecisionResult();
        }

        // Enhance all decisions and find the one with the highest priority
        DecisionResult bestDecision = m_styleEnhancer.EnhanceDecision(a_actor, possibleDecisions[0], a_state);
        for (size_t i = 1; i < possibleDecisions.size(); ++i) {
            // Enhance decision based on combat style (instead of suppressing vanilla AI)
            DecisionResult enhancedDecision = m_styleEnhancer.EnhanceDecision(a_actor, possibleDecisions[i], a_state);
            if (enhancedDecision.priority > bestDecision.priority) {
                bestDecision = enhancedDecision;
            }
        }

        return bestDecision;
    }

    DecisionResult DecisionMatrix::EvaluateInterrupt(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        DecisionResult result;

        // Check if target is valid
        if (!a_state.target.isValid) {
            return result;
        }

        // Can't bash with a bow or crossbow
        if (a_actor) {
            auto equippedWield = a_actor->GetEquippedObject(false);
            if (equippedWield && equippedWield->IsWeapon()) {
                auto weapon = equippedWield->As<RE::TESObjectWEAP>();
                if (weapon && (weapon->IsBow() || weapon->IsCrossbow())) {
                    return result; // Return None
                }
            }
        }

        // Get interrupt distance - use actual weapon reach from state
        // Fallback to config value if weapon reach not available
        auto& config = Config::GetInstance();
        float reachDistance = a_state.weaponReach;
        if (reachDistance <= 0.0f) {
            reachDistance = config.GetDecisionMatrix().interruptMaxDistance;
        } else {
            // Apply reach multiplier from config
            reachDistance *= config.GetDecisionMatrix().interruptReachMultiplier;
        }

        // Trigger: Target is power attacking + Distance < Reach + Target facing towards me
        // Use threshold instead of exact equality (0.9 = roughly facing towards, accounting for floating point precision)
        if (a_state.target.isPowerAttacking && a_state.target.distance < reachDistance && a_state.target.orientationDot > 0.9f) {
            result.action = ActionType::Bash;
            result.priority = 1;
        }

        return result;
    }

    DecisionResult DecisionMatrix::EvaluateEvasion(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        DecisionResult result;

        auto& config = Config::GetInstance();
        
        // Check if target is valid
        if (!a_state.target.isValid || !a_state.self.isIdle) {
            return result;
        }

        // Check stamina
        auto actorOwner = a_actor->AsActorValueOwner();
        float maxStamina = actorOwner->GetBaseActorValue(RE::ActorValue::kStamina);
        float currentStamina = a_state.self.staminaPercent * maxStamina;
        if (currentStamina < config.GetDodgeSystem().dodgeStaminaCost) {
            return result;
        }

        // --- Jump Evasion Logic ---
        if (config.GetDecisionMatrix().enableJumpEvasion) {
            // Check if target is using a bow or crossbow
            if (a_state.target.equippedRightHand && a_state.target.equippedRightHand->IsWeapon()) {
                auto weapon = a_state.target.equippedRightHand->As<RE::TESObjectWEAP>();
                if (weapon && (weapon->IsBow() || weapon->IsCrossbow())) {
                    // Check distance
                    if (a_state.target.distance > config.GetDecisionMatrix().jumpEvasionDistanceMin && 
                        a_state.target.distance < config.GetDecisionMatrix().jumpEvasionDistanceMax &&
                        a_state.target.orientationDot > 0.9f) {
                    
                        // Random chance to jump
                        std::random_device rd;
                        std::mt19937 gen(rd());
                        std::uniform_real_distribution<> dis(0.0, 1.0);
                        if (dis(gen) < config.GetDecisionMatrix().evasionJumpChance) {
                            result.action = ActionType::Jump;
                            result.priority = 1;
                            return result;
                        }
                    }
                }
            }
        }
        
        // --- Dodge Evasion Logic (existing) ---
        if (!config.GetDecisionMatrix().enableEvasionDodge) {
            return result;
        }

        // Trigger: Target is attacking me and close, OR target is blocking
        // Use threshold instead of exact equality (0.7 = roughly facing towards, more lenient for blocking)
        bool shouldDodge = false;
        if (a_state.target.orientationDot > 0.7f) {
            bool conditionsMet = false;
            if (a_state.target.isAttacking || a_state.target.isPowerAttacking) {
                if (a_state.target.distance >= config.GetDecisionMatrix().evasionMinDistance) {
                    conditionsMet = true;
                }
            } else if (a_state.target.isBlocking) {
                conditionsMet = true;
            }

            if (conditionsMet) {
                // evasionDodgeChance is the probability TO dodge
                // If random < chance, we dodge; otherwise we strafe
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> dis(0.0, 1.0);
                if (dis(gen) < config.GetDecisionMatrix().evasionDodgeChance) {
                    shouldDodge = true; // Dodge
                } else {
                    shouldDodge = false; // Strafe instead
                }
            }
        }

        if (shouldDodge) {
            result.action = ActionType::Dodge;
            result.priority = 1;
        } else {
            result.action = ActionType::Strafe;
            result.priority = 1;
            result.direction = CalculateStrafeDirection(a_state);
            result.intensity = 0.7f; // Moderate strafe speed
        }

        return result;
    }

    DecisionResult DecisionMatrix::EvaluateSurvival([[maybe_unused]]RE::Actor* a_actor, const ActorStateData& a_state)
    {
        DecisionResult result;

        // Check if survival retreat is enabled
        auto& config = Config::GetInstance();
        if (!config.GetDecisionMatrix().enableSurvivalRetreat) {
            return result;
        }

        // Get thresholds from config
        const float healthThreshold = config.GetDecisionMatrix().healthThreshold;

        // Trigger: Health < threshold
        if (a_state.self.healthPercent <= healthThreshold) {
            result.action = ActionType::Retreat;
            result.priority = 2; // Survival is highest priority

            // Calculate retreat direction (away from target)
            if (a_state.target.isValid) {
                RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
                toTarget.Unitize();
                result.direction = RE::NiPoint3(-toTarget.x, -toTarget.y, 0.0f); // Opposite direction
            } else {
                // No target, retreat backward
                result.direction = RE::NiPoint3(-a_state.self.forwardVector.x, 
                                                 -a_state.self.forwardVector.y, 
                                                 0.0f);
            }
            result.intensity = 0.8f; // Fast retreat
        }

        return result;
    }

    DecisionResult DecisionMatrix::EvaluateOffense([[maybe_unused]]RE::Actor* a_actor, const ActorStateData& a_state)
    {
        DecisionResult result;

        auto& config = Config::GetInstance();
        if (!config.GetDecisionMatrix().enableOffense) {
            return result;
        }
        
        if (!a_state.target.isValid || !a_state.self.isIdle) {
            return result;
        }

        // Sprint attack logic
        const float sprintAttackMaxDist = config.GetDecisionMatrix().sprintAttackMaxDistance;
        const float sprintAttackMinDist = config.GetDecisionMatrix().sprintAttackMinDistance;
        if (a_state.target.distance > sprintAttackMinDist && a_state.target.distance < sprintAttackMaxDist && a_state.self.staminaPercent > config.GetDecisionMatrix().sprintAttackStaminaThreshold) {
             result.action = ActionType::SprintAttack;
             result.priority = 1;
             return result;
        }

        float reachDistance = a_state.weaponReach;
        if (reachDistance <= 0.0f) {
            reachDistance = 150.0f; // Default fallback
        }

        reachDistance *= config.GetDecisionMatrix().offenseReachMultiplier;

        if (a_state.target.distance < reachDistance) {
            if (a_state.self.staminaPercent > config.GetDecisionMatrix().powerAttackStaminaThreshold) {
                result.action = ActionType::PowerAttack;
                result.priority = 1;
            } else {
                result.action = ActionType::Attack;
                result.priority = 1;
            }
        }

        return result;
    }

    RE::NiPoint3 DecisionMatrix::CalculateStrafeDirection(const ActorStateData& a_state)
    {
        if (!a_state.target.isValid) {
            return RE::NiPoint3(1.0f, 0.0f, 0.0f); // Default right
        }

        // Calculate direction to target
        RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
        toTarget.z = 0.0f; // Keep on horizontal plane
        toTarget.Unitize();

        // Calculate perpendicular (strafe) direction
        // Rotate 90 degrees: (x, y) -> (-y, x) for right, (y, -x) for left
        // We'll use right strafe by default
        RE::NiPoint3 strafeDir(-toTarget.y, toTarget.x, 0.0f);
        strafeDir.Unitize();

        return strafeDir;
    }
}
