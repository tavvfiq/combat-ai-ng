#include "pch.h"
#include "DodgeSystem.h"
#include "Config.h"
#include <cmath>

namespace CombatAI
{
    namespace
    {
        constexpr float PI = 3.14159265f;
        constexpr float PI8 = 0.39269908f; // PI / 8
    }

    bool DodgeSystem::CanDodge(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        // Check config first
        auto& config = CombatAI::Config::GetInstance();
        if (!config.GetModIntegrations().enableTKDodgeIntegration) {
            return false;
        }

        // Check if already dodging
        if (IsDodging(a_actor)) {
            return false;
        }

        // Check attack state (can cancel if enabled)
        RE::ActorState* state = a_actor->AsActorState();
        if (state) {
            RE::ATTACK_STATE_ENUM attackState = state->GetAttackState();
            if (attackState != RE::ATTACK_STATE_ENUM::kNone && !m_config.enableDodgeAttackCancel) {
                return false;
            }
        }

        // Check life state
        if (state) {
            if (state->GetSitSleepState() != RE::SIT_SLEEP_STATE::kNormal ||
                state->GetKnockState() != RE::KNOCK_STATE_ENUM::kNormal ||
                state->GetFlyState() != RE::FLY_STATE::kNone) {
                return false;
            }
        }

        // Check if swimming
        if (state && state->IsSwimming()) {
            return false;
        }

        // Check if in kill move
        if (a_actor->IsInKillMove()) {
            return false;
        }

        // Check if jumping (via animation graph)
        bool isJumping = false;
        a_actor->GetGraphVariableBool(RE::BSFixedString("bInJumpState"), isJumping);
        if (isJumping) {
            return false;
        }

        // Check stamina
        auto actorValueOwner = a_actor->AsActorValueOwner();
        float currentStamina = actorValueOwner->GetActorValue(RE::ActorValue::kStamina);
        if (currentStamina < m_config.dodgeStaminaCost) {
            return false;
        }

        return true;
    }

    bool DodgeSystem::ExecuteDodge(RE::Actor* a_actor, const RE::NiPoint3& a_dodgeDirection)
    {
        if (!a_actor || !CanDodge(a_actor)) {
            return false;
        }

        // Determine dodge direction string
        std::string dodgeEvent = DetermineDodgeDirection(a_actor, a_dodgeDirection);

        // Set step dodge variable if enabled
        if (m_config.enableStepDodge) {
            a_actor->SetGraphVariableInt(RE::BSFixedString("iStep"), 2);
        }

        // Set i-frame duration
        a_actor->SetGraphVariableFloat(RE::BSFixedString("TKDR_IframeDuration"), m_config.iFrameDuration);

        // Trigger dodge animation
        bool success = a_actor->NotifyAnimationGraph(RE::BSFixedString(dodgeEvent.c_str()));

        return success;
    }

    bool DodgeSystem::ExecuteEvasionDodge(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        if (!a_actor || !a_state.target.isValid) {
            return false;
        }

        // Calculate dodge direction (perpendicular to target, away from danger)
        // For evasion, we want to dodge perpendicular to the line between self and target
        RE::NiPoint3 toTarget = a_state.target.position - a_state.self.position;
        toTarget.z = 0.0f; // Keep on horizontal plane
        toTarget.Unitize();

        // Calculate perpendicular direction (90 degrees)
        // Choose the direction that moves away from target's forward vector (if target is attacking)
        RE::NiPoint3 dodgeDir;
        
        if (a_state.target.isAttacking || a_state.target.isPowerAttacking) {
            // If target is attacking, dodge perpendicular to their attack direction
            // Use target's forward vector to determine best dodge direction
            RE::NiPoint3 targetForward = a_state.target.forwardVector;
            targetForward.z = 0.0f;
            targetForward.Unitize();

            // Perpendicular: rotate 90 degrees
            dodgeDir = RE::NiPoint3(-targetForward.y, targetForward.x, 0.0f);
        } else {
            // If target is just blocking, dodge perpendicular to the line between us
            dodgeDir = RE::NiPoint3(-toTarget.y, toTarget.x, 0.0f);
        }

        dodgeDir.Unitize();

        return ExecuteDodge(a_actor, dodgeDir);
    }

    std::string DodgeSystem::DetermineDodgeDirection(RE::Actor* a_actor, const RE::NiPoint3& a_dodgeDirection)
    {
        if (!a_actor) {
            return "TKDodgeBack";
        }

        // Get actor's forward vector
        RE::NiPoint3 actorForward = StateHelpers::GetActorForwardVector(a_actor);
        
        // Normalize dodge direction
        RE::NiPoint3 normalizedDodge = a_dodgeDirection;
        normalizedDodge.z = 0.0f;
        normalizedDodge.Unitize();

        // Calculate angle between actor forward and dodge direction
        // Using 2D vectors for angle calculation
        RE::NiPoint2 forward2D(actorForward.x, actorForward.y);
        RE::NiPoint2 dodge2D(normalizedDodge.x, normalizedDodge.y);

        // Calculate angle using atan2
        float forwardAngle = std::atan2(forward2D.y, forward2D.x);
        float dodgeAngle = std::atan2(dodge2D.y, dodge2D.x);
        float angleDiff = dodgeAngle - forwardAngle;

        // Normalize angle to [-PI, PI]
        while (angleDiff > PI) angleDiff -= 2.0f * PI;
        while (angleDiff < -PI) angleDiff += 2.0f * PI;

        // Determine dodge direction based on angle
        // Forward: -PI/4 to PI/4
        // Right: PI/4 to 3*PI/4
        // Back: 3*PI/4 to -3*PI/4 (or PI to -PI)
        // Left: -3*PI/4 to -PI/4

        if (angleDiff >= -2.0f * PI8 && angleDiff < 2.0f * PI8) {
            return "TKDodgeForward";
        } else if (angleDiff >= 2.0f * PI8 && angleDiff < 6.0f * PI8) {
            return "TKDodgeRight";
        } else if (angleDiff >= -6.0f * PI8 && angleDiff < -2.0f * PI8) {
            return "TKDodgeLeft";
        } else {
            return "TKDodgeBack";
        }
    }

    bool DodgeSystem::IsDodging(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        bool isDodging = false;
        a_actor->GetGraphVariableBool(RE::BSFixedString("bIsDodging"), isDodging);
        return isDodging;
    }
}
