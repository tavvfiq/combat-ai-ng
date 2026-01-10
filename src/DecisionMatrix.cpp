#include "pch.h"
#include "DecisionMatrix.h"
#include "Config.h"
#include "ActorUtils.h"
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

        DecisionResult flankingResult = EvaluateFlanking(a_actor, a_state);
        if (flankingResult.action != ActionType::None) {
            possibleDecisions.push_back(flankingResult);
        }

        DecisionResult feintingResult = EvaluateFeinting(a_actor, a_state);
        if (feintingResult.action != ActionType::None) {
            possibleDecisions.push_back(feintingResult);
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
        float highestPriority = -1.0f;
        for (size_t i = 0; i < possibleDecisions.size(); ++i) {
            DecisionResult enhancedDecision = m_styleEnhancer.EnhanceDecision(a_actor, possibleDecisions[i], a_state);
            enhancedDecisions.push_back(enhancedDecision);
            
            if (enhancedDecision.priority >= highestPriority) {
                highestPriority = enhancedDecision.priority;
            }
            
            if (debugEnabled && selectedRef && a_actor == selectedRef.get()) {
                actions += std::format("({},{:.2f}),", static_cast<int>(enhancedDecision.action), enhancedDecision.priority);
            }
        }

        // Second pass: collect all decisions with highest priority (using epsilon for float comparison)
        const float epsilon = 0.01f;
        std::vector<DecisionResult> topPriorityDecisions;
        for (const auto& decision : enhancedDecisions) {
            if (std::abs(decision.priority - highestPriority) < epsilon) {
                topPriorityDecisions.push_back(decision);
            }
        }

        // Tie-breaking: select best decision from top priority ones
        DecisionResult bestDecision = SelectBestFromTie(topPriorityDecisions, a_state);

        if (debugEnabled && selectedRef && a_actor == selectedRef.get()) {
            std::ostringstream logs;
            if (auto formID = ActorUtils::SafeGetFormID(a_actor)) {
                logs << "actor: " << std::hex << *formID << std::dec 
                    << " | action: " << static_cast<int>(bestDecision.action) 
                    << " | possible actions (a,p): " << actions;
                ConsolePrint(logs.str().c_str());
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
            auto equippedWield = ActorUtils::SafeGetEquippedObject(a_actor, false);
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
            result.priority = 1.2f; // Interrupt priority
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
                            result.priority = 1.5f; // Evasion priority
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
            auto actorOwner = ActorUtils::SafeAsActorValueOwner(a_actor);
            if (actorOwner) {
                float maxStamina = actorOwner->GetBaseActorValue(RE::ActorValue::kStamina);
                float currentStamina = a_state.self.staminaPercent * maxStamina;
                if (currentStamina < config.GetDodgeSystem().dodgeStaminaCost) {
                    conditionsMet = false;
                }
            }

            if (conditionsMet) {
                // evasionDodgeChance is the probability TO dodge
                // If random < chance, we dodge; otherwise we strafe
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_real_distribution<> dis(0.0, 1.0);
                
                // Multiple enemies = prefer dodge over strafe (more defensive)
                float dodgeChance = config.GetDecisionMatrix().evasionDodgeChance;
                if (a_state.combatContext.enemyCount > 1) {
                    dodgeChance = (std::min)(1.0f, dodgeChance * 1.5f); // Increase dodge chance when outnumbered
                }
                
                if (dis(gen) < dodgeChance) {
                    shouldDodge = true; // Dodge
                } else {
                    shouldDodge = false; // Strafe instead
                }
            }
        }

        if (shouldDodge) {
            result.action = ActionType::Dodge;
            result.priority = 1.5f; // Base evasion priority
            
            // Multiple enemies = higher priority for dodge
            if (a_state.combatContext.enemyCount > 1) {
                result.priority += 0.5f; // More defensive when outnumbered
            }
            
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
            
            // Base tactical priority - higher than base offense to encourage positioning
            float strafePriority = 1.3f;
            
            // Boost priority when target is attacking (evasion + tactical)
            if (a_state.target.isAttacking || a_state.target.isPowerAttacking) {
                strafePriority = 1.6f; // Higher than base offense when target is attacking
            }
            
            // Boost priority when target is blocking (good time to reposition for better angle)
            if (a_state.target.isBlocking && !a_state.target.isAttacking) {
                strafePriority += 0.2f;
            }
            
            // Boost priority when in melee range (tactical positioning is important)
            float reachDistance = a_state.weaponReach;
            if (reachDistance <= 0.0f) {
                reachDistance = 150.0f;
            }
            if (a_state.target.distance <= reachDistance * 1.5f) {
                strafePriority += 0.2f; // Higher priority in melee range for positioning
            }
            
            // Boost priority if we just attacked (time to reposition)
            if (a_state.self.attackState == RE::ATTACK_STATE_ENUM::kFollowThrough) {
                strafePriority += 0.3f; // Just finished attack, reposition
            }
            
            // Multiple enemies = higher priority for strafe too
            if (a_state.combatContext.enemyCount > 1) {
                strafePriority += 0.3f; // More defensive when outnumbered
            }
            
            result.priority = strafePriority;
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
            result.priority = 2.0f; // Survival is highest priority

            // Multiple enemies = more urgent retreat
            if (a_state.combatContext.enemyCount > 1) {
                result.priority += 0.5f; // Higher priority with multiple enemies
                result.intensity = 1.0f; // Faster retreat when outnumbered
            } else {
                result.intensity = 0.8f; // Normal retreat speed
            }

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
            // Don't advance if target is actively attacking (stay defensive)
            if (a_state.target.isAttacking || a_state.target.isPowerAttacking) {
                return result; // Prefer strafe/evasion instead
            }
            
            result.action = ActionType::Advancing;
            
            // Multiple enemies = less likely to advance (might want to stay defensive)
            float basePriority = 0.7f;
            if (a_state.combatContext.enemyCount > a_state.combatContext.allyCount + 1) {
                basePriority = 0.5f; // Lower priority when outnumbered
            }
            
            // Boost if target is vulnerable (good time to close distance)
            if (a_state.target.isCasting || a_state.target.isDrawingBow) {
                basePriority += 0.2f;
            }
            if (a_state.target.knockState != RE::KNOCK_STATE_ENUM::kNormal) {
                basePriority += 0.3f;
            }
            
            result.priority = basePriority;
            
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
            // Don't sprint attack if target is actively attacking (too risky)
            if (a_state.target.isAttacking || a_state.target.isPowerAttacking) {
                return result; // Prefer advancing or strafe instead
            }
            
            // Only sprint attack when target is vulnerable
            bool hasGoodOpening = false;
            if (a_state.target.isCasting || a_state.target.isDrawingBow || 
                a_state.target.knockState != RE::KNOCK_STATE_ENUM::kNormal ||
                (a_state.target.isBlocking && !a_state.target.isAttacking)) {
                hasGoodOpening = true;
            }
            
            if (!hasGoodOpening) {
                return result; // No good opening - prefer advancing/strafe
            }
            
            result.action = ActionType::SprintAttack;
            
            // Multiple enemies = less likely to sprint attack (risky when outnumbered)
            float basePriority = 0.8f;
            if (a_state.combatContext.enemyCount > a_state.combatContext.allyCount + 1) {
                basePriority = 0.6f; // Lower priority when outnumbered
            }
            
            // Boost if target is vulnerable
            if (a_state.target.isCasting || a_state.target.isDrawingBow) {
                basePriority += 0.2f;
            }
            if (a_state.target.knockState != RE::KNOCK_STATE_ENUM::kNormal) {
                basePriority += 0.3f;
            }
            
            result.priority = basePriority;
            
            // Sprint attack - high intensity for aggressive gap closing
            result.intensity = 0.9f; // High intensity for sprint attack
            return result;
        }

        float reachDistance = a_state.weaponReach;
        if (reachDistance <= 0.0f) {
            reachDistance = 150.0f; // Default fallback
        }

        reachDistance *= config.GetDecisionMatrix().offenseReachMultiplier;

        if (a_state.target.distance <= reachDistance) {
            // TACTICAL CONSIDERATIONS: Don't attack blindly - need good opening
            
            // Don't attack if target is actively attacking (too risky - prefer evasion/strafe)
            if (a_state.target.isAttacking || a_state.target.isPowerAttacking) {
                return result; // Let evasion handle this - strafe/dodge instead
            }
            
            // Don't attack if we just finished an attack (should reposition first)
            if (a_state.self.attackState == RE::ATTACK_STATE_ENUM::kFollowThrough) {
                return result; // Prefer strafe/repositioning after attack
            }
            
            // Don't attack if stamina is critically low (conserve for emergencies)
            if (a_state.self.staminaPercent < 0.15f) {
                return result; // Too low stamina - prefer defensive actions
            }
            
            // Multiple enemies = less likely to commit to attacks (prefer defensive)
            float priorityModifier = 0.0f;
            if (a_state.combatContext.enemyCount > a_state.combatContext.allyCount + 1) {
                // Outnumbered: reduce offensive action priority significantly
                priorityModifier = -0.5f;
            } else if (a_state.combatContext.allyCount > a_state.combatContext.enemyCount) {
                // More allies: can be more aggressive
                priorityModifier = 0.2f;
            }
            
            // Health-based modifier: Lower health = more cautious
            float healthModifier = 0.0f;
            if (a_state.self.healthPercent < 0.4f) {
                healthModifier = -0.3f; // Low health - be more defensive
            }
            
            // Target state modifiers - only attack when there's a good opening
            float targetStateModifier = 0.0f;
            bool hasGoodOpening = false;
            
            // Excellent opportunities
            if (a_state.target.knockState != RE::KNOCK_STATE_ENUM::kNormal) {
                targetStateModifier += 0.4f; // Target staggered - perfect opportunity
                hasGoodOpening = true;
            }
            if (a_state.target.isCasting || a_state.target.isDrawingBow) {
                targetStateModifier += 0.3f; // Target casting/drawing - very vulnerable
                hasGoodOpening = true;
            }
            
            // Good opportunities
            if (a_state.target.isBlocking && !a_state.target.isAttacking) {
                targetStateModifier += 0.2f; // Target blocking (not attacking) - good opportunity
                hasGoodOpening = true;
            }
            
            // Orientation bonus: Higher priority if target is not facing us directly
            if (a_state.target.orientationDot < 0.5f) {
                targetStateModifier += 0.15f; // Target not facing us - good opportunity
                hasGoodOpening = true;
            }
            
            // If no good opening, significantly reduce priority (prefer strafe/repositioning)
            if (!hasGoodOpening) {
                priorityModifier -= 0.4f; // No clear opening - prefer tactical positioning
            }
            
            // Lower base priority to allow tactical actions (strafe) to compete
            float basePriority = 0.7f + priorityModifier + healthModifier + targetStateModifier;
            
            // Distance-based modifier: closer = slightly higher priority
            float distanceRatio = a_state.target.distance / reachDistance;
            float distanceModifier = (1.0f - distanceRatio) * 0.1f; // Up to +0.1f when closer
            
            if (a_state.self.staminaPercent > config.GetDecisionMatrix().powerAttackStaminaThreshold) {
                result.action = ActionType::PowerAttack;
                result.priority = basePriority + distanceModifier;
                // Power attack - committed attack, high intensity
                result.intensity = 0.8f; // High intensity for committed power attack
            } else {
                result.action = ActionType::Attack;
                result.priority = basePriority + distanceModifier;
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

        // Check if target is casting magic OR drawing bow OR attacking aggressively
        bool shouldBackoff = false;
        float urgencyModifier = 0.0f;
        
        // Primary triggers: Target casting or drawing bow (vulnerable to ranged attacks)
        if (a_state.target.isCasting || a_state.target.isDrawingBow) {
            shouldBackoff = true;
            urgencyModifier = 0.3f; // High urgency for ranged threats
        }
        
        // Secondary trigger: Target attacking aggressively (power attack or rapid attacks)
        // Only backoff if we're in close range and target is being very aggressive
        if (a_state.target.isPowerAttacking) {
            shouldBackoff = true;
            urgencyModifier = (std::max)(urgencyModifier, 0.4f); // Very high urgency for power attacks
        } else if (a_state.target.isAttacking && a_state.target.distance <= sprintAttackMinDist) {
            // Target is attacking and we're very close - backoff to create space
            shouldBackoff = true;
            urgencyModifier = (std::max)(urgencyModifier, 0.2f); // Moderate urgency for normal attacks in close range
        }
        
        // Additional consideration: Low health = more urgent backoff
        if (a_state.self.healthPercent < 0.4f) {
            urgencyModifier += 0.2f; // More urgent when low on health
        }

        if (shouldBackoff) {
            result.action = ActionType::Backoff;
            
            // Base priority (between Evasion and Survival)
            float basePriority = 1.8f;
            
            // Apply urgency modifier
            basePriority += urgencyModifier;
            
            // Multiple enemies = more urgent backoff
            if (a_state.combatContext.enemyCount > 1) {
                basePriority += 0.4f; // Higher priority when outnumbered
            }
            
            // Low stamina = more urgent backoff (conserve stamina)
            if (a_state.self.staminaPercent < 0.3f) {
                basePriority += 0.2f;
            }
            
            result.priority = basePriority;
            
            // Calculate backoff direction (away from target)
            RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
            toTarget.z = 0.0f; // Keep on horizontal plane
            toTarget.Unitize();
            result.direction = RE::NiPoint3(-toTarget.x, -toTarget.y, 0.0f); // Opposite direction
            
            // Set intensity based on distance and urgency - closer = faster backoff
            float distance = a_state.target.distance;
            float baseIntensity = 0.5f;
            if (distance <= sprintAttackMinDist) {
                baseIntensity = 1.0f; // Fast backoff if very close
            } else if (distance >= sprintAttackMinDist && distance <= sprintAttackMaxDist) {
                baseIntensity = 0.7f; // Medium backoff
            }
            
            // Increase intensity based on urgency
            result.intensity = (std::min)(1.0f, baseIntensity + urgencyModifier * 0.3f);
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

    DecisionResult DecisionMatrix::EvaluateFlanking(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        DecisionResult result;

        // Flanking only makes sense when:
        // 1. We have allies nearby
        // 2. Target is valid
        // 3. We're in melee range or close to it
        if (!a_state.target.isValid || !a_state.combatContext.hasNearbyAlly) {
            return result;
        }

        // Only consider flanking if we have at least one ally
        if (a_state.combatContext.allyCount < 1) {
            return result;
        }

        // Flanking is most effective when:
        // - Target is engaged with another actor (ally)
        // - We can position ourselves to the side/back of target
        // - We're in melee range or approaching it

        // Validate closestAllyPosition is valid (not zero/uninitialized)
        // Check if position is reasonable (not NaN, not zero, not extremely far)
        RE::NiPoint3 allyPos = a_state.combatContext.closestAllyPosition;
        float allyDistSq = allyPos.x * allyPos.x + allyPos.y * allyPos.y + allyPos.z * allyPos.z;
        if (allyDistSq < 1.0f || allyDistSq > 10000000.0f) { // Very close to zero or extremely far = invalid
            return result; // Invalid ally position
        }

        // Check if target is facing towards an ally (engaged with ally)
        RE::NiPoint3 targetToAlly = allyPos - a_state.target.position;
        float targetToAllyDistSq = targetToAlly.x * targetToAlly.x + targetToAlly.y * targetToAlly.y + targetToAlly.z * targetToAlly.z;
        if (targetToAllyDistSq < 0.01f) { // Too close, avoid division by zero
            return result;
        }
        float targetToAllyDist = std::sqrt(targetToAllyDistSq);
        targetToAlly.x /= targetToAllyDist;
        targetToAlly.y /= targetToAllyDist;
        targetToAlly.z /= targetToAllyDist;
        
        // Calculate angle between target's forward vector and direction to ally
        float targetAllyDot = a_state.target.forwardVector.Dot(targetToAlly);
        
        // If target is facing towards ally (dot > 0.5), we can flank from behind/side
        bool targetEngagedWithAlly = (targetAllyDot > 0.5f && targetToAllyDist < 800.0f);

        // Calculate our position relative to target
        RE::NiPoint3 targetToSelf = a_state.self.position - a_state.target.position;
        float targetToSelfDistSq = targetToSelf.x * targetToSelf.x + targetToSelf.y * targetToSelf.y + targetToSelf.z * targetToSelf.z;
        if (targetToSelfDistSq < 0.01f) { // Too close, avoid division by zero
            return result;
        }
        float targetToSelfDist = std::sqrt(targetToSelfDistSq);
        targetToSelf.x /= targetToSelfDist;
        targetToSelf.y /= targetToSelfDist;
        targetToSelf.z /= targetToSelfDist;
        float targetSelfDot = a_state.target.forwardVector.Dot(targetToSelf);

        // Optimal flanking position: to the side or behind target (dot < 0.3)
        // If we're already in good flanking position, we might want to attack instead
        bool isInFlankingPosition = (targetSelfDot < 0.3f);

        // Only suggest flanking if:
        // 1. Target is engaged with ally AND we're not in optimal flanking position yet
        // 2. OR we're close to melee range but not quite there
        if (targetEngagedWithAlly && !isInFlankingPosition && targetToSelfDist < 1000.0f) {
            result.action = ActionType::Flanking;
            result.priority = 1.4f; // Higher priority than normal strafe when flanking
            
            // Calculate optimal flanking direction (perpendicular to target, away from ally)
            // This creates a pincer movement
            RE::NiPoint3 flankingDir = CalculateFlankingDirection(a_state);
            result.direction = flankingDir;
            result.intensity = 1.0f; // Full intensity for tactical positioning
        }

        return result;
    }

    DecisionResult DecisionMatrix::EvaluateFeinting(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        DecisionResult result;

        // Feinting conditions:
        // 1. Target is blocking or being defensive
        // 2. We're in melee range
        // 3. Target is not currently attacking (safe to feint)
        // 4. We have stamina to perform feint + follow-up

        if (!a_state.target.isValid) {
            return result;
        }

        // Feinting is most effective when target is defensive
        bool targetIsDefensive = a_state.target.isBlocking || 
                                 (a_state.target.orientationDot > 0.7f && !a_state.target.isAttacking);

        // Need to be in melee range for feinting to be effective
        float reachDistance = a_state.weaponReach * 1.2f; // Slightly beyond reach
        bool inMeleeRange = (a_state.target.distance <= reachDistance);

        // Don't feint if target is actively attacking (too risky)
        if (a_state.target.isAttacking || a_state.target.isPowerAttacking) {
            return result;
        }

        // Need sufficient stamina for feint + potential follow-up
        if (a_state.self.staminaPercent < 0.4f) {
            return result;
        }

        // Feinting is effective when:
        // - Target is blocking/defensive
        // - We're in melee range
        // - We haven't attacked recently (to avoid spam)
        if (targetIsDefensive && inMeleeRange && 
            a_state.self.attackState == RE::ATTACK_STATE_ENUM::kNone) {
            
            result.action = ActionType::Feint;
            result.priority = 1.2f; // Moderate priority - useful but not urgent
            
            // Feint direction: slight forward movement to appear aggressive
            RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
            toTarget.Unitize();
            result.direction = toTarget;
            result.intensity = 0.6f; // Moderate intensity - not full commitment
        }

        return result;
    }

    RE::NiPoint3 DecisionMatrix::CalculateFlankingDirection(const ActorStateData& a_state)
    {
        if (!a_state.target.isValid || !a_state.combatContext.hasNearbyAlly) {
            // Fallback to normal strafe direction
            return CalculateStrafeDirection(a_state);
        }

        // Validate closestAllyPosition is valid (not zero/uninitialized)
        RE::NiPoint3 allyPos = a_state.combatContext.closestAllyPosition;
        float allyDistSq = allyPos.x * allyPos.x + allyPos.y * allyPos.y + allyPos.z * allyPos.z;
        if (allyDistSq < 1.0f || allyDistSq > 10000000.0f) { // Very close to zero or extremely far = invalid
            // Fallback to normal strafe direction
            return CalculateStrafeDirection(a_state);
        }

        // Calculate direction perpendicular to target's forward vector
        // Prefer direction away from ally to create pincer movement
        
        // Get target's right vector (perpendicular to forward)
        RE::NiPoint3 targetRight = RE::NiPoint3(
            -a_state.target.forwardVector.y,
            a_state.target.forwardVector.x,
            0.0f
        );
        float targetRightLenSq = targetRight.x * targetRight.x + targetRight.y * targetRight.y;
        if (targetRightLenSq < 0.01f) {
            // Fallback to normal strafe direction
            return CalculateStrafeDirection(a_state);
        }
        float targetRightLen = std::sqrt(targetRightLenSq);
        targetRight.x /= targetRightLen;
        targetRight.y /= targetRightLen;

        // Calculate direction from target to ally
        RE::NiPoint3 targetToAlly = allyPos - a_state.target.position;
        float targetToAllyLenSq = targetToAlly.x * targetToAlly.x + targetToAlly.y * targetToAlly.y + targetToAlly.z * targetToAlly.z;
        if (targetToAllyLenSq < 0.01f) {
            // Fallback to normal strafe direction
            return CalculateStrafeDirection(a_state);
        }
        float targetToAllyLen = std::sqrt(targetToAllyLenSq);
        targetToAlly.x /= targetToAllyLen;
        targetToAlly.y /= targetToAllyLen;
        targetToAlly.z /= targetToAllyLen;

        // Choose flanking direction opposite to ally (create pincer)
        // Dot product tells us which side ally is on
        float allySideDot = targetRight.Dot(targetToAlly);
        
        // If ally is on right side (positive dot), we go left (negative)
        // If ally is on left side (negative dot), we go right (positive)
        RE::NiPoint3 flankingDir = (allySideDot > 0.0f) ? -targetRight : targetRight;

        // Also add slight forward component to maintain pressure
        RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
        float toTargetLenSq = toTarget.x * toTarget.x + toTarget.y * toTarget.y + toTarget.z * toTarget.z;
        if (toTargetLenSq < 0.01f) {
            // Too close, just return flanking direction
            return flankingDir;
        }
        float toTargetLen = std::sqrt(toTargetLenSq);
        toTarget.x /= toTargetLen;
        toTarget.y /= toTargetLen;
        toTarget.z /= toTargetLen;
        
        flankingDir = flankingDir * 0.7f + toTarget * 0.3f;
        float flankingLenSq = flankingDir.x * flankingDir.x + flankingDir.y * flankingDir.y + flankingDir.z * flankingDir.z;
        if (flankingLenSq < 0.01f) {
            // Fallback to normal strafe direction
            return CalculateStrafeDirection(a_state);
        }
        float flankingLen = std::sqrt(flankingLenSq);
        flankingDir.x /= flankingLen;
        flankingDir.y /= flankingLen;
        flankingDir.z /= flankingLen;

        return flankingDir;
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
                thread_local static std::random_device rd;
                thread_local static std::mt19937 gen(rd());
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
        
        // Factor 4: Combat context (enemy/ally ratio)
        int enemyCount = a_state.combatContext.enemyCount;
        int allyCount = a_state.combatContext.allyCount;
        
        if (enemyCount > allyCount + 1) {
            // Outnumbered: prefer defensive actions
            if (a_decision.action == ActionType::Retreat || 
                a_decision.action == ActionType::Backoff || 
                a_decision.action == ActionType::Dodge ||
                a_decision.action == ActionType::Strafe ||
                a_decision.action == ActionType::Jump) {
                score += 3.0f;
            }
            // Outnumbered: reduce offensive action score
            if (a_decision.action == ActionType::Attack || 
                a_decision.action == ActionType::PowerAttack || 
                a_decision.action == ActionType::SprintAttack) {
                score -= 2.0f;
            }
        } else if (allyCount > enemyCount) {
            // More allies: prefer offensive actions
            if (a_decision.action == ActionType::Attack || 
                a_decision.action == ActionType::PowerAttack || 
                a_decision.action == ActionType::SprintAttack ||
                a_decision.action == ActionType::Bash) {
                score += 2.0f;
            }
        }
        
        // Factor 5: Action type priority (implicit ordering)
        // Higher enum values get slight preference (for variety)
        score += static_cast<float>(static_cast<int>(a_decision.action)) * 0.1f;
        
        return score;
    }
}
