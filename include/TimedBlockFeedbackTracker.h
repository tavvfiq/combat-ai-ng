#pragma once

#include "RE/A/Actor.h"
#include "ThreadSafeMap.h"
#include <chrono>

namespace CombatAI
{
    // Tracks timed block attempts and matches them with Simple Timed Block mod events
    class TimedBlockFeedbackTracker
    {
    public:
        static TimedBlockFeedbackTracker& GetInstance()
        {
            static TimedBlockFeedbackTracker instance;
            return instance;
        }

        TimedBlockFeedbackTracker(const TimedBlockFeedbackTracker&) = delete;
        TimedBlockFeedbackTracker& operator=(const TimedBlockFeedbackTracker&) = delete;

        // Record a timed block attempt
        void RecordTimedBlockAttempt(RE::Actor* a_defender, RE::Actor* a_target, float a_estimatedAttackDuration, float a_timeUntilHit);

        // Handle Simple Timed Block mod callback event (called when timed block succeeds)
        // a_defender is the actor who successfully timed blocked
        void OnTimedBlockSuccess(RE::Actor* a_defender);

        // Update timers and clean up old attempts
        void Update(float a_deltaTime);

        // Get feedback data for an actor
        struct TimedBlockFeedback
        {
            bool lastTimedBlockSuccess = false;
            float lastTimedBlockEstimatedDuration = 0.0f;
            float timeSinceLastTimedBlockAttempt = 999.0f;
            int timedBlockSuccessCount = 0;
            int timedBlockAttemptCount = 0;
        };
        TimedBlockFeedback GetFeedback(RE::Actor* a_actor);

    private:
        TimedBlockFeedbackTracker() = default;
        ~TimedBlockFeedbackTracker() = default;

        // Structure to track a timed block attempt
        struct TimedBlockAttempt
        {
            RE::FormID defenderFormID; // Actor who attempted timed block
            RE::FormID targetFormID;   // Actor attacking
            float estimatedAttackDuration;
            float timeUntilHit;
            std::chrono::steady_clock::time_point attemptTime;
            bool matched = false; // Whether this attempt was matched with a timed block event
        };

        // Track recent timed block attempts (keyed by defender FormID)
        // We match attempts by finding the most recent attempt where defender == defender from event
        ThreadSafeMap<RE::FormID, std::vector<TimedBlockAttempt>> m_recentAttempts;

        // Track feedback per actor (keyed by defender FormID)
        ThreadSafeMap<RE::FormID, TimedBlockFeedback> m_feedbackData;

        // Maximum age for timed block attempts (clean up old attempts)
        static constexpr float MAX_ATTEMPT_AGE = 2.0f; // 2 seconds
        static constexpr size_t MAX_ATTEMPTS_PER_DEFENDER = 5; // Keep max 5 recent attempts per defender
    };
}
