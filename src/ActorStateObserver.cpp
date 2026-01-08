#include "pch.h"
#include "ActorStateObserver.h"
#include "PrecisionIntegration.h"
#include "Config.h"

namespace CombatAI
{
    ActorStateData ActorStateObserver::GatherState(RE::Actor* a_actor, float a_deltaTime)
    {
        ActorStateData data;
        data.deltaTime = a_deltaTime;

        if (!a_actor) {
            return data;
        }

        // Gather self state
        data.self = GatherSelfState(a_actor);

        // Get weapon reach
        data.weaponReach = GetWeaponReach(a_actor);

        // Gather target state
        auto& runtimeData = a_actor->GetActorRuntimeData();
        RE::CombatController* combatController = runtimeData.combatController;

        if (combatController) {
            RE::ActorHandle targetHandle = combatController->targetHandle;
            RE::NiPointer<RE::Actor> target = RE::Actor::LookupByHandle(targetHandle);

            if (target && target.get()) {
                data.target = GatherTargetState(a_actor, target.get());
            }
        }

        return data;
    }

    SelfState ActorStateObserver::GatherSelfState(RE::Actor* a_actor)
    {
        SelfState state;

        if (!a_actor) {
            return state;
        }

        // Stamina percentage
        state.staminaPercent = GetActorValuePercent(a_actor, RE::ActorValue::kStamina);

        // Health percentage
        state.healthPercent = GetActorValuePercent(a_actor, RE::ActorValue::kHealth);

        // Attack state
        RE::ActorState* actorState = a_actor->AsActorState();
        if (actorState) {
            state.attackState = actorState->GetAttackState();
            state.isIdle = (state.attackState == RE::ATTACK_STATE_ENUM::kNone);
        }

        // Blocking state
        state.isBlocking = a_actor->IsBlocking();

        // Position
        state.position = a_actor->GetPosition();

        // Forward vector
        state.forwardVector = StateHelpers::GetActorForwardVector(a_actor);

        return state;
    }

    TargetState ActorStateObserver::GatherTargetState(RE::Actor* a_self, RE::Actor* a_target)
    {
        TargetState state;

        if (!a_self || !a_target) {
            return state;
        }

        state.isValid = true;

        // Blocking state
        state.isBlocking = a_target->IsBlocking();

        // Attacking state
        state.isAttacking = a_target->IsAttacking();

        // Power attacking
        state.isPowerAttacking = IsPowerAttacking(a_target);

        // Casting
        state.isCasting = (a_target->WhoIsCasting() != 0);

        // Knock state (stagger/recoil)
        RE::ActorState* targetState = a_target->AsActorState();
        if (targetState) {
            state.knockState = targetState->GetKnockState();
        }

        // Position
        state.position = a_target->GetPosition();

        // Forward vector
        state.forwardVector = StateHelpers::GetActorForwardVector(a_target);

        // Distance
        RE::NiPoint3 selfPos = a_self->GetPosition();
        state.distance = StateHelpers::CalculateDistance(selfPos, state.position);

        // Orientation dot product (is target looking at me?)
        state.orientationDot = StateHelpers::CalculateOrientationDot(
            selfPos, state.position, state.forwardVector);

        return state;
    }

    float ActorStateObserver::GetActorValuePercent(RE::Actor* a_actor, RE::ActorValue a_value)
    {
        if (!a_actor) {
            return 0.0f;
        }

        float current = a_actor->GetActorValue(a_value);
        float max = a_actor->GetBaseActorValue(a_value);

        if (max <= 0.0f) {
            return 0.0f;
        }

        return std::clamp(current / max, 0.0f, 1.0f);
    }

    bool ActorStateObserver::IsPowerAttacking(RE::Actor* a_target)
    {
        if (!a_target) {
            return false;
        }

        // Proper power attack detection using AttackData from HighProcessData
        // Based on ProjectGapClose implementation
        auto currentProcess = a_target->GetActorRuntimeData().currentProcess;
        if (currentProcess) {
            auto highProcess = currentProcess->high;
            if (highProcess) {
                auto attackData = highProcess->attackData;
                if (attackData) {
                    auto flags = attackData->data.flags;
                    return flags.any(RE::AttackData::AttackFlag::kPowerAttack);
                }
            }
        }

        return false;
    }

    float ActorStateObserver::GetWeaponReach(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 150.0f; // Default reach in game units
        }

        // Use Precision integration if available and enabled
        auto& config = Config::GetInstance();
        if (config.GetModIntegrations().enablePrecisionIntegration) {
            return PrecisionIntegration::GetInstance().GetWeaponReach(a_actor);
        }
        
        // Fallback: use weapon stat or default
        auto weapon = a_actor->GetEquippedObject(false);
        if (weapon && weapon->IsWeapon()) {
            auto weaponForm = weapon->As<RE::TESObjectWEAP>();
            if (weaponForm) {
                return weaponForm->reach * 100.0f; // Convert to game units
            }
        }
        
        return 150.0f; // Default fallback
    }
}
