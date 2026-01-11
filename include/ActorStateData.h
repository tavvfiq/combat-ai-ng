#pragma once

#include "RE/A/Actor.h"
#include "RE/N/NiPoint3.h"
#include "ActorUtils.h"

namespace CombatAI
{
    // Weapon type classification
    enum class WeaponType : std::uint8_t
    {
        None = 0,
        Unarmed,
        OneHandedSword,
        OneHandedDagger,
        OneHandedMace,
        OneHandedAxe,
        TwoHandedSword,
        TwoHandedAxe,
        Bow,
        Crossbow,
        Staff, // Magic staff
        Spell // Magic/spell (no weapon)
    };

    // Self state information
    struct SelfState
    {
        float staminaPercent = 0.0f;
        float healthPercent = 0.0f;
        RE::ATTACK_STATE_ENUM attackState = RE::ATTACK_STATE_ENUM::kNone;
        bool isBlocking = false;
        bool isIdle = false;
        bool isSprinting = false; // Movement state: sprinting
        bool isWalking = false; // Movement state: walking
        RE::NiPoint3 position;
        RE::NiPoint3 forwardVector;
        WeaponType weaponType = WeaponType::None; // Weapon type equipped
        bool isOneHanded = false; // True if one-handed weapon
        bool isTwoHanded = false; // True if two-handed weapon
        bool isRanged = false; // True if ranged weapon (bow/crossbow)
        bool isMelee = false; // True if melee weapon
    };

    // Target state information
    struct TargetState
    {
        bool isValid = false;
        bool isBlocking = false;
        bool isAttacking = false;
        bool isPowerAttacking = false;
        bool isCasting = false;
        bool isDrawingBow = false;
        bool isInAttackRecovery = false; // Target just finished attack (kFollowThrough) - recovery window
        bool isSprinting = false; // Movement state: sprinting
        bool isWalking = false; // Movement state: walking
        bool isFleeing = false; // Target is fleeing/retreating from combat
        float healthPercent = 0.0f; // Target health percentage
        float staminaPercent = 0.0f; // Target stamina percentage
        RE::KNOCK_STATE_ENUM knockState = RE::KNOCK_STATE_ENUM::kNormal;
        float distance = 0.0f;
        float orientationDot = 0.0f; // Dot product: 1.0 = facing directly at me, -1.0 = facing away
        RE::NiPoint3 position;
        RE::NiPoint3 forwardVector;

        RE::TESForm* equippedRightHand = nullptr;
        WeaponType weaponType = WeaponType::None; // Target weapon type equipped
        bool isOneHanded = false; // True if target has one-handed weapon
        bool isTwoHanded = false; // True if target has two-handed weapon
        bool isRanged = false; // True if target has ranged weapon (bow/crossbow)
        bool isMelee = false; // True if target has melee weapon
    };

    // Threat level classification
    enum class ThreatLevel : std::uint8_t
    {
        None = 0,        // No enemies
        Low = 1,         // 1 enemy
        Moderate = 2,    // 2 enemies
        High = 3,        // 3-4 enemies
        Critical = 4     // 5+ enemies
    };

    // Range classification
    enum class RangeCategory : std::uint8_t
    {
        OutOfRange = 0,      // Beyond max attack range
        MaxRange = 1,        // Within max attack range but beyond optimal
        OptimalRange = 2,    // Within optimal attack range
        CloseRange = 3       // Very close (within 60% of optimal range)
    };

    // Combat context information (multiple enemies, allies, threat assessment)
    struct CombatContext
    {
        int enemyCount = 0; // Number of enemies in combat
        int allyCount = 0; // Number of allies in combat (friendly actors)
        RE::Actor* closestEnemy = nullptr; // Closest enemy (might not be primary target)
        float closestEnemyDistance = 0.0f;
        
        // Threat level - how many enemies are targeting us
        ThreatLevel threatLevel = ThreatLevel::None;
        int enemiesTargetingUs = 0; // Number of enemies that have us as their target
        
        // Flanking support: track closest ally position relative to target
        RE::NiPoint3 closestAllyPosition = RE::NiPoint3(0.0f, 0.0f, 0.0f);
        float closestAllyDistance = 0.0f;
        bool hasNearbyAlly = false; // True if ally is within flanking range
        
        // Target facing relative to allies - for better flanking coordination
        float targetFacingAllyDot = 0.0f; // Dot product: 1.0 = target facing ally, -1.0 = target facing away from ally
        bool targetFacingAwayFromAlly = false; // True if target is facing away from closest ally (good flanking opportunity)
        bool targetFacingTowardAlly = false; // True if target is facing toward closest ally (we can flank from behind)
        
        // Range granularity - precise range classification
        RangeCategory rangeCategory = RangeCategory::OutOfRange;
        bool isInAttackRange = false; // True if within max attack range
        bool isInOptimalRange = false; // True if within optimal attack range
        bool isInCloseRange = false; // True if very close (within 60% of optimal range)
    };

    // Temporal state information (time-based tracking for decision-making)
    struct SelfTemporalState
    {
        float timeSinceLastAttack = 999.0f; // Time since last attack (any type)
        float timeSinceLastDodge = 999.0f; // Time since last dodge/evasion
        float timeSinceLastAction = 999.0f; // Time since last action (any action type)
        float blockingDuration = 0.0f; // How long currently blocking
        float attackingDuration = 0.0f; // How long currently attacking
        float idleDuration = 0.0f; // How long currently idle
        float timeSinceLastPowerAttack = 999.0f; // Time since last power attack
        float timeSinceLastSprintAttack = 999.0f; // Time since last sprint attack
        float timeSinceLastBash = 999.0f; // Time since last bash
        float timeSinceLastFeint = 999.0f; // Time since last feint
    };

    struct TargetTemporalState
    {
        float timeSinceLastAttack = 999.0f; // Time since target last attacked
        float blockingDuration = 0.0f; // How long target has been blocking
        float castingDuration = 0.0f; // How long target has been casting
        float drawingDuration = 0.0f; // How long target has been drawing bow
        float attackingDuration = 0.0f; // How long target has been attacking
        float idleDuration = 0.0f; // How long target has been idle
        float timeSinceLastPowerAttack = 999.0f; // Time since target's last power attack
    };

    struct TemporalState
    {
        SelfTemporalState self;
        TargetTemporalState target;
    };

    // Combined state data for decision making
    struct ActorStateData
    {
        SelfState self;
        TargetState target;
        CombatContext combatContext; // Combat context (multiple enemies, etc.)
        TemporalState temporal; // Temporal state (time-based tracking)
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

        // Get forward vector from actor (approximation using heading) - uses safe wrapper
        inline RE::NiPoint3 GetActorForwardVector(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return RE::NiPoint3(0.0f, 1.0f, 0.0f);
            }

            // Get rotation (heading) and convert to forward vector - use safe wrapper
            auto headingOpt = ActorUtils::SafeGetHeading(a_actor, false);
            if (headingOpt.has_value()) {
                float heading = headingOpt.value();
                float cosH = std::cos(heading);
                float sinH = std::sin(heading);
                return RE::NiPoint3(sinH, cosH, 0.0f);
            }
            
            // Fallback if heading access failed
            return RE::NiPoint3(0.0f, 1.0f, 0.0f);
        }

        // Identify weapon type from TESObjectWEAP
        inline WeaponType IdentifyWeaponType(RE::TESObjectWEAP* a_weapon)
        {
            if (!a_weapon) {
                return WeaponType::None;
            }

            // Check for ranged weapons first
            if (a_weapon->IsBow()) {
                return WeaponType::Bow;
            }
            if (a_weapon->IsCrossbow()) {
                return WeaponType::Crossbow;
            }

            // Check weapon type using weaponData.animationType
            // RE::WEAPON_TYPE is an enumeration wrapper, need to get underlying value
            auto weaponTypeValue = a_weapon->weaponData.animationType.underlying();
            
            // RE::WEAPON_TYPE enum values:
            // kHandToHand = 0, kOneHandSword = 1, kOneHandDagger = 2, kOneHandAxe = 3,
            // kOneHandMace = 4, kTwoHandSword = 5, kTwoHandAxe = 6, kBow = 7, kCrossbow = 8, kStaff = 9
            switch (weaponTypeValue) {
                case 0: // kHandToHand
                    return WeaponType::Unarmed;
                case 1: // kOneHandSword
                    return WeaponType::OneHandedSword;
                case 2: // kOneHandDagger
                    return WeaponType::OneHandedDagger;
                case 3: // kOneHandAxe
                    return WeaponType::OneHandedAxe;
                case 4: // kOneHandMace
                    return WeaponType::OneHandedMace;
                case 5: // kTwoHandSword
                    return WeaponType::TwoHandedSword;
                case 6: // kTwoHandAxe
                    return WeaponType::TwoHandedAxe;
                case 7: // kBow
                    return WeaponType::Bow;
                case 8: // kCrossbow
                    return WeaponType::Crossbow;
                case 9: // kStaff
                    return WeaponType::Staff;
                default:
                    return WeaponType::None;
            }
        }

        // Get weapon type from actor (checks equipped weapon)
        inline WeaponType GetActorWeaponType(RE::Actor* a_actor)
        {
            if (!a_actor) {
                return WeaponType::None;
            }

            // Check right hand first
            auto weapon = ActorUtils::SafeGetEquippedObject(a_actor, false);
            if (weapon && weapon->IsWeapon()) {
                auto weap = weapon->As<RE::TESObjectWEAP>();
                if (weap) {
                    return IdentifyWeaponType(weap);
                }
            }

            // Check left hand (for dual wielding or spell)
            weapon = ActorUtils::SafeGetEquippedObject(a_actor, true);
            if (weapon) {
                if (weapon->IsWeapon()) {
                    auto weap = weapon->As<RE::TESObjectWEAP>();
                    if (weap) {
                        return IdentifyWeaponType(weap);
                    }
                } else {
                    // Left hand has spell/magic
                    return WeaponType::Spell;
                }
            }

            // Check if casting magic
            if (ActorUtils::SafeWhoIsCasting(a_actor) != 0) {
                return WeaponType::Spell;
            }

            // No weapon equipped - unarmed
            return WeaponType::Unarmed;
        }
    }
}
