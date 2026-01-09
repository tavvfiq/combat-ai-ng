#pragma once

#include "DecisionResult.h"
#include "ActorStateData.h"
#include "DodgeSystem.h"
#include "RE/A/Actor.h"

namespace CombatAI
{
    // Executes actions (animations, movement)
    class ActionExecutor
    {
    public:
        ActionExecutor() = default;
        ~ActionExecutor() = default;

        // Execute a decision result
        bool Execute(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);

        void EnableBFCO(bool isEnabled);

    private:
        // Execute bash action
        bool ExecuteBash(RE::Actor* a_actor);

        // Execute dodge (TK Dodge integration)
        bool ExecuteDodge(RE::Actor* a_actor, const ActorStateData& a_state);

        // Execute strafe movement (with CPR support)
        bool ExecuteStrafe(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);

        // Execute retreat movement (with CPR support)
        bool ExecuteRetreat(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);

        // Execute attack
        bool ExecuteAttack(RE::Actor* a_actor);

        // Execute power attack
        bool ExecutePowerAttack(RE::Actor* a_actor);

        // Execute sprint attack (BFCO support)
        bool ExecuteSprintAttack(RE::Actor* a_actor);

        // Execute jump (actually executes dodge with CombatAI_NG_Jump=true for OAR animation replacement)
        bool ExecuteJump(RE::Actor* a_actor, const ActorStateData& a_state);

        // Execute backoff movement (away from target when they're casting/drawing)
        bool ExecuteBackoff(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);

        // Execute advancing movement (closing distance when too far)
        bool ExecuteAdvancing(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state);

        // Helper: Trigger animation graph event
        bool NotifyAnimation(RE::Actor* a_actor, const char* a_eventName);

        // Helper: Set movement via animation graph (if possible)
        bool SetMovementDirection(RE::Actor* a_actor, const RE::NiPoint3& a_direction, float a_intensity);

        // Helper: Check if CPR is available (check for graph variable support)
        bool IsCPRAvailable(RE::Actor* a_actor) const;

        // Helper: Reset BFCO attack state
        void ResetBFCOAttackState(RE::Actor* a_actor);

        // Helper: Set CPR circling parameters
        void SetCPRCircling(RE::Actor* a_actor, float a_minDist, float a_maxDist, float a_minAngle, float a_maxAngle);

        // Helper: Set CPR fallback parameters
        void SetCPRFallback(RE::Actor* a_actor, float a_minDist, float a_maxDist, float a_minWait, float a_maxWait);

        // Helper: Set CPR backoff parameters
        void SetCPRBackoff(RE::Actor* a_actor, float a_minDistMult);

        // Helper: Set CPR advancing parameters
        void SetCPRAdvancing(RE::Actor* a_actor, float a_innerRadiusMin, float a_innerRadiusMid, float a_innerRadiusMax, float a_outerRadiusMin, float a_outerRadiusMid, float a_outerRadiusMax);

        // Helper: Disable all CPR behaviors
        void DisableCPR(RE::Actor* a_actor);

        // Helper: Reset jump variable (when actor lands or when executing other actions)
        void ResetJumpVariable(RE::Actor* a_actor);

        // Dodge system
        DodgeSystem m_dodgeSystem;
        bool m_isBFCOEnabled = false;
    };
}
