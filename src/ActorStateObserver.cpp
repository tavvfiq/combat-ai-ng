#include "pch.h"
#include "ActorStateObserver.h"
#include "PrecisionIntegration.h"
#include "Config.h"
#include "ActorUtils.h"

// Undefine Windows macros that conflict with std functions
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef clamp
#undef clamp
#endif

namespace CombatAI
{
    // Helper function to clamp values (avoids Windows macro issues)
    template<typename T>
    constexpr T ClampValue(const T& value, const T& minVal, const T& maxVal)
    {
        return (value < minVal) ? minVal : ((value > maxVal) ? maxVal : value);
    }
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

        // Gather target state - wrap in try-catch to protect against invalid actor access
        try {
            auto& runtimeData = a_actor->GetActorRuntimeData();
            RE::CombatController* combatController = runtimeData.combatController;

            if (combatController) {
                RE::ActorHandle targetHandle = combatController->targetHandle;
                RE::NiPointer<RE::Actor> target = targetHandle.get();

                if (target && target.get()) {
                    data.target = GatherTargetState(a_actor, target.get());
                }
            }
        } catch (...) {
            // Actor or combat controller access failed - continue without target
        }

        // Gather combat context (multiple enemies, etc.) - cached for performance
        static float currentTime = 0.0f;
        currentTime += a_deltaTime;
        data.combatContext = GatherCombatContext(a_actor, currentTime);

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

        // Attack state - wrap in try-catch as AsActorState can crash if actor is invalid
        try {
            RE::ActorState* actorState = a_actor->AsActorState();
            if (actorState) {
                state.attackState = actorState->GetAttackState();
                state.isIdle = (state.attackState == RE::ATTACK_STATE_ENUM::kNone);
            }
        } catch (...) {
            state.attackState = RE::ATTACK_STATE_ENUM::kNone;
            state.isIdle = true;
        }

        // Blocking state
        try {
            state.isBlocking = a_actor->IsBlocking();
        } catch (...) {
            state.isBlocking = false;
        }

        // Position
        try {
            state.position = a_actor->GetPosition();
        } catch (...) {
            state.position = RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }

        // Forward vector
        try {
            state.forwardVector = StateHelpers::GetActorForwardVector(a_actor);
        } catch (...) {
            state.forwardVector = RE::NiPoint3(0.0f, 1.0f, 0.0f);
        }

        return state;
    }

    TargetState ActorStateObserver::GatherTargetState(RE::Actor* a_self, RE::Actor* a_target)
    {
        TargetState state;

        if (!a_self || !a_target) {
            return state;
        }

        state.isValid = true;

        // Wrap all target property access in try-catch - target can be knocked down/deleted
        // Blocking state
        try {
            state.isBlocking = a_target->IsBlocking();
        } catch (...) {
            state.isBlocking = false;
        }

        // Attacking state
        try {
            state.isAttacking = a_target->IsAttacking();
        } catch (...) {
            state.isAttacking = false;
        }

        // Power attacking
        try {
            state.isPowerAttacking = IsPowerAttacking(a_target);
        } catch (...) {
            state.isPowerAttacking = false;
        }

        // Casting
        try {
            state.isCasting = (a_target->WhoIsCasting() != 0);
        } catch (...) {
            state.isCasting = false;
        }

        // Drawing bow - check if target has bow/crossbow and is in draw state
        state.isDrawingBow = false;
        try {
            if (a_target) {
                auto equippedWeapon = a_target->GetEquippedObject(false);
                if (equippedWeapon && equippedWeapon->IsWeapon()) {
                    auto weapon = equippedWeapon->As<RE::TESObjectWEAP>();
                    if (weapon && (weapon->IsBow() || weapon->IsCrossbow())) {
                        RE::ActorState* targetState = nullptr;
                        try {
                            targetState = a_target->AsActorState();
                        } catch (...) {
                            targetState = nullptr;
                        }
                        if (targetState) {
                            try {
                                RE::ATTACK_STATE_ENUM attackState = targetState->GetAttackState();
                                // kDraw means they're drawing the bow
                                state.isDrawingBow = (attackState == RE::ATTACK_STATE_ENUM::kDraw);
                            } catch (...) {
                                // GetAttackState failed
                            }
                        }
                    }
                }
            }
        } catch (...) {
            state.isDrawingBow = false;
        }

        // Knock state (stagger/recoil) - CRITICAL: wrap in try-catch as this can crash when actor is knocked down
        try {
            RE::ActorState* targetState = nullptr;
            try {
                targetState = a_target->AsActorState();
            } catch (...) {
                targetState = nullptr;
            }
            if (targetState) {
                try {
                    state.knockState = targetState->GetKnockState();
                } catch (...) {
                    // GetKnockState failed - common when actor is transitioning states
                    state.knockState = RE::KNOCK_STATE_ENUM::kNormal;
                }
            } else {
                state.knockState = RE::KNOCK_STATE_ENUM::kNormal;
            }
        } catch (...) {
            state.knockState = RE::KNOCK_STATE_ENUM::kNormal;
        }

        // Position
        try {
            state.position = a_target->GetPosition();
        } catch (...) {
            return state; // Can't get position, return incomplete state
        }

        // Forward vector
        try {
            state.forwardVector = StateHelpers::GetActorForwardVector(a_target);
        } catch (...) {
            state.forwardVector = RE::NiPoint3(0.0f, 1.0f, 0.0f);
        }

        // Distance
        try {
            RE::NiPoint3 selfPos = a_self->GetPosition();
            state.distance = StateHelpers::CalculateDistance(selfPos, state.position);
        } catch (...) {
            state.distance = 0.0f;
        }

        // Orientation dot product (is target looking at me?)
        try {
            RE::NiPoint3 selfPos = a_self->GetPosition();
            state.orientationDot = StateHelpers::CalculateOrientationDot(
                selfPos, state.position, state.forwardVector);
        } catch (...) {
            state.orientationDot = 0.0f;
        }

        // equipped weapons
        try {
            state.equippedRightHand = a_target->GetEquippedObject(false);
        } catch (...) {
            state.equippedRightHand = nullptr;
        }

        return state;
    }

    float ActorStateObserver::GetActorValuePercent(RE::Actor* a_actor, RE::ActorValue a_value)
    {
        if (!a_actor) {
            return 0.0f;
        }

        auto actorValueOwner = a_actor->AsActorValueOwner();

        float current = actorValueOwner->GetActorValue(a_value);
        float max = actorValueOwner->GetBaseActorValue(a_value);

        if (max <= 0.0f) {
            return 0.0f;
        }

        return ClampValue(current / max, 0.0f, 1.0f);
    }

    bool ActorStateObserver::IsPowerAttacking(RE::Actor* a_target)
    {
        if (!a_target) {
            return false;
        }

        // Proper power attack detection using AttackData from HighProcessData
        // Based on ProjectGapClose implementation
        // Wrap in try-catch as process data access can crash when actor is in transitional states
        try {
            auto& runtimeData = a_target->GetActorRuntimeData();
            auto currentProcess = runtimeData.currentProcess;
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
        } catch (...) {
            // Process data access failed - actor may be in invalid state
            return false;
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
                return weaponForm->GetReach() * 100.0f; // Convert to game units
            }
        }
        
        return 150.0f; // Default fallback
    }

    CombatContext ActorStateObserver::GatherCombatContext(RE::Actor* a_actor, float a_currentTime)
    {
        CombatContext context;

        if (!a_actor) {
            return context;
        }

        // Get FormID safely - use FormID as key instead of raw pointer
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return context; // Can't get FormID, actor is invalid
        }
        RE::FormID formID = formIDOpt.value();

        // Validate actor before accessing cache
        if (ActorUtils::SafeIsDead(a_actor) || !ActorUtils::SafeIsInCombat(a_actor)) {
            // Actor is invalid, clean up cache and return empty context
            m_combatContextCache.erase(formID);
            return context;
        }

        // Check cache first
        auto cacheIt = m_combatContextCache.find(formID);
        if (cacheIt != m_combatContextCache.end()) {
            // Check if cache is still valid
            if (a_currentTime - cacheIt->second.lastUpdateTime < COMBAT_CONTEXT_UPDATE_INTERVAL) {
                context = cacheIt->second.context;
                // Ensure raw pointer is cleared (it was cleared when cached)
                context.closestEnemy = nullptr;
                return context;
            }
        }

        // Not in cache or expired, gather fresh data - wrap in try-catch
        try {
            auto& runtimeData = a_actor->GetActorRuntimeData();
            RE::CombatController* combatController = runtimeData.combatController;

            if (!combatController) {
                return context;
            }

            // Get primary target
            RE::ActorHandle primaryTarget = combatController->targetHandle;
            RE::NiPointer<RE::Actor> target = primaryTarget.get();
            if (target && target.get()) {
                try {
                    context.enemyCount = 1;
                    // Don't store raw pointer - it becomes invalid
                    context.closestEnemy = nullptr;
                    context.closestEnemyDistance = StateHelpers::CalculateDistance(
                        a_actor->GetPosition(),
                        target->GetPosition()
                    );
                } catch (...) {
                    // Position access failed, continue without primary target
                }
            }

            // Scan for nearby actors (enemies and allies) - expensive operation, done in one pass
            try {
                ScanForNearbyActors(a_actor, context);
            } catch (...) {
                // Scan failed, continue with whatever we have
            }

            // Update cache - only if actor is still valid
            if (!ActorUtils::SafeIsDead(a_actor) && ActorUtils::SafeIsInCombat(a_actor)) {
                // Clear raw pointer before caching
                context.closestEnemy = nullptr;
                m_combatContextCache[formID] = {context, a_currentTime};
            }
        } catch (...) {
            // Actor access failed - return empty context
            m_combatContextCache.erase(formID);
            return context;
        }

        return context;
    }

    void ActorStateObserver::ScanForNearbyActors(RE::Actor* a_actor, CombatContext& a_context)
    {
        if (!a_actor) {
            return;
        }

        // Validate actor before expensive operations - use wrapper
        if (!ActorUtils::SafeIsInCombat(a_actor)) {
            return;
        }

        // Scan range for nearby actors
        const float scanRange = 2000.0f; // Game units
        auto selfPosOpt = ActorUtils::SafeGetPosition(a_actor);
        if (!selfPosOpt.has_value()) {
            return; // Position access failed
        }
        RE::NiPoint3 selfPos = selfPosOpt.value();

        int additionalEnemies = 0;
        int allies = 0;
        float closestDistance = a_context.closestEnemyDistance > 0.0f ? a_context.closestEnemyDistance : scanRange;

        // Get current cell to scan actors in loaded area
        RE::TESObjectCELL* currentCell = nullptr;
        try {
            currentCell = a_actor->GetParentCell();
        } catch (...) {
            return; // Cell access failed
        }

        if (!currentCell || !currentCell->IsAttached()) {
            return;
        }

        // Scan actors in the current cell - single pass for both enemies and allies
        // Wrap entire scan in try-catch to handle iterator invalidation
        try {
            auto& runtimeData = currentCell->GetRuntimeData();
            
            // Get size first to detect container modification during iteration
            size_t initialSize = runtimeData.references.size();
            size_t processedCount = 0;
            
            for (auto& ref : runtimeData.references) {
                processedCount++;
                
                // Safety check: if container size changed, abort to avoid iterator invalidation
                if (runtimeData.references.size() != initialSize) {
                    break; // Container was modified, abort scan
                }
                
                RE::TESObjectREFR* refr = nullptr;
                try {
                    refr = ref.get();
                } catch (...) {
                    continue; // Reference access failed
                }
                
                if (!refr) {
                    continue;
                }

                RE::Actor* nearbyActor = nullptr;
                try {
                    nearbyActor = refr->As<RE::Actor>();
                } catch (...) {
                    continue; // Cast failed
                }
                
                if (!nearbyActor) {
                    continue;
                }

                // Skip self
                if (nearbyActor == a_actor) {
                    continue;
                }

                // Validate nearby actor before accessing properties
                // Use wrapper for safe validation
                if (ActorUtils::SafeIsDead(nearbyActor) || !ActorUtils::SafeIsInCombat(nearbyActor)) {
                    continue; // Actor validation failed
                }

                // Re-validate self actor before distance calculation
                if (ActorUtils::SafeIsDead(a_actor) || !ActorUtils::SafeIsInCombat(a_actor)) {
                    return; // Self became invalid, abort scan
                }

                // Check distance - use wrapper for safe position access
                auto actorPosOpt = ActorUtils::SafeGetPosition(nearbyActor);
                if (!actorPosOpt.has_value()) {
                    continue; // Position access failed
                }
                RE::NiPoint3 actorPos = actorPosOpt.value();
                
                float distance = StateHelpers::CalculateDistance(selfPos, actorPos);
                if (distance > scanRange) {
                    continue;
                }

                // Re-validate actors before hostility check (they might have become invalid)
                // Use wrapper for safe access
                if (ActorUtils::SafeIsDead(a_actor) || ActorUtils::SafeIsDead(nearbyActor) ||
                    !ActorUtils::SafeIsInCombat(a_actor) || !ActorUtils::SafeIsInCombat(nearbyActor)) {
                    continue; // Actor validation failed
                }
                
                // Determine if enemy or ally - use wrapper for safe hostility check
                bool isHostile = ActorUtils::SafeIsHostileToActor(a_actor, nearbyActor);
                
                // Re-validate actors one more time before using distance/position data
                // Actors can become invalid during the loop
                if (ActorUtils::SafeIsDead(nearbyActor) || !ActorUtils::SafeIsInCombat(nearbyActor)) {
                    continue; // Actor became invalid, skip
                }
                
                if (isHostile) {
                    // Found an additional enemy
                    additionalEnemies++;

                    // Update closest enemy if this one is closer
                    // Don't store raw pointer - it becomes invalid
                    if (distance < closestDistance) {
                        closestDistance = distance;
                        a_context.closestEnemy = nullptr; // Don't store raw pointer
                        a_context.closestEnemyDistance = distance;
                    }
                } else {
                    // Found an ally (non-hostile actor in combat)
                    allies++;
                    
                    // Track closest ally position for flanking calculations
                    // Only update if this ally is closer than previous closest
                    if (!a_context.hasNearbyAlly || distance < a_context.closestAllyDistance) {
                        a_context.closestAllyPosition = actorPos;
                        a_context.closestAllyDistance = distance;
                        a_context.hasNearbyAlly = (distance <= 1500.0f); // Flanking range threshold
                    }
                }
            }
        } catch (...) {
            // Scan failed, return with whatever we have so far
            // Don't update counts if scan failed completely
            return;
        }

        // Update counts
        a_context.enemyCount += additionalEnemies;
        a_context.allyCount = allies;
    }

    void ActorStateObserver::Cleanup(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return;
        }

        // Get FormID safely - use FormID as key instead of raw pointer
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return; // Can't get FormID, actor is invalid
        }
        RE::FormID formID = formIDOpt.value();

        // Remove cached combat context for this actor
        m_combatContextCache.erase(formID);
    }
}
