#include "pch.h"
#include "ActionExecutor.h"
#include "Config.h"

namespace CombatAI
{
    bool ActionExecutor::Execute(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        if (!a_actor || a_decision.action == ActionType::None) {
            return false;
        }

        bool success = false;

        switch (a_decision.action) {
        case ActionType::Bash:
            success = ExecuteBash(a_actor);
            break;

        case ActionType::Strafe:
            // Strafe now uses dodge for evasion
            success = ExecuteDodge(a_actor, a_state);
            break;

        case ActionType::Retreat:
            success = ExecuteRetreat(a_actor, a_decision.direction, a_decision.intensity);
            break;

        default:
            return false;
        }

        return success;
    }

    bool ActionExecutor::ExecuteBash(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        // Check if already bashing/attacking (don't interrupt)
        RE::ActorState* state = a_actor->AsActorState();
        if (state) {
            RE::ATTACK_STATE_ENUM attackState = state->GetAttackState();
            if (attackState != RE::ATTACK_STATE_ENUM::kNone && 
                attackState != RE::ATTACK_STATE_ENUM::kDraw) {
                // Already in an attack, don't interrupt
                return false;
            }
        }

        // Reset BFCO attack state if BFCO is installed
        if (IsBFCOInstalled()) {
            ResetBFCOAttackState(a_actor);
        }

        // Trigger bash animation
        return NotifyAnimation(a_actor, "bashStart");
    }

    bool ActionExecutor::ExecuteDodge(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        if (!a_actor) {
            return false;
        }

        // Load dodge system config
        auto& config = Config::GetInstance();
        DodgeSystem::Config dodgeConfig;
        dodgeConfig.dodgeStaminaCost = config.GetDodgeSystem().dodgeStaminaCost;
        dodgeConfig.iFrameDuration = config.GetDodgeSystem().iFrameDuration;
        dodgeConfig.enableStepDodge = config.GetDodgeSystem().enableStepDodge;
        dodgeConfig.enableDodgeAttackCancel = config.GetDodgeSystem().enableDodgeAttackCancel;
        m_dodgeSystem.SetConfig(dodgeConfig);

        // Use DodgeSystem to execute evasion dodge
        return m_dodgeSystem.ExecuteEvasionDodge(a_actor, a_state);
    }

    bool ActionExecutor::ExecuteStrafe(RE::Actor* a_actor, const RE::NiPoint3& a_direction, float a_intensity)
    {
        if (!a_actor) {
            return false;
        }

        // Try CPR circling if available
        if (IsCPRAvailable(a_actor)) {
            // Calculate circling parameters based on distance and direction
            // For now, use default values - could be enhanced with actual distance calculations
            float minDist = 90.0f;
            float maxDist = 1200.0f;
            float minAngle = 45.0f;
            float maxAngle = 135.0f;
            
            SetCPRCircling(a_actor, minDist, maxDist, minAngle, maxAngle);
            return true;
        }

        // Fallback to direct movement control
        return SetMovementDirection(a_actor, a_direction, a_intensity);
    }

    bool ActionExecutor::ExecuteRetreat(RE::Actor* a_actor, const RE::NiPoint3& a_direction, float a_intensity)
    {
        if (!a_actor) {
            return false;
        }

        // Try CPR fallback if available
        if (IsCPRAvailable(a_actor)) {
            // Calculate fallback parameters
            float minDist = 200.0f;
            float maxDist = 500.0f;
            float minWait = 1.5f;
            float maxWait = 3.0f;
            
            SetCPRFallback(a_actor, minDist, maxDist, minWait, maxWait);
            return true;
        }

        // Fallback to direct movement control
        return SetMovementDirection(a_actor, a_direction, a_intensity);
    }

    bool ActionExecutor::ExecuteSprintAttack(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        // Start sprinting if not already
        if (!a_actor->AsActorState()->IsSprinting()) {
            NotifyAnimation(a_actor, "SprintStart");
            a_actor->AsActorState()->actorState1.sprinting = 1;
        }

        // Reset BFCO attack state if BFCO is installed
        if (IsBFCOInstalled()) {
            ResetBFCOAttackState(a_actor);
        }

        // Trigger sprint attack
        // BFCO supports attackStartSprint, vanilla also supports it
        NotifyAnimation(a_actor, "attackStartSprint");
        NotifyAnimation(a_actor, "attackStart");

        return true;
    }


    bool ActionExecutor::NotifyAnimation(RE::Actor* a_actor, const char* a_eventName)
    {
        if (!a_actor) {
            return false;
        }

        // Actor implements IAnimationGraphManagerHolder
        RE::BSFixedString eventName(a_eventName);
        return a_actor->NotifyAnimationGraph(eventName);
    }

    bool ActionExecutor::SetMovementDirection(RE::Actor* a_actor, const RE::NiPoint3& a_direction, float a_intensity)
    {
        if (!a_actor) {
            return false;
        }

        // Movement control using animation graph variables (inspired by ProjectGapClose)
        // ProjectGapClose uses variables like CPR_EnableCircling, CPR_CirclingAngleMin, etc.
        // For our purposes, we'll use a simpler approach with available graph variables
        
        RE::NiPoint3 normalizedDir = a_direction;
        normalizedDir.Unitize();

        // Scale by intensity
        normalizedDir.x *= a_intensity;
        normalizedDir.y *= a_intensity;

        // Note: Direct movement control via animation graph variables is limited
        // The animation graph needs to support these variables
        // For strafe/retreat, we rely on dodge system or let vanilla AI handle it
        
        // For now, we can try to influence movement through available means
        // This is a simplified approach - full movement control would require
        // reverse engineering MovementControllerNPC or using animation graph callbacks
        
        return true; // Placeholder - movement is handled by dodge system for evasion
    }

    bool ActionExecutor::IsBFCOInstalled() const
    {
        // Check config first
        auto& config = Config::GetInstance();
        if (!config.GetModIntegrations().enableBFCOIntegration) {
            return false;
        }

        // Check if BFCO plugin is loaded
        auto dataHandler = RE::TESDataHandler::GetSingleton();
        if (!dataHandler) {
            return false;
        }
        
        auto bfcoPlugin = dataHandler->LookupModByName("BFCO.esp");
        return bfcoPlugin != nullptr;
    }

    bool ActionExecutor::IsCPRAvailable(RE::Actor* a_actor) const
    {
        if (!a_actor) {
            return false;
        }

        // Check config first
        auto& config = Config::GetInstance();
        if (!config.GetModIntegrations().enableCPRIntegration) {
            return false;
        }

        // Check if CPR graph variables are available by trying to read one
        // If the variable exists, CPR is likely available
        bool enableCircling = false;
        bool hasVariable = a_actor->GetGraphVariableBool("CPR_EnableCircling", enableCircling);
        
        // If we can read the variable, CPR is available
        return hasVariable;
    }

    void ActionExecutor::ResetBFCOAttackState(RE::Actor* a_actor)
    {
        if (!a_actor || !IsBFCOInstalled()) {
            return;
        }

        // Reset BFCO attack state variables
        a_actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
        a_actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
        a_actor->SetGraphVariableInt("BFCONG_PARMB", 0);
        a_actor->SetGraphVariableInt("NEW_BFCO_DisablePALMB", 0);
    }

    void ActionExecutor::SetCPRCircling(RE::Actor* a_actor, float a_minDist, float a_maxDist, float a_minAngle, float a_maxAngle)
    {
        if (!a_actor) {
            return;
        }

        // Enable CPR circling
        a_actor->SetGraphVariableBool("CPR_EnableCircling", true);
        a_actor->SetGraphVariableFloat("CPR_CirclingDistMin", a_minDist);
        a_actor->SetGraphVariableFloat("CPR_CirclingDistMax", a_maxDist);
        a_actor->SetGraphVariableFloat("CPR_CirclingAngleMin", a_minAngle);
        a_actor->SetGraphVariableFloat("CPR_CirclingAngleMax", a_maxAngle);
        a_actor->SetGraphVariableFloat("CPR_CirclingViewConeAngle", 360.0f); // Full view cone
    }

    void ActionExecutor::SetCPRFallback(RE::Actor* a_actor, float a_minDist, float a_maxDist, float a_minWait, float a_maxWait)
    {
        if (!a_actor) {
            return;
        }

        // Enable CPR fallback
        a_actor->SetGraphVariableBool("CPR_EnableFallback", true);
        a_actor->SetGraphVariableFloat("CPR_FallbackDistMin", a_minDist);
        a_actor->SetGraphVariableFloat("CPR_FallbackDistMax", a_maxDist);
        a_actor->SetGraphVariableFloat("CPR_FallbackWaitTimeMin", a_minWait);
        a_actor->SetGraphVariableFloat("CPR_FallbackWaitTimeMax", a_maxWait);
    }

    void ActionExecutor::DisableCPR(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }

        // Disable all CPR behaviors
        a_actor->SetGraphVariableBool("CPR_EnableCircling", false);
        a_actor->SetGraphVariableBool("CPR_EnableAdvanceRadius", false);
        a_actor->SetGraphVariableBool("CPR_EnableBackoff", false);
        a_actor->SetGraphVariableBool("CPR_EnableFallback", false);
    }
}
