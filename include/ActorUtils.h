#pragma once

#include "RE/A/Actor.h"
#include "RE/B/BSTSmartPointer.h"
#include <optional>

namespace CombatAI
{
    // Utility functions for safe actor access with built-in error handling
    // These functions wrap actor property access in try-catch to prevent crashes from stale actors
    
    namespace ActorUtils
    {
        // Check if actor is valid and accessible
        inline bool IsValid(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return false;
            }
            try {
                return !a_actor->IsDead() && a_actor->IsInCombat();
            } catch (...) {
                return false;
            }
        }

        // Safe GetFormID - returns optional to handle failures
        inline std::optional<RE::FormID> SafeGetFormID(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return std::nullopt;
            }
            try {
                return a_actor->GetFormID();
            } catch (...) {
                return std::nullopt;
            }
        }

        // Safe GetEquippedObject
        inline RE::TESForm* SafeGetEquippedObject(RE::Actor* a_actor, bool a_leftHand)
        {
            if (!a_actor) {
                return nullptr;
            }
            try {
                return a_actor->GetEquippedObject(a_leftHand);
            } catch (...) {
                return nullptr;
            }
        }

        // Safe AsActorState
        inline RE::ActorState* SafeAsActorState(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return nullptr;
            }
            try {
                return a_actor->AsActorState();
            } catch (...) {
                return nullptr;
            }
        }

        // Safe AsActorValueOwner
        inline RE::ActorValueOwner* SafeAsActorValueOwner(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return nullptr;
            }
            try {
                return a_actor->AsActorValueOwner();
            } catch (...) {
                return nullptr;
            }
        }


        // Safe GetActorBase
        inline RE::TESNPC* SafeGetActorBase(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return nullptr;
            }
            try {
                return a_actor->GetActorBase();
            } catch (...) {
                return nullptr;
            }
        }

        // Safe Get3D
        inline RE::NiAVObject* SafeGet3D(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return nullptr;
            }
            try {
                return a_actor->Get3D();
            } catch (...) {
                return nullptr;
            }
        }

        // Safe WhoIsCasting
        inline std::uint32_t SafeWhoIsCasting(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return 0;
            }
            try {
                return a_actor->WhoIsCasting();
            } catch (...) {
                return 0;
            }
        }

        // Safe SetGraphVariableBool
        inline bool SafeSetGraphVariableBool(RE::Actor* a_actor, const char* a_varName, bool a_value)
        {
            if (!a_actor) {
                return false;
            }
            try {
                return a_actor->SetGraphVariableBool(a_varName, a_value);
            } catch (...) {
                return false;
            }
        }

        // Safe SetGraphVariableFloat
        inline bool SafeSetGraphVariableFloat(RE::Actor* a_actor, const char* a_varName, float a_value)
        {
            if (!a_actor) {
                return false;
            }
            try {
                return a_actor->SetGraphVariableFloat(a_varName, a_value);
            } catch (...) {
                return false;
            }
        }

        // Safe SetGraphVariableInt
        inline bool SafeSetGraphVariableInt(RE::Actor* a_actor, const char* a_varName, int32_t a_value)
        {
            if (!a_actor) {
                return false;
            }
            try {
                return a_actor->SetGraphVariableInt(a_varName, a_value);
            } catch (...) {
                return false;
            }
        }

        // Safe GetGraphVariableBool
        inline bool SafeGetGraphVariableBool(RE::Actor* a_actor, const char* a_varName, bool& a_outValue)
        {
            if (!a_actor) {
                return false;
            }
            try {
                return a_actor->GetGraphVariableBool(a_varName, a_outValue);
            } catch (...) {
                return false;
            }
        }

        // Safe NotifyAnimationGraph
        inline bool SafeNotifyAnimationGraph(RE::Actor* a_actor, const char* a_eventName)
        {
            if (!a_actor) {
                return false;
            }
            try {
                RE::BSFixedString eventName(a_eventName);
                return a_actor->NotifyAnimationGraph(eventName);
            } catch (...) {
                return false;
            }
        }

        // Safe GetAttackState
        inline RE::ATTACK_STATE_ENUM SafeGetAttackState(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return RE::ATTACK_STATE_ENUM::kNone;
            }
            try {
                RE::ActorState* state = a_actor->AsActorState();
                if (state) {
                    return state->GetAttackState();
                }
            } catch (...) {
                // Actor access failed
            }
            return RE::ATTACK_STATE_ENUM::kNone;
        }

        // Safe IsSprinting
        inline bool SafeIsSprinting(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return false;
            }
            try {
                RE::ActorState* state = a_actor->AsActorState();
                if (state) {
                    return state->IsSprinting();
                }
            } catch (...) {
                // Actor access failed
            }
            return false;
        }

        // Safe SetSprinting
        inline void SafeSetSprinting(RE::Actor* a_actor, bool a_sprinting)
        {
            if (!a_actor) {
                return;
            }
            try {
                RE::ActorState* state = a_actor->AsActorState();
                if (state) {
                    state->actorState1.sprinting = a_sprinting ? 1 : 0;
                }
            } catch (...) {
                // Actor access failed
            }
        }
    }
}
