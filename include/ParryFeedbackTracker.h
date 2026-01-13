#pragma once

#include "RE/A/Actor.h"
#include "ThreadSafeMap.h"
#include <chrono>

namespace CombatAI
{
    // Tracks bash attempts for parrying and matches them with EldenParry events
    class ParryFeedbackTracker
    {
    public:
        static ParryFeedbackTracker& GetInstance()
        {
            static ParryFeedbackTracker instance;
            return instance;
        }

        ParryFeedbackTracker(const ParryFeedbackTracker&) = delete;
        ParryFeedbackTracker& operator=(const ParryFeedbackTracker&) = delete;

        // Record a bash attempt for parrying
        void RecordParryAttempt(RE::Actor* a_parrier, RE::Actor* a_target, float a_estimatedAttackDuration, float a_timeUntilHit);

        // Handle EldenParry mod callback event (called when parry succeeds)
        void OnParrySuccess(RE::Actor* a_attacker);

        // Update timers and clean up old attempts
        void Update(float a_deltaTime);

        // Get feedback data for an actor
        struct ParryFeedback
        {
            bool lastParrySuccess = false;
            float lastParryEstimatedDuration = 0.0f;
            float timeSinceLastParryAttempt = 999.0f;
            int parrySuccessCount = 0;
            int parryAttemptCount = 0;
        };
        ParryFeedback GetFeedback(RE::Actor* a_actor);

    private:
        ParryFeedbackTracker() = default;
        ~ParryFeedbackTracker() = default;

        // Structure to track a parry attempt
        struct ParryAttempt
        {
            RE::FormID parrierFormID; // Actor who attempted parry
            RE::FormID targetFormID;  // Actor being parried
            float estimatedAttackDuration;
            float timeUntilHit;
            std::chrono::steady_clock::time_point attemptTime;
            bool matched = false; // Whether this attempt was matched with a parry event
        };

        // Track recent parry attempts (keyed by target FormID, since event gives us attacker)
        // We match attempts by finding the most recent attempt where target == attacker from event
        ThreadSafeMap<RE::FormID, std::vector<ParryAttempt>> m_recentAttempts;

        // Track feedback per actor (keyed by parrier FormID)
        ThreadSafeMap<RE::FormID, ParryFeedback> m_feedbackData;

        // Maximum age for parry attempts (clean up old attempts)
        static constexpr float MAX_ATTEMPT_AGE = 2.0f; // 2 seconds
        static constexpr size_t MAX_ATTEMPTS_PER_TARGET = 5; // Keep max 5 recent attempts per target
    };
}
