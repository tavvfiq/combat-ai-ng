#include "pch.h"
#include "PrecisionIntegration.h"
#include "Logger.h"
#include <Windows.h>

// Include Precision API header (copy from Precision project)
// For now, we'll use the API directly
namespace PRECISION_API
{
    constexpr const auto PrecisionPluginName = "Precision";
    
    enum class InterfaceVersion : uint8_t
    {
        V1, V2, V3, V4
    };
    
    enum class RequestedAttackCollisionType : uint8_t
    {
        Default,
        Current,
        RightWeapon,
        LeftWeapon
    };
    
    class IVPrecision1
    {
    public:
        virtual float GetAttackCollisionCapsuleLength(RE::ActorHandle a_actorHandle, RequestedAttackCollisionType a_collisionType = RequestedAttackCollisionType::Default) const noexcept = 0;
    };
    
    typedef void* (*_RequestPluginAPI)(const InterfaceVersion interfaceVersion);
    
    [[nodiscard]] inline void* RequestPluginAPI(const InterfaceVersion a_interfaceVersion = InterfaceVersion::V4)
    {
        auto pluginHandle = GetModuleHandleA("Precision.dll");
        if (!pluginHandle) {
            return nullptr;
        }
        _RequestPluginAPI requestAPIFunction = (_RequestPluginAPI)GetProcAddress(pluginHandle, "RequestPluginAPI");
        if (requestAPIFunction) {
            return requestAPIFunction(a_interfaceVersion);
        }
        return nullptr;
    }
}

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
        if (!a_actor) {
            return 150.0f; // Default fallback
        }

        // Try Precision API first
        if (m_precisionAPI) {
            auto* precision = static_cast<PRECISION_API::IVPrecision1*>(m_precisionAPI);
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
