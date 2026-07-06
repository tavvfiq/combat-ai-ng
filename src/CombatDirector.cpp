#include "CombatDirector.h"
#include "APIManager.h"
#include "ActorUtils.h"
#include "AttackCoordinator.h"
#include "AttackDefenseFeedbackTracker.h"
#include "Config.h"
#include "GuardCounterFeedbackTracker.h"
#include "Logger.h"
#include "ModEventSinks.h"
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

        // Apply processing interval + humanizer values from config
        ApplyConfig();

        // Register mod callback listeners (for EldenParry integration)
        RegisterModCallbacks();
    }

    void CombatDirector::ApplyConfig()
    {
        auto &config = Config::GetInstance();

        // Set processing interval
        m_processInterval = config.GetGeneral().processingInterval;

        // Push humanizer config values into the humanizer (copied by value)
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

        // Re-apply humanizer config on the game thread if the runtime menu changed it.
        if (config.ConsumeHumanizerDirty()) {
            ApplyConfig();
        }

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

        // Check if actor can react (reaction delay). Bail out before the expensive
        // state gathering and decision evaluation when the actor is still within its
        // reaction latency window - there is nothing we could act on this frame anyway.
        if (!m_humanizer.CanReact(a_actor, a_deltaTime)) {
            return; // can't react yet
        }

        // Gather state and evaluate the decision matrix (the heavy per-actor work)
        ActorStateData state = m_observer.GatherState(a_actor, a_deltaTime);
        DecisionResult decision = m_decisionMatrix.Evaluate(a_actor, state);

        // Combat pacing ("Wait Your Turn"): limit concurrent attackers on a target. An
        // actor must hold an attack slot to commit; otherwise it's rerouted to circle
        // and wait its turn.
        const auto &pacing = config.GetCombatPacing();
        if (pacing.enableCombatPacing && state.target.isValid && state.target.targetFormID != 0) {
            auto selfIDOpt = ActorUtils::SafeGetFormID(a_actor);
            RE::FormID selfID = selfIDOpt.has_value() ? selfIDOpt.value() : 0;
            auto &coordinator = AttackCoordinator::GetSingleton();

            bool isCommit = decision.action == ActionType::Attack || decision.action == ActionType::PowerAttack ||
                            decision.action == ActionType::SprintAttack;
            bool paced = isCommit && (!pacing.paceTargetPlayerOnly || state.target.isPlayer);

            if (paced && selfID != 0) {
                if (!coordinator.TryAcquire(state.target.targetFormID, selfID, pacing.maxSimultaneousAttackers,
                                            pacing.slotWindowMinSeconds, pacing.slotWindowMaxSeconds)) {
                    // No slot - hold back and circle instead of attacking
                    decision = m_decisionMatrix.EvaluateHeldBack(a_actor, state);
                    if (debugEnabled) {
                        LOG_DEBUG("Pacing: attack slot full - holding back (action now {})",
                                  static_cast<int>(decision.action));
                    }
                }
            } else if (selfID != 0) {
                // Not committing to an attack on this target - free any slot we held
                coordinator.Release(state.target.targetFormID, selfID);
            }
        }

        if (debugEnabled) {
            LOG_DEBUG("Decision: action={} priority={:.2f}", static_cast<int>(decision.action), decision.priority);
        }

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
            }
        }
    }

    void CombatDirector::Update(float a_deltaTime)
    {
        // Update per-frame systems (must be called once per frame, not per actor)
        m_humanizer.Update(a_deltaTime);
        m_observer.Update(a_deltaTime);
        AttackCoordinator::GetSingleton().Update(a_deltaTime);

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

        // Drop empty attack-slot buckets (slots themselves expire by their window)
        AttackCoordinator::GetSingleton().Cleanup();

        // Clean up spawn times for actors no longer in combat
        // This is done lazily in Update(), but we can also clean up here if needed
        // (Spawn times are cleaned up automatically in Update() after warmup period)

        // If we want to be more aggressive, we could add a size limit and remove
        // oldest entries But for now, lazy cleanup is safer and more efficient
    }

    void CombatDirector::EvictActor(RE::FormID a_formID)
    {
        if (a_formID == 0) {
            return;
        }
        m_actorProcessTimers.Erase(a_formID);
        m_actorSpawnTimes.Erase(a_formID);
        m_processedActors.Erase(a_formID);
        m_observer.EvictActor(a_formID);
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
        RE::FormID formID = formIDOpt.value();

        // Validate actor using safe wrappers - protects against transitional states
        // Actor is passed directly from hook, but could become invalid at any time

        // Quick validation - if actor is dead or not in combat, skip early. Evict any
        // per-actor state so a recycled temporary FormID (0xFF...) starts fresh and the
        // bookkeeping maps stay bounded.
        bool isDead = ActorUtils::SafeIsDead(a_actor);
        bool inCombat = ActorUtils::SafeIsInCombat(a_actor);
        if (isDead || !inCombat) {
            EvictActor(formID);
            return false;
        }

        // Liveness guard: an actor can be in combat yet mid-teardown (3D unloaded,
        // deleted, or disabled). Processing it walks transient/freed game state in the
        // heavier gather path. Skip and evict so recycled FormIDs don't inherit state.
        if (!ActorUtils::SafeIs3DLoaded(a_actor) || ActorUtils::SafeIsDeleted(a_actor) ||
            ActorUtils::SafeIsDisabled(a_actor)) {
            EvictActor(formID);
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
        // yet. The read-modify-write is done atomically under the map's lock.
        float currentSpawnTime = 0.0f;
        m_actorSpawnTimes.Modify(formID, [&](float &spawnTime) {
            spawnTime += a_deltaTime;
            currentSpawnTime = spawnTime;
        });

        // Check if actor is still in warmup period
        if (currentSpawnTime < SPAWN_WARMUP_DELAY) {
            // Actor is still warming up, don't process yet
            return false;
        }

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

        // Update the per-actor throttle timer atomically under the map's lock and
        // decide whether enough time has elapsed to process this actor this frame.
        bool shouldProcess = false;
        bool existed = m_actorProcessTimers.Modify(formID, [&](float &timer) {
            timer += a_deltaTime;
            if (timer >= targetInterval) {
                timer = 0.0f; // Reset timer for next interval
                shouldProcess = true;
            }
        });

        if (!existed) {
            // First time processing this actor, initialize timer and allow processing
            m_actorProcessTimers.Emplace(formID, 0.0f);
            return true;
        }

        return shouldProcess;
    }
} // namespace CombatAI
