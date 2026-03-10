#include "CombatDirector.h"
#include "APIManager.h"
#include "ActorUtils.h"
#include "AttackDefenseFeedbackTracker.h"
#include "Config.h"
#include "GuardCounterFeedbackTracker.h"
#include "Logger.h"
#include "ModEventSinks.h"
#include "PacingPackageManager.h"
#include "ParryFeedbackTracker.h"
#include "PrecisionIntegration.h"
#include "TimedBlockFeedbackTracker.h"
#include "TimedBlockIntegration.h"
#include "pch.h"
#include <sstream>

namespace CombatAI
{
    // Track if mod callback events are being received
    // These flags are accessed by ModEventSinks.cpp
    bool s_receivedParryModEvent = false;
    bool s_receivedTimedBlockModEvent = true; // Set to true since mod events work for timed blocks
    void CombatDirector::Initialize()
    {
        LOG_INFO("CombatDirector initialized");

        // Initialize Precision integration (if enabled in config)
        auto &config = Config::GetInstance();
        if (config.GetModIntegrations().enablePrecisionIntegration) {
            PrecisionIntegration::GetInstance().Initialize();
        }

        // Initialize Timed Block integration (if enabled in config)
        if (config.GetTimedBlock().enableTimedBlock) {
            TimedBlockIntegration::GetInstance().Initialize();
        }

        if (config.GetModIntegrations().enableBFCOIntegration) {
            // Check if BFCO plugin is loaded
            auto dataHandler = RE::TESDataHandler::GetSingleton();
            if (dataHandler) {
                auto bfcoPlugin = dataHandler->LookupModByName("SCSI-ACTbfco-Main.esp");
                if (bfcoPlugin) {
                    m_executor.EnableBFCO(true);
                }
            } else {
                m_executor.EnableBFCO(false);
            }
        }

        // Set processing interval
        m_processInterval = config.GetGeneral().processingInterval;

        // Initialize humanizer with config values
        Humanizer::Config humanizerConfig;
        humanizerConfig.baseReactionDelayMs = config.GetHumanizer().baseReactionDelayMs;
        humanizerConfig.reactionVarianceMs = config.GetHumanizer().reactionVarianceMs;
        humanizerConfig.bashCooldownSeconds = config.GetHumanizer().bashCooldownSeconds;
        humanizerConfig.dodgeCooldownSeconds = config.GetHumanizer().dodgeCooldownSeconds;
        humanizerConfig.jumpCooldownSeconds = config.GetHumanizer().jumpCooldownSeconds;
        humanizerConfig.bashMistakeMultiplier = config.GetHumanizer().bashMistakeMultiplier;
        humanizerConfig.dodgeMistakeMultiplier = config.GetHumanizer().dodgeMistakeMultiplier;
        humanizerConfig.jumpMistakeMultiplier = config.GetHumanizer().jumpMistakeMultiplier;
        humanizerConfig.strafeMistakeMultiplier = config.GetHumanizer().strafeMistakeMultiplier;
        humanizerConfig.powerAttackMistakeMultiplier = config.GetHumanizer().powerAttackMistakeMultiplier;
        humanizerConfig.attackMistakeMultiplier = config.GetHumanizer().attackMistakeMultiplier;
        humanizerConfig.sprintAttackMistakeMultiplier = config.GetHumanizer().sprintAttackMistakeMultiplier;
        humanizerConfig.retreatMistakeMultiplier = config.GetHumanizer().retreatMistakeMultiplier;
        humanizerConfig.backoffMistakeMultiplier = config.GetHumanizer().backoffMistakeMultiplier;
        humanizerConfig.advancingMistakeMultiplier = config.GetHumanizer().advancingMistakeMultiplier;
        humanizerConfig.flankingMistakeMultiplier = config.GetHumanizer().flankingMistakeMultiplier;
        m_humanizer.SetConfig(humanizerConfig);

        // Initialize PacingPackageManager (WYT integration, optional)
        if (config.GetCombatPacing().enableCombatPacing) {
            PacingPackageManager::GetInstance().Initialize();
        }

        // Register mod callback listeners (for EldenParry integration)
        RegisterModCallbacks();
    }

    void CombatDirector::RegisterModCallbacks()
    {
        auto modCallbackEventSource = SKSE::GetModCallbackEventSource();
        if (!modCallbackEventSource) {
            LOG_WARN("Mod callback event source not available");
            return;
        }

        auto &config = Config::GetInstance();

        // Register parry callbacks if parry is enabled
        if (config.GetParry().enableParry) {
            static EldenParryEventSink parryEventSink;
            modCallbackEventSource->AddEventSink(&parryEventSink);
            LOG_INFO("Registered mod callback listeners for EldenParry integration");
        } else {
            LOG_INFO("Parry is disabled, skipping EldenParry callback registration");
        }

        // Register timed block callbacks if timed block is enabled
        if (config.GetTimedBlock().enableTimedBlock) {
            static TimedBlockEventSink timedBlockEventSink;
            modCallbackEventSource->AddEventSink(&timedBlockEventSink);
            LOG_INFO("Registered mod callback listeners for Simple Timed Block integration");
        } else {
            LOG_INFO("Timed Block is disabled, skipping Simple Timed Block callback "
                     "registration");
        }

        // Register TESHitEvent sink to detect when NPC attacks successfully hit the
        // player This is always enabled as it's needed for hit/miss tracking
        auto eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
        if (eventSourceHolder) {
            static AttackHitEventSink hitEventSink;
            eventSourceHolder->AddEventSink<RE::TESHitEvent>(&hitEventSink);
            LOG_INFO("Registered TESHitEvent sink for attack hit detection");
        } else {
            LOG_WARN("ScriptEventSourceHolder not available for hit detection");
        }
    }

    void CombatDirector::ProcessActor(RE::Actor *a_actor, float a_deltaTime)
    {
        // Debug logging - log ProcessActor calls occasionally
        static std::uint32_t processActorCallCount = 0;
        processActorCallCount++;

        auto &config = Config::GetInstance();
        bool debugEnabled = config.GetGeneral().enableDebugLog;

        if (!ShouldProcessActor(a_actor, a_deltaTime)) {
            return;
        }

        if (debugEnabled && processActorCallCount % 100 == 0) { // Log every 100 calls
            auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
            if (formIDOpt.has_value()) {
                std::uint32_t formID = static_cast<std::uint32_t>(formIDOpt.value());
                LOG_DEBUG("ProcessActor called for actor FormID: 0x{:08X}", formID);
            }
        }

        // Check if actor can react (reaction delay)
        // For movement actions, allow continuous execution even during reaction delay
        // (we check this after getting the decision to see if it's a movement action)
        bool canReact = m_humanizer.CanReact(a_actor, a_deltaTime);

        // Gather state first to check decision type
        ActorStateData state = m_observer.GatherState(a_actor, a_deltaTime);

        // --- Combat Pacing: timed slot system ---
        const auto &pacingConfig = config.GetCombatPacing();
        RE::FormID actorID = 0;
        RE::FormID targetID = 0;
        if (pacingConfig.enableCombatPacing) {
            auto actorIDOpt = ActorUtils::SafeGetFormID(a_actor);
            if (actorIDOpt.has_value()) {
                actorID = actorIDOpt.value();
            }
            if (state.target.isValid) {
                auto combatTarget = ActorUtils::SafeGetCombatTarget(a_actor);
                if (combatTarget) {
                    auto tidOpt = ActorUtils::SafeGetFormID(combatTarget);
                    if (tidOpt.has_value()) {
                        targetID = tidOpt.value();
                    }
                }
            }

            if (actorID != 0 && targetID != 0) {
                std::lock_guard<std::mutex> slotLock(m_slotMutex);

                // Does this NPC already hold a valid slot for this target?
                bool alreadyHoldsSlot = false;
                auto selfIt = m_slotHolders.find(actorID);
                if (selfIt != m_slotHolders.end()) {
                    if (selfIt->second.targetID == targetID && selfIt->second.expiryTime > m_elapsedTime) {
                        alreadyHoldsSlot = true; // keep fighting
                    } else {
                        // Slot expired or target changed — relinquish
                        m_slotHolders.erase(selfIt);
                    }
                }

                if (!alreadyHoldsSlot) {
                    // Count how many OTHERS currently hold a slot for this target
                    int activeSlots = 0;
                    for (const auto &[aid, entry] : m_slotHolders) {
                        if (entry.targetID == targetID && entry.expiryTime > m_elapsedTime) {
                            ++activeSlots;
                        }
                    }
                    state.attackSlotsUsedOnTarget = activeSlots;

                    // Dynamic max: scale with enemy count so large groups feel more
                    // aggressive, while small groups remain disciplined.
                    // Formula: base + 1 extra attacker per 3 enemies beyond the first,
                    // capped at 2x the configured base.
                    int baseMax = pacingConfig.maxSimultaneousAttackers;
                    int enemyCount = state.combatContext.enemyCount;
                    int bonus = (std::max)(0, (enemyCount - 1) / 3);
                    int effectiveMax = (std::min)(baseMax * 2, baseMax + bonus);
                    state.heldBackByPacing = (activeSlots >= effectiveMax);

                    if (state.heldBackByPacing) {
                        static constexpr float kOpportunityDotThreshold = -0.3f;
                        static constexpr float kEngagementDotThreshold = 0.7f;
                        static constexpr float kEngagementDistThreshold = 200.0f;

                        bool targetFacingAway = state.target.orientationDot < kOpportunityDotThreshold;
                        bool targetEngagingUs = state.target.orientationDot > kEngagementDotThreshold &&
                                                state.target.distance < kEngagementDistThreshold;

                        auto DoSlotTransfer = [&](const char *reason) {
                            // Evict the slot holder with least remaining time (fairest to evict)
                            RE::FormID evictID = 0;
                            float shortestRemaining = (std::numeric_limits<float>::max)();
                            for (const auto &[aid, entry] : m_slotHolders) {
                                if (aid != actorID && entry.targetID == targetID && entry.expiryTime > m_elapsedTime) {
                                    float remaining = entry.expiryTime - m_elapsedTime;
                                    if (remaining < shortestRemaining) {
                                        shortestRemaining = remaining;
                                        evictID = aid;
                                    }
                                }
                            }
                            if (evictID != 0) {
                                m_slotHolders.erase(evictID);
                                LOG_DEBUG("Pacing: {} — evicted {:08X}, {:08X} claims slot", reason, evictID, actorID);
                            }
                            static constexpr float kProvisionalWindowSeconds = 0.5f;
                            m_slotHolders[actorID] = SlotEntry{targetID, m_elapsedTime + kProvisionalWindowSeconds};
                            PacingPackageManager::GetInstance().UnlockEnemy(a_actor);
                            state.heldBackByPacing = false;
                        };

                        if (targetFacingAway) {
                            // --- Opportunity Attack ---
                            // Target's back is turned — seize the flank
                            DoSlotTransfer("opportunity");
                        } else if (targetEngagingUs) {
                            // --- Engagement Priority ---
                            // Target is actively facing and closing on this NPC — they get priority
                            DoSlotTransfer("engagement");
                        } else {
                            // Apply WYT package override (no-op when WYT absent)
                            PacingPackageManager::GetInstance().LockEnemy(a_actor);
                        }
                    } else {
                        // *** RACE-CONDITION FIX ***
                        // Reserve the slot NOW (inside the mutex) with a short provisional
                        // window so that another NPC processed in the same tick cannot also
                        // see an empty slot and sneak in.
                        // The window is extended to the full SlotWindowMin-Max duration
                        // if this NPC actually executes an attack below.
                        static constexpr float kProvisionalWindowSeconds = 0.5f;
                        m_slotHolders[actorID] = SlotEntry{targetID, m_elapsedTime + kProvisionalWindowSeconds};
                        // Also remove any lingering WYT lock from a previous held-back frame
                        PacingPackageManager::GetInstance().UnlockEnemy(a_actor);
                        LOG_DEBUG("Pacing: actor {:08X} reserved provisional slot for {:08X}", actorID, targetID);
                    }
                } else {
                    // This NPC already holds a valid slot — not held back
                    state.attackSlotsUsedOnTarget = 0;
                    state.heldBackByPacing = false;
                }
            }
        }

        DecisionResult decision = m_decisionMatrix.Evaluate(a_actor, state);

        if (!canReact) {
            return; // can't react yet
        }

        // State and decision already gathered above for movement action check

        // Check if should make mistake (humanizer)
        if (decision.action != ActionType::None && m_humanizer.ShouldMakeMistake(a_actor, decision.action)) {
            // Make a mistake - don't execute the action
            return;
        }

        // Check cooldowns (only actions with cooldowns will return true)
        if (m_humanizer.IsOnCooldown(a_actor, decision.action)) {
            return; // On cooldown
        }

        // Execute decision
        if (decision.action != ActionType::None) {
            bool success = m_executor.Execute(a_actor, decision, state);

            if (success) {
                auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
                if (formIDOpt.has_value()) {
                    m_processedActors.Insert(formIDOpt.value());
                }

                // Mark action as used (start cooldown)
                m_humanizer.MarkActionUsed(a_actor, decision.action);

                // Notify temporal state tracker that action was executed
                m_observer.NotifyActionExecuted(a_actor, decision.action);

                // Notify Public API listeners
                APIManager::GetSingleton()->NotifyDecision(a_actor, decision);

                // Extend provisional slot to full window on confirmed attack
                if (pacingConfig.enableCombatPacing && actorID != 0 && targetID != 0 && !state.heldBackByPacing &&
                    (decision.action == ActionType::Attack || decision.action == ActionType::PowerAttack ||
                     decision.action == ActionType::SprintAttack)) {
                    std::lock_guard<std::mutex> slotLock(m_slotMutex);
                    // Extend whatever slot we have (provisional or existing) to the full window
                    float window = m_slotDist(m_rng);
                    m_slotHolders[actorID] = SlotEntry{targetID, m_elapsedTime + window};
                    LOG_DEBUG("Pacing: actor {:08X} confirmed slot for {:08X} for {:.1f}s", actorID, targetID, window);
                }
            }
        }
    }

    void CombatDirector::OnNPCHit(RE::Actor *a_victim)
    {
        if (!a_victim) {
            return;
        }

        const auto &pacingConfig = Config::GetInstance().GetCombatPacing();
        if (!pacingConfig.enableCombatPacing) {
            return;
        }

        auto victimIDOpt = ActorUtils::SafeGetFormID(a_victim);
        if (!victimIDOpt.has_value()) {
            return;
        }
        RE::FormID victimID = victimIDOpt.value();

        // Grant a short retaliation window so this NPC can hit back
        // This overrides any WYT lock for the duration of the window
        static constexpr float kRetaliationWindowSeconds = 2.5f;

        std::lock_guard<std::mutex> slotLock(m_slotMutex);

        // Only react if this NPC doesn't already hold a real (non-provisional) slot
        auto it = m_slotHolders.find(victimID);
        bool alreadyHasFullSlot =
            (it != m_slotHolders.end() && it->second.expiryTime > m_elapsedTime + kRetaliationWindowSeconds);
        if (alreadyHasFullSlot) {
            return; // Already actively attacking — no need
        }

        // Resolve their current target to store in the slot
        RE::FormID targetID = 0;
        auto combatTarget = ActorUtils::SafeGetCombatTarget(a_victim);
        if (combatTarget) {
            auto tidOpt = ActorUtils::SafeGetFormID(combatTarget);
            if (tidOpt.has_value()) {
                targetID = tidOpt.value();
            }
        }

        // Grant the retaliation slot and remove WYT lock
        m_slotHolders[victimID] = SlotEntry{targetID, m_elapsedTime + kRetaliationWindowSeconds};
        PacingPackageManager::GetInstance().UnlockEnemy(a_victim);
        LOG_DEBUG("Pacing: actor {:08X} granted retaliation slot (was hit)", victimID);
    }

    void CombatDirector::Update(float a_deltaTime)
    {
        // Update per-frame systems (must be called once per frame, not per actor)
        m_humanizer.Update(a_deltaTime);
        m_observer.Update(a_deltaTime);

        // Advance elapsed time and prune expired attack slots
        {
            std::lock_guard<std::mutex> slotLock(m_slotMutex);
            m_elapsedTime += a_deltaTime;
            // Update slotDist bounds from config (cheap struct copy, harmless every frame)
            const auto &pacingCfg = Config::GetInstance().GetCombatPacing();
            m_slotDist =
                std::uniform_real_distribution<float>(pacingCfg.slotWindowMinSeconds, pacingCfg.slotWindowMaxSeconds);
            // Purge expired holders so their slots become available for the next wave
            for (auto it = m_slotHolders.begin(); it != m_slotHolders.end();) {
                if (it->second.expiryTime <= m_elapsedTime) {
                    LOG_DEBUG("Pacing: slot for actor {:08X} expired at t={:.1f}", it->first, m_elapsedTime);
                    it = m_slotHolders.erase(it);
                } else {
                    ++it;
                }
            }
        }

        auto &config = Config::GetInstance();

        // Update parry feedback tracker (only if parry is enabled)
        if (config.GetParry().enableParry) {
            ParryFeedbackTracker::GetInstance().Update(a_deltaTime);
        }

        // Update timed block feedback tracker (only if timed block is enabled)
        if (config.GetTimedBlock().enableTimedBlock) {
            TimedBlockFeedbackTracker::GetInstance().Update(a_deltaTime);
        }

        // Update attack defense feedback tracker (always update)
        AttackDefenseFeedbackTracker::GetInstance().Update(a_deltaTime);

        // Update guard counter feedback tracker (always update)
        GuardCounterFeedbackTracker::GetInstance().Update(a_deltaTime);

        // Clean up spawn time entries:
        // 1. Remove entries for actors that have been processed for a while (after
        // warmup period)
        // 2. This also catches actors that left combat (their spawn times stop
        // incrementing)
        m_actorSpawnTimes.WithWriteLock([&](auto &spawnTimesMap) {
            auto spawnIt = spawnTimesMap.begin();
            while (spawnIt != spawnTimesMap.end()) {
                // Clean up old spawn times (actors that have been processed for a while
                // or left combat)
                if (spawnIt->second > SPAWN_WARMUP_DELAY + 5.0f) {
                    // Actor has been processed for 5+ seconds after warmup, remove spawn
                    // time entry This also cleans up entries for actors that left combat
                    // (their times stop incrementing)
                    spawnIt = spawnTimesMap.erase(spawnIt);
                } else {
                    ++spawnIt;
                }
            }
        });

        // Periodic cleanup (from config)
        static float cleanupTimer = 0.0f;
        float cleanupInterval = config.GetPerformance().cleanupInterval;
        cleanupTimer += a_deltaTime;
        if (cleanupTimer > cleanupInterval) {
            Cleanup();
            cleanupTimer = 0.0f;
        }
    }

    void CombatDirector::Cleanup()
    {
        // With FormID keys, cleanup is much simpler
        // FormIDs are stable identifiers - they don't become invalid
        // We rely on lazy cleanup: entries are removed when actors leave combat
        // (checked in ProcessActor when actor is not in combat)
        // This avoids expensive LookupByID() calls that could crash

        // Note: We don't need to do aggressive cleanup here since:
        // 1. FormIDs don't become invalid (unlike pointers)
        // 2. Entries are cleaned up lazily when actors leave combat
        // 3. Avoiding LookupByID() prevents crashes from invalid FormIDs or deleted
        // forms

        // Clean up Humanizer state (it also uses lazy cleanup)
        m_humanizer.Cleanup();

        // Clean up spawn times for actors no longer in combat
        // This is done lazily in Update(), but we can also clean up here if needed
        // (Spawn times are cleaned up automatically in Update() after warmup period)

        // If we want to be more aggressive, we could add a size limit and remove
        // oldest entries But for now, lazy cleanup is safer and more efficient
    }

    bool CombatDirector::ShouldProcessActor(RE::Actor *a_actor, float a_deltaTime)
    {
        if (!a_actor) {
            return false;
        }

        // Additional validation for newly spawned actors:
        // Check if actor has a valid FormID before processing
        // Newly spawned actors might not have FormID initialized yet
        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value() || formIDOpt.value() == RE::FormID(0)) {
            return false; // Actor not fully initialized yet
        }

        // Validate actor using safe wrappers - protects against transitional states
        // Actor is passed directly from hook, but could become invalid at any time

        // Quick validation - if actor is dead or not in combat, skip early
        bool isDead = ActorUtils::SafeIsDead(a_actor);
        bool inCombat = ActorUtils::SafeIsInCombat(a_actor);
        if (isDead || !inCombat) {
            return false;
        }

        // Skip player
        if (ActorUtils::SafeIsPlayerRef(a_actor)) {
            return false;
        }

        // Only process NPCs, not creatures
        // Check for ActorTypeNPC keyword (NPCs have this, creatures don't)
        bool hasNPCCheck = ActorUtils::SafeHasKeywordString(a_actor, "ActorTypeNPC");
        if (!hasNPCCheck) {
            return false;
        }

        // Double-check: explicitly exclude creatures
        if (ActorUtils::SafeHasKeywordString(a_actor, "ActorTypeCreature")) {
            return false;
        }

        // Additional check: use CalculateCachedOwnerIsNPC as fallback
        // This should match the keyword check, but provides extra safety
        if (!ActorUtils::SafeCalculateCachedOwnerIsNPC(a_actor)) {
            return false;
        }

        // Check if we should only process combat actors
        auto &config = Config::GetInstance();
        if (config.GetPerformance().onlyProcessCombatActors) {
            // Only process actors in combat (already checked above, but check again for
            // consistency)
            if (!ActorUtils::SafeIsInCombat(a_actor)) {
                return false;
            }
        }

        // Check if AI is enabled
        if (!ActorUtils::SafeIsAIEnabled(a_actor)) {
            return false;
        }

        RE::FormID formID = formIDOpt.value();

        // Check spawn warmup delay for newly spawned actors
        // This prevents processing actors before they're fully initialized
        auto spawnTimeOpt = m_actorSpawnTimes.Find(formID);
        if (!spawnTimeOpt.has_value()) {
            // First time seeing this actor - record spawn time
            // Start at 0.0f, we'll increment it each frame
            m_actorSpawnTimes.Emplace(formID, 0.0f);
            // Don't process newly spawned actors immediately
            return false;
        }

        // Increment spawn time using the deltaTime for this actor
        // This ensures spawn times are updated even if Update() hasn't been called
        // yet
        auto *spawnTimePtr = m_actorSpawnTimes.GetMutable(formID);
        if (spawnTimePtr) {
            *spawnTimePtr += a_deltaTime;
        }

        // Check if actor is still in warmup period
        auto currentSpawnTimeOpt = m_actorSpawnTimes.Find(formID);
        if (currentSpawnTimeOpt.has_value() && currentSpawnTimeOpt.value() < SPAWN_WARMUP_DELAY) {
            // Actor is still warming up, don't process yet
            return false;
        }

        // Use thread-safe map operations for timer
        auto *timerPtr = m_actorProcessTimers.GetMutable(formID);
        if (!timerPtr) {
            // First time processing this actor, initialize timer
            auto [inserted, newTimerPtr] = m_actorProcessTimers.Emplace(formID, 0.0f);
            if (!inserted || !newTimerPtr) {
                return false; // Failed to insert
            }
            return true; // First time, allow processing
        }

        // Update timer
        *timerPtr += a_deltaTime;

        // Determine processing interval based on distance to player (LOD)
        // Get player position - safely
        auto player = RE::PlayerCharacter::GetSingleton();
        float targetInterval = m_processInterval; // Default to near interval

        if (player) {
            auto playerPos = player->GetPosition();
            auto actorPosOpt = ActorUtils::SafeGetPosition(a_actor);

            if (actorPosOpt.has_value()) {
                float distSq = playerPos.GetSquaredDistance(actorPosOpt.value());
                float nearDistSq = config.GetPerformance().distanceNear * config.GetPerformance().distanceNear;
                float midDistSq = config.GetPerformance().distanceMid * config.GetPerformance().distanceMid;

                if (distSq > midDistSq) {
                    // Far distance - slowest updates
                    targetInterval = config.GetPerformance().processingIntervalFar;
                } else if (distSq > nearDistSq) {
                    // Mid distance - medium updates
                    targetInterval = config.GetPerformance().processingIntervalMid;
                }
            }
        }

        // Only process if interval has passed
        if (*timerPtr < targetInterval) {
            return false; // Skip processing this frame
        }

        // Reset timer for next interval
        *timerPtr = 0.0f;

        return true;
    }
} // namespace CombatAI
