#include "pch.h"
#include "DecisionMatrix.h"
#include "Config.h"
#include <cmath>

namespace CombatAI
{
    DecisionResult DecisionMatrix::Evaluate(const ActorStateData& a_state)
    {
        DecisionResult result;

        // Priority 1: Survival (highest priority - must check first)
        result = EvaluateSurvival(a_state);
        if (result.action != ActionType::None) {
            return result;
        }

        // Priority 2: Interrupt (counter-play)
        result = EvaluateInterrupt(a_state);
        if (result.action != ActionType::None) {
            return result;
        }

        // Priority 3: Evasion (tactical)
        result = EvaluateEvasion(a_state);
        if (result.action != ActionType::None) {
            return result;
        }

        return result; // No action
    }

    DecisionResult DecisionMatrix::EvaluateInterrupt(const ActorStateData& a_state)
    {
        DecisionResult result;

        // Check if target is valid
        if (!a_state.target.isValid) {
            return result;
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

        // Trigger: Target is power attacking + Distance < Reach
        if (a_state.target.isPowerAttacking && a_state.target.distance < reachDistance) {
            result.action = ActionType::Bash;
            result.priority = 1;
        }

        return result;
    }

    DecisionResult DecisionMatrix::EvaluateEvasion(const ActorStateData& a_state)
    {
        DecisionResult result;

        // Check if evasion dodge is enabled
        auto& config = Config::GetInstance();
        if (!config.GetDecisionMatrix().enableEvasionDodge) {
            return result;
        }

        // Check if target is valid
        if (!a_state.target.isValid) {
            return result;
        }

        // Trigger: Target is blocking + Self is idle
        if (a_state.target.isBlocking && a_state.self.isIdle) {
            result.action = ActionType::Strafe;
            result.priority = 2;
            result.direction = CalculateStrafeDirection(a_state);
            result.intensity = 0.7f; // Moderate strafe speed
        }

        return result;
    }

    DecisionResult DecisionMatrix::EvaluateSurvival(const ActorStateData& a_state)
    {
        DecisionResult result;

        // Check if survival retreat is enabled
        auto& config = Config::GetInstance();
        if (!config.GetDecisionMatrix().enableSurvivalRetreat) {
            return result;
        }

        // Get thresholds from config
        const float staminaThreshold = config.GetDecisionMatrix().staminaThreshold;
        const float healthThreshold = config.GetDecisionMatrix().healthThreshold;

        // Trigger: Stamina < threshold OR Health < threshold
        if (a_state.self.staminaPercent < staminaThreshold || 
            a_state.self.healthPercent < healthThreshold) {
            result.action = ActionType::Retreat;
            result.priority = 3; // Highest priority

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
