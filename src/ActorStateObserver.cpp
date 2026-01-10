#include "pch.h"
#include "ActorStateObserver.h"
#include "PrecisionIntegration.h"
#include "Config.h"

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

        // Gather target state
        auto& runtimeData = a_actor->GetActorRuntimeData();
        RE::CombatController* combatController = runtimeData.combatController;

        if (combatController) {
            RE::ActorHandle targetHandle = combatController->targetHandle;
            RE::NiPointer<RE::Actor> target = targetHandle.get();

            if (target && target.get()) {
                data.target = GatherTargetState(a_actor, target.get());
            }
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

        // Drawing bow - check if target has bow/crossbow and is in draw state
        state.isDrawingBow = false;
        if (a_target) {
            auto equippedWeapon = a_target->GetEquippedObject(false);
            if (equippedWeapon && equippedWeapon->IsWeapon()) {
                auto weapon = equippedWeapon->As<RE::TESObjectWEAP>();
                if (weapon && (weapon->IsBow() || weapon->IsCrossbow())) {
                    RE::ActorState* targetState = a_target->AsActorState();
                    if (targetState) {
                        RE::ATTACK_STATE_ENUM attackState = targetState->GetAttackState();
                        // kDraw means they're drawing the bow
                        state.isDrawingBow = (attackState == RE::ATTACK_STATE_ENUM::kDraw);
                    }
                }
            }
        }

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

        // equipped weapons
        state.equippedRightHand = a_target->GetEquippedObject(false);

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

        // Check cache first
        auto cacheIt = m_combatContextCache.find(a_actor);
        if (cacheIt != m_combatContextCache.end()) {
            // Check if cache is still valid
            if (a_currentTime - cacheIt->second.lastUpdateTime < COMBAT_CONTEXT_UPDATE_INTERVAL) {
                return cacheIt->second.context;
            }
        }

        // Not in cache or expired, gather fresh data
        auto& runtimeData = a_actor->GetActorRuntimeData();
        RE::CombatController* combatController = runtimeData.combatController;

        if (!combatController) {
            return context;
        }

        // Get primary target
        RE::ActorHandle primaryTarget = combatController->targetHandle;
        RE::NiPointer<RE::Actor> target = primaryTarget.get();
        if (target && target.get()) {
            context.enemyCount = 1;
            context.closestEnemy = target.get();
            context.closestEnemyDistance = StateHelpers::CalculateDistance(
                a_actor->GetPosition(),
                target->GetPosition()
            );
        }

        // Scan for nearby actors (enemies and allies) - expensive operation, done in one pass
        ScanForNearbyActors(a_actor, context);

        // Update cache
        m_combatContextCache[a_actor] = {context, a_currentTime};

        return context;
    }

    void ActorStateObserver::ScanForNearbyActors(RE::Actor* a_actor, CombatContext& a_context)
    {
        if (!a_actor || !a_actor->IsInCombat()) {
            return;
        }

        // Scan range for nearby actors
        const float scanRange = 2000.0f; // Game units
        RE::NiPoint3 selfPos = a_actor->GetPosition();
        int additionalEnemies = 0;
        int allies = 0;
        float closestDistance = a_context.closestEnemyDistance > 0.0f ? a_context.closestEnemyDistance : scanRange;

        // Get current cell to scan actors in loaded area
        RE::TESObjectCELL* currentCell = a_actor->GetParentCell();
        if (!currentCell || !currentCell->IsAttached()) {
            return;
        }

        // Scan actors in the current cell - single pass for both enemies and allies
        auto& runtimeData = currentCell->GetRuntimeData();
        for (auto& ref : runtimeData.references) {
            RE::TESObjectREFR* refr = ref.get();
            if (!refr) {
                continue;
            }

            RE::Actor* nearbyActor = refr->As<RE::Actor>();
            if (!nearbyActor) {
                continue;
            }

            // Skip self
            if (nearbyActor == a_actor) {
                continue;
            }

            // Skip invalid actors
            if (nearbyActor->IsDead()) {
                continue;
            }

            // Check if in combat
            if (!nearbyActor->IsInCombat()) {
                continue;
            }

            // Check distance
            RE::NiPoint3 actorPos = nearbyActor->GetPosition();
            float distance = StateHelpers::CalculateDistance(selfPos, actorPos);
            if (distance > scanRange) {
                continue;
            }

            // Determine if enemy or ally
            bool isHostile = a_actor->IsHostileToActor(nearbyActor);
            
            if (isHostile) {
                // Found an additional enemy
                additionalEnemies++;

                // Update closest enemy if this one is closer
                if (distance < closestDistance) {
                    closestDistance = distance;
                    a_context.closestEnemy = nearbyActor;
                    a_context.closestEnemyDistance = distance;
                }
            } else {
                // Found an ally (non-hostile actor in combat)
                allies++;
            }
        }

        // Update counts
        a_context.enemyCount += additionalEnemies;
        a_context.allyCount = allies;
    }

    void ActorStateObserver::Cleanup(RE::Actor* a_actor)
    {
        if (a_actor) {
            m_combatContextCache.erase(a_actor);
        }
    }
}
