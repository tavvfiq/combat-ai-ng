#include "pch.h"
#include "DecisionMatrix.h"
#include "Config.h"
#include <cmath>
#include <string>
#include <format>

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

        DecisionResult backoffResult = EvaluateBackoff(a_actor, a_state);
        if (backoffResult.action != ActionType::None) {
            possibleDecisions.push_back(backoffResult);
        }

        DecisionResult offenseResult = EvaluateOffense(a_actor, a_state);
        if (offenseResult.action != ActionType::None) {
            possibleDecisions.push_back(offenseResult);
        }

        // If no decisions, return None
        if (possibleDecisions.empty()) {
            return DecisionResult();
        }

        // for debugging
        std::string actions;
        auto& config = Config::GetInstance();
        auto selectedRef = RE::Console::GetSelectedRef();

        bool debugEnabled = config.GetGeneral().enableDebugLog;

        // Enhance all decisions and find the one with the highest priority
        std::vector<DecisionResult> enhancedDecisions;
        int highestPriority = -1;
        for (size_t i = 0; i < possibleDecisions.size(); ++i) {
            DecisionResult enhancedDecision = m_styleEnhancer.EnhanceDecision(a_actor, possibleDecisions[i], a_state);
            enhancedDecisions.push_back(enhancedDecision);
            
            if (enhancedDecision.priority >= highestPriority) {
                highestPriority = enhancedDecision.priority;
            }
            
            if (debugEnabled && selectedRef && a_actor == selectedRef.get()) {
                actions += std::format("({},{}),", static_cast<int>(enhancedDecision.action), enhancedDecision.priority);
            }
        }

        // Second pass: collect all decisions with highest priority
        std::vector<DecisionResult> topPriorityDecisions;
        for (const auto& decision : enhancedDecisions) {
            if (decision.priority == highestPriority) {
                topPriorityDecisions.push_back(decision);
            }
        }

        // Tie-breaking: select best decision from top priority ones
        DecisionResult bestDecision = SelectBestFromTie(topPriorityDecisions, a_state);

        if (debugEnabled && selectedRef && a_actor == selectedRef.get()) {
            std::ostringstream logs;
            logs << "actor: " << a_actor->GetFormID() 
                << " | action: " << static_cast<int>(bestDecision.action) 
                << " | possible actions (a,p): " << actions;
            ConsolePrint(logs.str().c_str());
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
            // Bash is a quick interrupt - high intensity for immediate response
            result.intensity = 1.0f; // Maximum intensity for urgent interrupt
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
                            // Jump evasion - high intensity for urgent escape
                            result.intensity = 0.9f; // High intensity for jump evasion
                            return result;
                        }
                    }
                }
            }
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

            // Check stamina
            auto actorOwner = a_actor->AsActorValueOwner();
            float maxStamina = actorOwner->GetBaseActorValue(RE::ActorValue::kStamina);
            float currentStamina = a_state.self.staminaPercent * maxStamina;
            if (currentStamina < config.GetDodgeSystem().dodgeStaminaCost) {
                conditionsMet = false;
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
            // Dodge intensity based on urgency (closer = more urgent)
            float distance = a_state.target.distance;
            if (distance < config.GetDecisionMatrix().evasionMinDistance) {
                result.intensity = 1.0f; // Maximum intensity when very close
            } else if (distance < config.GetDecisionMatrix().evasionMinDistance && distance < config.GetDecisionMatrix().sprintAttackMaxDistance) {
                result.intensity = 0.8f; // High intensity when close
            } else {
                result.intensity = 0.6f; // Moderate intensity when further
            }
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

        if (a_state.target.distance > sprintAttackMaxDist) {
            result.action = ActionType::Advancing;
            result.priority = 1;
            
            // Calculate advancing direction (towards target)
            if (a_state.target.isValid) {
                RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
                toTarget.z = 0.0f; // Keep on horizontal plane
                toTarget.Unitize();
                result.direction = toTarget; // Move towards target
            } else {
                // No target, use forward direction
                result.direction = a_state.self.forwardVector;
            }
            
            // Set intensity based on how far away we are
            float distance = a_state.target.distance;
            float maxDist = sprintAttackMaxDist;
            if (distance > maxDist * 2.0f) {
                result.intensity = 1.0f; // Fast advance if very far
            } else if (distance > maxDist * 1.5f) {
                result.intensity = 0.8f; // Medium-fast advance
            } else {
                result.intensity = 0.6f; // Normal advance
            }
            
            return result;
        }

        if (a_state.target.distance > sprintAttackMinDist && a_state.target.distance < sprintAttackMaxDist && a_state.self.staminaPercent > config.GetDecisionMatrix().sprintAttackStaminaThreshold) {
            result.action = ActionType::SprintAttack;
            result.priority = 1;
            // Sprint attack - high intensity for aggressive gap closing
            result.intensity = 0.9f; // High intensity for sprint attack
            return result;
            return result;
        }

        float reachDistance = a_state.weaponReach;
        if (reachDistance <= 0.0f) {
            reachDistance = 150.0f; // Default fallback
        }

        reachDistance *= config.GetDecisionMatrix().offenseReachMultiplier;

        if (a_state.target.distance <= reachDistance) {
            if (a_state.self.staminaPercent > config.GetDecisionMatrix().powerAttackStaminaThreshold) {
                result.action = ActionType::PowerAttack;
                result.priority = 1;
                // Power attack - committed attack, high intensity
                result.intensity = 0.8f; // High intensity for committed power attack
            } else {
                result.action = ActionType::Attack;
                result.priority = 1;
                // Normal attack - moderate intensity
                result.intensity = 0.6f; // Moderate intensity for normal attack
            }
        }

        return result;
    }

    DecisionResult DecisionMatrix::EvaluateBackoff(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        DecisionResult result;

        // Check if target is valid
        if (!a_state.target.isValid) {
            return result;
        }

        auto& config = Config::GetInstance();
        const float sprintAttackMaxDist = config.GetDecisionMatrix().sprintAttackMaxDistance;
        const float sprintAttackMinDist = config.GetDecisionMatrix().sprintAttackMinDistance;

        // dont backoff if target is too far away and not facing me
        if (a_state.target.distance > sprintAttackMaxDist && a_state.target.orientationDot < 0.7f) {
            return result;
        }

        // Check if target is casting magic OR drawing bow
        bool shouldBackoff = false;
        if (a_state.target.isCasting || a_state.target.isDrawingBow) {
            shouldBackoff = true;
        }

        if (shouldBackoff) {
            result.action = ActionType::Backoff;
            result.priority = 2; // High priority (between Evasion and Survival)
            
            // Calculate backoff direction (away from target)
            RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
            toTarget.z = 0.0f; // Keep on horizontal plane
            toTarget.Unitize();
            result.direction = RE::NiPoint3(-toTarget.x, -toTarget.y, 0.0f); // Opposite direction
            
            // Set intensity based on distance - closer = faster backoff
            float distance = a_state.target.distance;
            if (distance <= sprintAttackMinDist) {
                result.intensity = 1.0f; // Fast backoff if very close
            } else if (distance >= sprintAttackMinDist && distance <= sprintAttackMaxDist) {
                result.intensity = 0.7f; // Medium backoff
            } else {
                result.intensity = 0.5f; // Slower backoff if far
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

    DecisionResult DecisionMatrix::SelectBestFromTie(const std::vector<DecisionResult>& a_decisions, const ActorStateData& a_state)
    {
        if (a_decisions.empty()) {
            return DecisionResult();
        }
        
        if (a_decisions.size() == 1) {
            return a_decisions[0];
        }
        
        // Tie-breaking criteria (in order of importance):
        // 1. Prefer actions with higher intensity (more committed)
        // 2. Prefer offensive actions over defensive when in good health
        // 3. Prefer defensive actions when health is low
        // 4. Prefer actions that work better at current distance
        // 5. Random selection for variety
        
        DecisionResult bestDecision = a_decisions[0];
        float bestScore = CalculateDecisionScore(a_decisions[0], a_state);
        
        for (size_t i = 1; i < a_decisions.size(); ++i) {
            float score = CalculateDecisionScore(a_decisions[i], a_state);
            if (score > bestScore) {
                bestScore = score;
                bestDecision = a_decisions[i];
            } else if (score == bestScore) {
                // If scores are equal, add some randomness for variety
                static std::random_device rd;
                static std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, 1);
                if (dis(gen) == 1) {
                    bestDecision = a_decisions[i];
                }
            }
        }
        
        return bestDecision;
    }

    float DecisionMatrix::CalculateDecisionScore(const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        float score = 0.0f;
        
        // Factor 1: Intensity (higher = more committed, better)
        score += a_decision.intensity * 10.0f;
        
        // Factor 2: Health-based preference
        if (a_state.self.healthPercent > 0.5f) {
            // Good health: prefer offensive actions
            if (a_decision.action == ActionType::Attack || 
                a_decision.action == ActionType::PowerAttack || 
                a_decision.action == ActionType::SprintAttack ||
                a_decision.action == ActionType::Bash) {
                score += 5.0f;
            }
        } else {
            // Low health: prefer defensive actions
            if (a_decision.action == ActionType::Retreat || 
                a_decision.action == ActionType::Backoff || 
                a_decision.action == ActionType::Dodge ||
                a_decision.action == ActionType::Strafe) {
                score += 5.0f;
            }
        }
        
        // Factor 3: Distance-based preference
        float distance = a_state.target.distance;
        if (a_decision.action == ActionType::Advancing && distance > 800.0f) {
            score += 3.0f; // Prefer advancing when very far
        } else if (a_decision.action == ActionType::Bash && distance < 200.0f) {
            score += 3.0f; // Prefer bash when very close
        } else if ((a_decision.action == ActionType::Attack || a_decision.action == ActionType::PowerAttack) 
                && distance >= 150.0f && distance <= 300.0f) {
            score += 2.0f; // Prefer attacks at optimal range
        }
        
        // Factor 4: Action type priority (implicit ordering)
        // Higher enum values get slight preference (for variety)
        score += static_cast<float>(static_cast<int>(a_decision.action)) * 0.1f;
        
        return score;
    }
}
