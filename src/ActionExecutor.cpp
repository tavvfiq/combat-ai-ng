#include "pch.h"
#include "ActionExecutor.h"
#include "Config.h"

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace CombatAI
{
    bool cpr_integrated_debug_shown = false;
    bool bfco_integrated_debug_shown = false;

    bool ActionExecutor::Execute(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        if (!a_actor || a_decision.action == ActionType::None) {
            return false;
        }

        // Reset jump variable if executing a different action
        // This prevents the jump variable from staying true when we want to do something else
        if (a_decision.action != ActionType::Jump) {
            ResetJumpVariable(a_actor);
        }

        bool success = false;

        switch (a_decision.action) {
        case ActionType::Attack:
            success = ExecuteAttack(a_actor);
            break;
            
        case ActionType::PowerAttack:
            success = ExecutePowerAttack(a_actor);
            break;

        case ActionType::SprintAttack:
            success = ExecuteSprintAttack(a_actor);
            break;

        case ActionType::Bash:
            success = ExecuteBash(a_actor);
            break;

        case ActionType::Strafe:
            success = ExecuteStrafe(a_actor, a_decision, a_state);
            break;

        case ActionType::Retreat:
            success = ExecuteRetreat(a_actor, a_decision, a_state);
            break;

        case ActionType::Jump:
            success = ExecuteJump(a_actor, a_state);
            break;

        case ActionType::Dodge:
            success = ExecuteDodge(a_actor, a_state);
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

    bool ActionExecutor::ExecuteStrafe(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        if (!a_actor) {
            return false;
        }

        // Try CPR circling if available
        if (IsCPRAvailable(a_actor)) {
            // Calculate circling parameters based on distance and direction
            float minDist = a_state.target.distance * 0.8f;
            float maxDist = a_state.target.distance * 1.2f;
            float minAngle = 45.0f;
            float maxAngle = 135.0f;
            
            SetCPRCircling(a_actor, minDist, maxDist, minAngle, maxAngle);
            return true;
        }

        // Fallback to direct movement control
        return SetMovementDirection(a_actor, a_decision.direction, a_decision.intensity);
    }

    bool ActionExecutor::ExecuteRetreat(RE::Actor* a_actor, const DecisionResult& a_decision, const ActorStateData& a_state)
    {
        if (!a_actor) {
            return false;
        }

        // Try CPR fallback if available
        if (IsCPRAvailable(a_actor)) {
            // Calculate fallback parameters
            float retreatDistance = a_state.target.isValid ? a_state.target.distance : 500.0f; // Default if no valid target
            float minDist = retreatDistance;
            float maxDist = retreatDistance * 1.5f;
            float minWait = 1.5f; // TODO: make configurable
            float maxWait = 3.0f; // TODO: make configurable
            
            SetCPRFallback(a_actor, minDist, maxDist, minWait, maxWait);
            return true;
        }

        // Fallback to direct movement control
        return SetMovementDirection(a_actor, a_decision.direction, a_decision.intensity);
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
        if (!a_actor || !a_actor->Get3D()) {
            return false;
        }

        RE::NiPoint3 worldDir = a_direction;
        worldDir.Unitize();

        RE::NiAVObject* actor3d = a_actor->Get3D();
        if (!actor3d) {
            return false;
        }

        // Get actor's rotation matrix (transpose is the inverse for rotation matrices)
        RE::NiMatrix3 invActorRot = actor3d->world.rotate.Transpose();

        // Transform world direction to local direction
        RE::NiPoint3 localDir = invActorRot * worldDir;
        localDir.Unitize();

        // Angle of movement in radians. atan2(x, y) because y is forward in Skyrim's local space.
        float angle = atan2(localDir.x, localDir.y);
        float speed = std::min(a_intensity, 1.0f);

        // Set standard movement graph variables.
        // The animation graph must support these for the movement to work.
        a_actor->SetGraphVariableFloat("movementDirection", angle);
        a_actor->SetGraphVariableFloat("movementSpeed", speed);
        
        // Also set InputDirection and InputMagnitude for compatibility with other animation setups (e.g. DAR)
        a_actor->SetGraphVariableFloat("InputDirection", angle);
        a_actor->SetGraphVariableFloat("InputMagnitude", speed);

        return true;
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
        
        auto bfcoPlugin = dataHandler->LookupModByName("BFCO NG.esp");

        if (!bfco_integrated_debug_shown) {
            if (bfcoPlugin) {
                LOG_INFO("BFCO plugin found");
            } else {
                LOG_INFO("BFCO plugin not found. will use vanilla combat");
            }
            bfco_integrated_debug_shown = true;
        }

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
        if (!cpr_integrated_debug_shown) {
            if (hasVariable) {
                LOG_DEBUG("CPR graph variables available");
            } else {
                LOG_DEBUG("CPR graph variables not available");
            }
            cpr_integrated_debug_shown = true;
        }
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

    bool ActionExecutor::ExecuteAttack(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        // Check if already attacking
        RE::ActorState* state = a_actor->AsActorState();
        if (state) {
            RE::ATTACK_STATE_ENUM attackState = state->GetAttackState();
            if (attackState != RE::ATTACK_STATE_ENUM::kNone && 
                attackState != RE::ATTACK_STATE_ENUM::kDraw) {
                return false;
            }
        }

        if (IsBFCOInstalled()) {
            ResetBFCOAttackState(a_actor);
            a_actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
        }

        return NotifyAnimation(a_actor, "attackStart");
    }

    bool ActionExecutor::ExecutePowerAttack(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        // Check if already attacking
        RE::ActorState* state = a_actor->AsActorState();
        if (state) {
            RE::ATTACK_STATE_ENUM attackState = state->GetAttackState();
            if (attackState != RE::ATTACK_STATE_ENUM::kNone && 
                attackState != RE::ATTACK_STATE_ENUM::kDraw) {
                return false;
            }
        }

        if (IsBFCOInstalled()) {
            ResetBFCOAttackState(a_actor);
            a_actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
            return NotifyAnimation(a_actor, "attackStart");
        }

        // Vanilla power attack
        return NotifyAnimation(a_actor, "powerAttack");
    }

    bool ActionExecutor::ExecuteJump(RE::Actor* a_actor, const ActorStateData& a_state)
    {
        if (!a_actor) {
            return false;
        }
        
        // Check if already dodging/jumping - don't trigger another dodge
        // We check bInJumpState to prevent spam, but we also check if already dodging
        bool isJumping = false;
        a_actor->GetGraphVariableBool("CombatAI_NG_Jump", isJumping);
        if (isJumping) {
            return false; // Already in jump state, don't trigger again
        }
        
        // Set CombatAI_NG_Jump to true BEFORE executing dodge
        // This allows OAR to detect it and replace the dodge animation with a jump animation
        a_actor->SetGraphVariableBool("CombatAI_NG_Jump", true);
        
        return ExecuteDodge(a_actor, a_state);
    }

    void ActionExecutor::ResetJumpVariable(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }
        
        a_actor->SetGraphVariableBool("CombatAI_NG_Jump", false);
    }
}
