#pragma once

#include "RE/A/Actor.h"
#include "RE/N/NiPoint3.h"

namespace CombatAI
{
    // Self state information
    struct SelfState
    {
        float staminaPercent = 0.0f;
        float healthPercent = 0.0f;
        RE::ATTACK_STATE_ENUM attackState = RE::ATTACK_STATE_ENUM::kNone;
        bool isBlocking = false;
        bool isIdle = false;
        RE::NiPoint3 position;
        RE::NiPoint3 forwardVector;
    };

    // Target state information
    struct TargetState
    {
        bool isValid = false;
        bool isBlocking = false;
        bool isAttacking = false;
        bool isPowerAttacking = false;
        bool isCasting = false;
        RE::KNOCK_STATE_ENUM knockState = RE::KNOCK_STATE_ENUM::kNormal;
        float distance = 0.0f;
        float orientationDot = 0.0f; // Dot product: 1.0 = facing directly at me, -1.0 = facing away
        RE::NiPoint3 position;
        RE::NiPoint3 forwardVector;
    };

    // Combined state data for decision making
    struct ActorStateData
    {
        SelfState self;
        TargetState target;
        float deltaTime = 0.0f;
        float weaponReach = 150.0f; // Weapon reach (from Precision or fallback)
    };

    // Helper functions
    namespace StateHelpers
    {
        // Calculate distance between two positions
        inline float CalculateDistance(const RE::NiPoint3& a, const RE::NiPoint3& b)
        {
            return a.GetDistance(b);
        }

        // Calculate dot product for orientation check
        // Returns 1.0 if target is directly facing self, -1.0 if facing away
        inline float CalculateOrientationDot(const RE::NiPoint3& selfPos, const RE::NiPoint3& targetPos, const RE::NiPoint3& targetForward)
        {
            RE::NiPoint3 toSelf = (selfPos - targetPos);
            toSelf.Unitize();
            return toSelf.Dot(targetForward);
        }

        // Get forward vector from actor (approximation using heading)
        inline RE::NiPoint3 GetActorForwardVector(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return RE::NiPoint3(0.0f, 1.0f, 0.0f);
            }

            // Get rotation (heading) and convert to forward vector
            float heading = a_actor->GetHeading(false);
            float cosH = std::cos(heading);
            float sinH = std::sin(heading);
            return RE::NiPoint3(sinH, cosH, 0.0f);
        }
    }
}
