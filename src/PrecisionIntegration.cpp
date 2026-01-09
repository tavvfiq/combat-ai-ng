#include "pch.h"
#include "PrecisionIntegration.h"
#include "Logger.h"
#include "PrecisionAPI.h"
#include <Windows.h>

namespace CombatAI
{
    bool PrecisionIntegration::Initialize()
    {
        // Request Precision API
        m_precisionAPI = PRECISION_API::RequestPluginAPI(PRECISION_API::InterfaceVersion::V4);
        
        if (m_precisionAPI) {
            LOG_INFO("Precision API initialized successfully");
            return true;
        } else {
            LOG_INFO("Precision not found, using fallback weapon reach calculation");
            return false;
        }
    }

    float PrecisionIntegration::GetWeaponReach(RE::Actor* a_actor)
    {
        if (!a_actor || !a_actor->Is3DLoaded()) {
            return 150.0f; // Default fallback
        }

        // Try Precision API first
        if (m_precisionAPI) {
            auto* precision = static_cast<PRECISION_API::IVPrecision4*>(m_precisionAPI);
            if (precision) {
                RE::ActorHandle actorHandle = a_actor->GetHandle();
                float reach = precision->GetAttackCollisionCapsuleLength(actorHandle, PRECISION_API::RequestedAttackCollisionType::Default);
                if (reach > 0.0f) {
                    return reach;
                }
            }
        }

        // Fallback to manual calculation
        return GetWeaponReachFallback(a_actor);
    }

    float PrecisionIntegration::GetWeaponReachFallback(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return 150.0f;
        }

        // Get equipped weapon
        RE::TESForm* weapon = a_actor->GetEquippedObject(false); // Right hand
        if (!weapon) {
            weapon = a_actor->GetEquippedObject(true); // Left hand
        }

        if (weapon) {
            RE::TESObjectWEAP* weap = weapon->As<RE::TESObjectWEAP>();
            if (weap) {
                // Use weapon reach stat (in game units, typically 100-200)
                float baseReach = weap->weaponData.reach;
                if (baseReach > 0.0f) {
                    return baseReach;
                }
            }
        }

        // Unarmed - get from race
        RE::TESRace* race = a_actor->GetRace();
        if (race) {
            float unarmedReach = race->data.unarmedReach;
            if (unarmedReach > 0.0f) {
                return unarmedReach;
            }
        }

        // Ultimate fallback
        return 150.0f;
    }
}
