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

        // Gather temporal state (time-based tracking)
        RE::Actor* target = nullptr;
        try {
            auto& runtimeData = a_actor->GetActorRuntimeData();
            RE::CombatController* combatController = runtimeData.combatController;
            if (combatController) {
                RE::ActorHandle targetHandle = combatController->targetHandle;
                RE::NiPointer<RE::Actor> targetPtr = targetHandle.get();
                if (targetPtr && targetPtr.get()) {
                    target = targetPtr.get();
                }
            }
        } catch (...) {
            // Target access failed
        }
        data.temporal = GatherTemporalState(a_actor, target, a_deltaTime);

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

        // Attack state - use safe wrapper
        state.attackState = ActorUtils::SafeGetAttackState(a_actor);
        state.isIdle = (state.attackState == RE::ATTACK_STATE_ENUM::kNone);

        // Blocking state - use safe wrapper
        state.isBlocking = ActorUtils::SafeIsBlocking(a_actor);

        // Movement state - sprinting/walking - use safe wrappers
        state.isSprinting = ActorUtils::SafeIsSprinting(a_actor);
        state.isWalking = ActorUtils::SafeIsWalking(a_actor);

        // Position - use safe wrapper
        auto posOpt = ActorUtils::SafeGetPosition(a_actor);
        if (posOpt.has_value()) {
            state.position = posOpt.value();
        } else {
            state.position = RE::NiPoint3(0.0f, 0.0f, 0.0f);
        }

        // Forward vector - GetActorForwardVector now uses safe wrapper internally
        state.forwardVector = StateHelpers::GetActorForwardVector(a_actor);

        // Weapon type information
        state.weaponType = StateHelpers::GetActorWeaponType(a_actor);
        
        // Set weapon category flags
        state.isOneHanded = (state.weaponType == WeaponType::OneHandedSword ||
                             state.weaponType == WeaponType::OneHandedDagger ||
                             state.weaponType == WeaponType::OneHandedMace ||
                             state.weaponType == WeaponType::OneHandedAxe);
        state.isTwoHanded = (state.weaponType == WeaponType::TwoHandedSword ||
                             state.weaponType == WeaponType::TwoHandedAxe);
        state.isRanged = (state.weaponType == WeaponType::Bow ||
                         state.weaponType == WeaponType::Crossbow);
        state.isMelee = (state.isOneHanded || state.isTwoHanded ||
                        state.weaponType == WeaponType::Unarmed ||
                        state.weaponType == WeaponType::Staff);

        return state;
    }

    TargetState ActorStateObserver::GatherTargetState(RE::Actor* a_self, RE::Actor* a_target)
    {
        TargetState state;

        if (!a_self || !a_target) {
            return state;
        }

        state.isValid = true;

        // Use safe wrappers for all target property access - target can be knocked down/deleted
        // Blocking state - use safe wrapper
        state.isBlocking = ActorUtils::SafeIsBlocking(a_target);

        // Attacking state - use safe wrapper
        state.isAttacking = ActorUtils::SafeIsAttacking(a_target);

        // Power attacking - IsPowerAttacking already has internal error handling
        state.isPowerAttacking = IsPowerAttacking(a_target);

        // Casting - use safe wrapper
        state.isCasting = (ActorUtils::SafeWhoIsCasting(a_target) != 0);

        // Drawing bow - check if target has bow/crossbow and is in draw state - use safe wrappers
        state.isDrawingBow = false;
        auto equippedWeapon = ActorUtils::SafeGetEquippedObject(a_target, false);
        if (equippedWeapon && equippedWeapon->IsWeapon()) {
            auto weapon = equippedWeapon->As<RE::TESObjectWEAP>();
            if (weapon && (weapon->IsBow() || weapon->IsCrossbow())) {
                RE::ATTACK_STATE_ENUM attackState = ActorUtils::SafeGetAttackState(a_target);
                // kDraw means they're drawing the bow
                state.isDrawingBow = (attackState == RE::ATTACK_STATE_ENUM::kDraw);
            }
        }

        // Attack recovery state - check if target just finished attack (kFollowThrough) - use safe wrapper
        RE::ATTACK_STATE_ENUM targetAttackState = ActorUtils::SafeGetAttackState(a_target);
        state.isInAttackRecovery = (targetAttackState == RE::ATTACK_STATE_ENUM::kFollowThrough);

        // Target health and stamina percentages
        state.healthPercent = GetActorValuePercent(a_target, RE::ActorValue::kHealth);
        state.staminaPercent = GetActorValuePercent(a_target, RE::ActorValue::kStamina);

        // Target movement state - sprinting/walking - use safe wrappers
        state.isSprinting = ActorUtils::SafeIsSprinting(a_target);
        state.isWalking = ActorUtils::SafeIsWalking(a_target);

        // Target fleeing state - check if target is fleeing from combat - use safe wrapper
        state.isFleeing = ActorUtils::SafeIsFleeing(a_target);

        // Knock state (stagger/recoil) - use safe wrapper (can crash when actor is knocked down)
        state.knockState = ActorUtils::SafeGetKnockState(a_target);

        // Position - use safe wrapper
        auto posOpt = ActorUtils::SafeGetPosition(a_target);
        if (posOpt.has_value()) {
            state.position = posOpt.value();
        } else {
            return state; // Can't get position, return incomplete state
        }

        // Forward vector - GetActorForwardVector now uses safe wrapper internally
        state.forwardVector = StateHelpers::GetActorForwardVector(a_target);

        // Distance - use safe wrapper for self position
        auto selfPosOpt = ActorUtils::SafeGetPosition(a_self);
        if (selfPosOpt.has_value()) {
            state.distance = StateHelpers::CalculateDistance(selfPosOpt.value(), state.position);
        } else {
            state.distance = 0.0f;
        }

        // Orientation dot product (is target looking at me?) - use safe wrapper for self position
        auto selfPosOpt2 = ActorUtils::SafeGetPosition(a_self);
        if (selfPosOpt2.has_value()) {
            state.orientationDot = StateHelpers::CalculateOrientationDot(
                selfPosOpt2.value(), state.position, state.forwardVector);
        } else {
            state.orientationDot = 0.0f;
        }

        // equipped weapons - use safe wrapper
        state.equippedRightHand = ActorUtils::SafeGetEquippedObject(a_target, false);

        // Weapon type information
        state.weaponType = StateHelpers::GetActorWeaponType(a_target);
        
        // Set weapon category flags
        state.isOneHanded = (state.weaponType == WeaponType::OneHandedSword ||
                             state.weaponType == WeaponType::OneHandedDagger ||
                             state.weaponType == WeaponType::OneHandedMace ||
                             state.weaponType == WeaponType::OneHandedAxe);
        state.isTwoHanded = (state.weaponType == WeaponType::TwoHandedSword ||
                             state.weaponType == WeaponType::TwoHandedAxe);
        state.isRanged = (state.weaponType == WeaponType::Bow ||
                         state.weaponType == WeaponType::Crossbow);
        state.isMelee = (state.isOneHanded || state.isTwoHanded ||
                        state.weaponType == WeaponType::Unarmed ||
                        state.weaponType == WeaponType::Staff);

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
        
        // Fallback: use weapon stat or default - use safe wrapper
        auto weapon = ActorUtils::SafeGetEquippedObject(a_actor, false);
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
            m_combatContextCache.Erase(formID);
            return context;
        }

        // Check cache first - use thread-safe operations
        auto cachedContextOpt = m_combatContextCache.Find(formID);
        if (cachedContextOpt.has_value()) {
            // Check if cache is still valid
            const auto& cached = cachedContextOpt.value();
            if (a_currentTime - cached.lastUpdateTime < COMBAT_CONTEXT_UPDATE_INTERVAL) {
                context = cached.context;
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
                // Use safe position access to prevent crashes
                auto selfPosOpt = ActorUtils::SafeGetPosition(a_actor);
                auto targetPosOpt = ActorUtils::SafeGetPosition(target.get());
                if (selfPosOpt.has_value() && targetPosOpt.has_value()) {
                    context.enemyCount = 1;
                    // Don't store raw pointer - it becomes invalid
                    context.closestEnemy = nullptr;
                    context.closestEnemyDistance = StateHelpers::CalculateDistance(
                        selfPosOpt.value(),
                        targetPosOpt.value()
                    );
                }
            }

            // Scan for nearby actors (enemies and allies) - expensive operation, done in one pass
            try {
                ScanForNearbyActors(a_actor, context, target.get());
            } catch (...) {
                // Scan failed, continue with whatever we have
            }
            
            // Calculate range granularity based on target distance and weapon reach
            if (target && target.get()) {
                float weaponReach = GetWeaponReach(a_actor);
                if (weaponReach <= 0.0f) {
                    weaponReach = 150.0f; // Fallback
                }
                
                float maxAttackRange = weaponReach * 1.5f; // Max attack range (with multiplier)
                float optimalAttackRange = weaponReach * 0.9f; // Optimal attack range
                float closeRange = optimalAttackRange * 0.6f; // Close range threshold
                
                float targetDistance = context.closestEnemyDistance;
                
                if (targetDistance <= closeRange) {
                    context.rangeCategory = RangeCategory::CloseRange;
                    context.isInCloseRange = true;
                    context.isInOptimalRange = true;
                    context.isInAttackRange = true;
                } else if (targetDistance <= optimalAttackRange) {
                    context.rangeCategory = RangeCategory::OptimalRange;
                    context.isInOptimalRange = true;
                    context.isInAttackRange = true;
                } else if (targetDistance <= maxAttackRange) {
                    context.rangeCategory = RangeCategory::MaxRange;
                    context.isInAttackRange = true;
                } else {
                    context.rangeCategory = RangeCategory::OutOfRange;
                }
            }
            
            // Calculate threat level based on enemy count
            if (context.enemyCount == 0) {
                context.threatLevel = ThreatLevel::None;
            } else if (context.enemyCount == 1) {
                context.threatLevel = ThreatLevel::Low;
            } else if (context.enemyCount == 2) {
                context.threatLevel = ThreatLevel::Moderate;
            } else if (context.enemyCount <= 4) {
                context.threatLevel = ThreatLevel::High;
            } else {
                context.threatLevel = ThreatLevel::Critical;
            }

            // Update cache - only if actor is still valid
            if (!ActorUtils::SafeIsDead(a_actor) && ActorUtils::SafeIsInCombat(a_actor)) {
                // Clear raw pointer before caching
                context.closestEnemy = nullptr;
                CachedCombatContext cached{context, a_currentTime};
                m_combatContextCache.Emplace(formID, std::move(cached));
            }
        } catch (...) {
            // Actor access failed - return empty context
            m_combatContextCache.Erase(formID);
            return context;
        }

        return context;
    }

    void ActorStateObserver::ScanForNearbyActors(RE::Actor* a_actor, CombatContext& a_context, RE::Actor* a_primaryTarget)
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
        int enemiesTargetingUs = 0; // Count enemies that have us as their target
        float closestDistance = a_context.closestEnemyDistance > 0.0f ? a_context.closestEnemyDistance : scanRange;
        
        // Get self forward vector for threat assessment
        RE::NiPoint3 selfForward = StateHelpers::GetActorForwardVector(a_actor);
        
        // Get primary target position and forward vector for flanking calculations
        RE::NiPoint3 targetPos = RE::NiPoint3(0.0f, 0.0f, 0.0f);
        RE::NiPoint3 targetForward = RE::NiPoint3(0.0f, 1.0f, 0.0f);
        bool hasTarget = false;
        if (a_primaryTarget) {
            auto targetPosOpt = ActorUtils::SafeGetPosition(a_primaryTarget);
            if (targetPosOpt.has_value()) {
                targetPos = targetPosOpt.value();
                targetForward = StateHelpers::GetActorForwardVector(a_primaryTarget);
                hasTarget = true;
            }
        }

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

                    // Check if this enemy is targeting us (has us as their combat target)
                    // This helps determine threat level
                    try {
                        auto& enemyRuntimeData = nearbyActor->GetActorRuntimeData();
                        RE::CombatController* enemyCombatController = enemyRuntimeData.combatController;
                        if (enemyCombatController) {
                            RE::ActorHandle enemyTargetHandle = enemyCombatController->targetHandle;
                            RE::NiPointer<RE::Actor> enemyTarget = enemyTargetHandle.get();
                            if (enemyTarget && enemyTarget.get() == a_actor) {
                                enemiesTargetingUs++; // This enemy is targeting us
                            }
                        }
                    } catch (...) {
                        // Enemy combat controller access failed, skip threat check
                    }

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
                        
                        // Calculate target facing relative to this ally (for flanking coordination)
                        if (hasTarget) {
                            RE::NiPoint3 targetToAlly = actorPos - targetPos;
                            float targetToAllyDistSq = targetToAlly.x * targetToAlly.x + targetToAlly.y * targetToAlly.y + targetToAlly.z * targetToAlly.z;
                            if (targetToAllyDistSq > 0.01f) {
                                float targetToAllyDist = std::sqrt(targetToAllyDistSq);
                                targetToAlly.x /= targetToAllyDist;
                                targetToAlly.y /= targetToAllyDist;
                                targetToAlly.z /= targetToAllyDist;
                                
                                // Calculate dot product: 1.0 = target facing ally, -1.0 = target facing away from ally
                                float targetAllyDot = targetForward.Dot(targetToAlly);
                                a_context.targetFacingAllyDot = targetAllyDot;
                                
                                // Set flags for easier decision-making
                                a_context.targetFacingAwayFromAlly = (targetAllyDot < -0.3f); // Target facing away from ally (good flanking opportunity)
                                a_context.targetFacingTowardAlly = (targetAllyDot > 0.3f); // Target facing toward ally (we can flank from behind)
                            }
                        }
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
        a_context.enemiesTargetingUs = enemiesTargetingUs;
        
        // If we have a primary target, they're also targeting us (count them)
        if (a_primaryTarget && a_context.enemyCount > 0) {
            a_context.enemiesTargetingUs++; // Primary target is targeting us
        }
    }

    TemporalState ActorStateObserver::GatherTemporalState(RE::Actor* a_actor, RE::Actor* a_target, float a_deltaTime)
    {
        TemporalState temporal;
        
        if (!a_actor) {
            return temporal;
        }

        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return temporal;
        }
        RE::FormID formID = formIDOpt.value();

        // Get or create actor temporal data
        auto* actorTemporalPtr = m_actorTemporalData.GetOrCreateDefault(formID);
        if (!actorTemporalPtr) {
            return temporal;
        }
        ActorTemporalData& actorTemporal = *actorTemporalPtr;

        // Update timers
        actorTemporal.timeSinceLastAttack += a_deltaTime;
        actorTemporal.timeSinceLastDodge += a_deltaTime;
        actorTemporal.timeSinceLastAction += a_deltaTime;
        actorTemporal.timeSinceLastPowerAttack += a_deltaTime;
        actorTemporal.timeSinceLastSprintAttack += a_deltaTime;
        actorTemporal.timeSinceLastBash += a_deltaTime;
        actorTemporal.timeSinceLastFeint += a_deltaTime;

        // Track duration of current states
        bool isBlocking = ActorUtils::SafeIsBlocking(a_actor);
        bool isAttacking = ActorUtils::SafeIsAttacking(a_actor);
        RE::ATTACK_STATE_ENUM attackState = ActorUtils::SafeGetAttackState(a_actor);
        bool isIdle = (attackState == RE::ATTACK_STATE_ENUM::kNone);

        if (isBlocking) {
            if (actorTemporal.wasBlocking) {
                actorTemporal.blockingDuration += a_deltaTime;
            } else {
                actorTemporal.blockingDuration = a_deltaTime; // Just started blocking
            }
        } else {
            actorTemporal.blockingDuration = 0.0f;
        }

        if (isAttacking || attackState != RE::ATTACK_STATE_ENUM::kNone) {
            if (actorTemporal.wasAttacking || actorTemporal.previousAttackState != RE::ATTACK_STATE_ENUM::kNone) {
                actorTemporal.attackingDuration += a_deltaTime;
            } else {
                actorTemporal.attackingDuration = a_deltaTime; // Just started attacking
            }
        } else {
            actorTemporal.attackingDuration = 0.0f;
        }

        if (isIdle) {
            if (actorTemporal.wasIdle) {
                actorTemporal.idleDuration += a_deltaTime;
            } else {
                actorTemporal.idleDuration = a_deltaTime; // Just became idle
            }
        } else {
            actorTemporal.idleDuration = 0.0f;
        }

        // Update previous states
        actorTemporal.wasBlocking = isBlocking;
        actorTemporal.wasAttacking = isAttacking;
        actorTemporal.wasIdle = isIdle;
        actorTemporal.previousAttackState = attackState;

        // Copy to output
        temporal.self.timeSinceLastAttack = actorTemporal.timeSinceLastAttack;
        temporal.self.timeSinceLastDodge = actorTemporal.timeSinceLastDodge;
        temporal.self.timeSinceLastAction = actorTemporal.timeSinceLastAction;
        temporal.self.blockingDuration = actorTemporal.blockingDuration;
        temporal.self.attackingDuration = actorTemporal.attackingDuration;
        temporal.self.idleDuration = actorTemporal.idleDuration;
        temporal.self.timeSinceLastPowerAttack = actorTemporal.timeSinceLastPowerAttack;
        temporal.self.timeSinceLastSprintAttack = actorTemporal.timeSinceLastSprintAttack;
        temporal.self.timeSinceLastBash = actorTemporal.timeSinceLastBash;
        temporal.self.timeSinceLastFeint = actorTemporal.timeSinceLastFeint;

        // Track target temporal state
        if (a_target) {
            auto targetFormIDOpt = ActorUtils::SafeGetFormID(a_target);
            if (targetFormIDOpt.has_value()) {
                RE::FormID targetFormID = targetFormIDOpt.value();
                
                // Use a composite key: actorFormID + targetFormID for target tracking
                // For simplicity, we'll use targetFormID and track per actor's perspective
                // Actually, let's use a simple approach: track target state per actor
                // We'll use formID (actor) as key, but store target-specific data
                // For now, let's track target state separately per actor-target pair
                // We'll use a hash of both FormIDs as the key
                RE::FormID targetKey = formID; // Use actor's FormID as base key
                
                auto* targetTemporalPtr = m_targetTemporalData.GetOrCreateDefault(targetKey);
                if (targetTemporalPtr) {
                    TargetTemporalData& targetTemporal = *targetTemporalPtr;

                    // Update timers
                    targetTemporal.timeSinceLastAttack += a_deltaTime;
                    targetTemporal.timeSinceLastPowerAttack += a_deltaTime;

                    // Track duration of target states
                    bool targetIsBlocking = ActorUtils::SafeIsBlocking(a_target);
                    bool targetIsAttacking = ActorUtils::SafeIsAttacking(a_target);
                    bool targetIsCasting = (ActorUtils::SafeWhoIsCasting(a_target) != 0);
                    bool targetIsDrawing = false;
                    auto targetEquippedWeapon = ActorUtils::SafeGetEquippedObject(a_target, false);
                    if (targetEquippedWeapon && targetEquippedWeapon->IsWeapon()) {
                        auto weapon = targetEquippedWeapon->As<RE::TESObjectWEAP>();
                        if (weapon && (weapon->IsBow() || weapon->IsCrossbow())) {
                            RE::ATTACK_STATE_ENUM targetAttackState = ActorUtils::SafeGetAttackState(a_target);
                            targetIsDrawing = (targetAttackState == RE::ATTACK_STATE_ENUM::kDraw);
                        }
                    }
                    RE::ATTACK_STATE_ENUM targetAttackState = ActorUtils::SafeGetAttackState(a_target);
                    bool targetIsIdle = (targetAttackState == RE::ATTACK_STATE_ENUM::kNone);

                    if (targetIsBlocking) {
                        if (targetTemporal.wasBlocking) {
                            targetTemporal.blockingDuration += a_deltaTime;
                        } else {
                            targetTemporal.blockingDuration = a_deltaTime;
                        }
                    } else {
                        targetTemporal.blockingDuration = 0.0f;
                    }

                    if (targetIsAttacking || targetAttackState != RE::ATTACK_STATE_ENUM::kNone) {
                        if (targetTemporal.wasAttacking || targetTemporal.previousAttackState != RE::ATTACK_STATE_ENUM::kNone) {
                            targetTemporal.attackingDuration += a_deltaTime;
                        } else {
                            targetTemporal.attackingDuration = a_deltaTime;
                        }
                    } else {
                        targetTemporal.attackingDuration = 0.0f;
                    }

                    if (targetIsCasting) {
                        if (targetTemporal.wasCasting) {
                            targetTemporal.castingDuration += a_deltaTime;
                        } else {
                            targetTemporal.castingDuration = a_deltaTime;
                        }
                    } else {
                        targetTemporal.castingDuration = 0.0f;
                    }

                    if (targetIsDrawing) {
                        if (targetTemporal.wasDrawing) {
                            targetTemporal.drawingDuration += a_deltaTime;
                        } else {
                            targetTemporal.drawingDuration = a_deltaTime;
                        }
                    } else {
                        targetTemporal.drawingDuration = 0.0f;
                    }

                    if (targetIsIdle) {
                        if (targetTemporal.wasIdle) {
                            targetTemporal.idleDuration += a_deltaTime;
                        } else {
                            targetTemporal.idleDuration = a_deltaTime;
                        }
                    } else {
                        targetTemporal.idleDuration = 0.0f;
                    }

                    // Detect when target attacks (state transition from not attacking to attacking)
                    if ((targetIsAttacking || targetAttackState == RE::ATTACK_STATE_ENUM::kSwing) && 
                        !targetTemporal.wasAttacking && 
                        targetTemporal.previousAttackState != RE::ATTACK_STATE_ENUM::kSwing) {
                        // Target just started attacking
                        targetTemporal.timeSinceLastAttack = 0.0f;
                    }

                    // Detect when target power attacks
                    if (targetAttackState == RE::ATTACK_STATE_ENUM::kSwing) {
                        // Check if it's a power attack (we'd need to check attack data, but for now we'll detect on follow-through)
                        // Actually, we'll detect power attack when target finishes attack (kFollowThrough) and was swinging
                        if (targetTemporal.previousAttackState == RE::ATTACK_STATE_ENUM::kSwing && 
                            targetAttackState == RE::ATTACK_STATE_ENUM::kFollowThrough) {
                            // Might be a power attack, but we can't be sure without checking attack data
                            // For now, we'll track it when we detect a power attack state
                        }
                    }

                    // Update previous states
                    targetTemporal.wasBlocking = targetIsBlocking;
                    targetTemporal.wasAttacking = targetIsAttacking;
                    targetTemporal.wasCasting = targetIsCasting;
                    targetTemporal.wasDrawing = targetIsDrawing;
                    targetTemporal.wasIdle = targetIsIdle;
                    targetTemporal.previousAttackState = targetAttackState;

                    // Copy to output
                    temporal.target.timeSinceLastAttack = targetTemporal.timeSinceLastAttack;
                    temporal.target.blockingDuration = targetTemporal.blockingDuration;
                    temporal.target.castingDuration = targetTemporal.castingDuration;
                    temporal.target.drawingDuration = targetTemporal.drawingDuration;
                    temporal.target.attackingDuration = targetTemporal.attackingDuration;
                    temporal.target.idleDuration = targetTemporal.idleDuration;
                    temporal.target.timeSinceLastPowerAttack = targetTemporal.timeSinceLastPowerAttack;
                }
            }
        }

        return temporal;
    }

    void ActorStateObserver::NotifyActionExecuted(RE::Actor* a_actor, ActionType a_action)
    {
        if (!a_actor) {
            return;
        }

        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return;
        }
        RE::FormID formID = formIDOpt.value();

        auto* actorTemporalPtr = m_actorTemporalData.GetMutable(formID);
        if (!actorTemporalPtr) {
            return;
        }

        // Reset timers based on action type
        actorTemporalPtr->timeSinceLastAction = 0.0f;

        switch (a_action) {
            case ActionType::Attack:
            case ActionType::PowerAttack:
            case ActionType::SprintAttack:
                actorTemporalPtr->timeSinceLastAttack = 0.0f;
                if (a_action == ActionType::PowerAttack) {
                    actorTemporalPtr->timeSinceLastPowerAttack = 0.0f;
                } else if (a_action == ActionType::SprintAttack) {
                    actorTemporalPtr->timeSinceLastSprintAttack = 0.0f;
                }
                break;
            case ActionType::Dodge:
            case ActionType::Jump:
            case ActionType::Strafe:
                actorTemporalPtr->timeSinceLastDodge = 0.0f;
                break;
            case ActionType::Bash:
                actorTemporalPtr->timeSinceLastBash = 0.0f;
                break;
            case ActionType::Feint:
                actorTemporalPtr->timeSinceLastFeint = 0.0f;
                break;
            default:
                break;
        }
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
        m_combatContextCache.Erase(formID);
        
        // Remove temporal data for this actor
        m_actorTemporalData.Erase(formID);
        m_targetTemporalData.Erase(formID);
    }
}
