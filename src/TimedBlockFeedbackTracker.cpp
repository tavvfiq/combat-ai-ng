#include "pch.h"
#include "TimedBlockFeedbackTracker.h"
#include "ActorUtils.h"
#include "Logger.h"
#include <algorithm>

namespace CombatAI
{
    void TimedBlockFeedbackTracker::RecordTimedBlockAttempt(RE::Actor* a_defender, RE::Actor* a_target, float a_estimatedAttackDuration, float a_timeUntilHit)
    {
        if (!a_defender || !a_target) {
            return;
        }

        auto defenderFormIDOpt = ActorUtils::SafeGetFormID(a_defender);
        auto targetFormIDOpt = ActorUtils::SafeGetFormID(a_target);
        
        if (!defenderFormIDOpt.has_value() || !targetFormIDOpt.has_value()) {
            return;
        }

        RE::FormID defenderFormID = defenderFormIDOpt.value();
        RE::FormID targetFormID = targetFormIDOpt.value();

        TimedBlockAttempt attempt;
        attempt.defenderFormID = defenderFormID;
        attempt.targetFormID = targetFormID;
        attempt.estimatedAttackDuration = a_estimatedAttackDuration;
        attempt.timeUntilHit = a_timeUntilHit;
        attempt.attemptTime = std::chrono::steady_clock::now();
        attempt.matched = false;

        // Add to recent attempts (keyed by defender FormID for easy matching)
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto& attempts = attemptsMap[defenderFormID];
            attempts.push_back(attempt);
            
            // Keep only recent attempts (limit size)
            if (attempts.size() > MAX_ATTEMPTS_PER_DEFENDER) {
                attempts.erase(attempts.begin());
            }
        });

        // Update attempt count in feedback
        m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
            auto& feedback = feedbackMap[defenderFormID];
            feedback.timedBlockAttemptCount++;
            feedback.timeSinceLastTimedBlockAttempt = 0.0f;
            feedback.lastTimedBlockEstimatedDuration = a_estimatedAttackDuration;
        });
    }

    void TimedBlockFeedbackTracker::OnTimedBlockSuccess(RE::Actor* a_defender)
    {
        if (!a_defender) {
            return;
        }

        auto defenderFormIDOpt = ActorUtils::SafeGetFormID(a_defender);
        if (!defenderFormIDOpt.has_value()) {
            return;
        }

        RE::FormID defenderFormID = defenderFormIDOpt.value();

        // Find the most recent unmatched attempt for this defender
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto it = attemptsMap.find(defenderFormID);
            if (it == attemptsMap.end()) {
                return; // No attempts found for this defender
            }

            auto& attempts = it->second;
            
            // Find most recent unmatched attempt
            for (auto attemptIt = attempts.rbegin(); attemptIt != attempts.rend(); ++attemptIt) {
                if (!attemptIt->matched) {
                    // Found unmatched attempt - mark as matched
                    attemptIt->matched = true;
                    
                    // Update feedback for the defender
                    m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
                        auto& feedback = feedbackMap[defenderFormID];
                        feedback.lastTimedBlockSuccess = true;
                        feedback.timedBlockSuccessCount++;
                        feedback.lastTimedBlockEstimatedDuration = attemptIt->estimatedAttackDuration;
                    });

                    // Log timed block success match (commented out to avoid spam, enable for debugging)
                    // LOG_DEBUG("Timed block success matched: defender=0x{:08X}, target=0x{:08X}, estimatedDuration={}", 
                    //          static_cast<std::uint32_t>(defenderFormID),
                    //          static_cast<std::uint32_t>(attemptIt->targetFormID),
                    //          attemptIt->estimatedAttackDuration);
                    break;
                }
            }
        });
    }

    void TimedBlockFeedbackTracker::Update(float a_deltaTime)
    {
        auto now = std::chrono::steady_clock::now();

        // Clean up old attempts and update feedback timers
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            for (auto it = attemptsMap.begin(); it != attemptsMap.end(); ++it) {
                auto& attempts = it->second;
                // Remove old attempts
                attempts.erase(
                    std::remove_if(attempts.begin(), attempts.end(),
                        [&](const TimedBlockAttempt& attempt) {
                            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - attempt.attemptTime).count() / 1000.0f;
                            return age > MAX_ATTEMPT_AGE;
                        }),
                    attempts.end()
                );
            }

            // Remove empty entries
            auto mapIt = attemptsMap.begin();
            while (mapIt != attemptsMap.end()) {
                if (mapIt->second.empty()) {
                    mapIt = attemptsMap.erase(mapIt);
                } else {
                    ++mapIt;
                }
            }
        });

        // Update feedback timers
        m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
            for (auto it = feedbackMap.begin(); it != feedbackMap.end(); ++it) {
                it->second.timeSinceLastTimedBlockAttempt += a_deltaTime;
            }
        });
    }

    TimedBlockFeedbackTracker::TimedBlockFeedback TimedBlockFeedbackTracker::GetFeedback(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return TimedBlockFeedback();
        }

        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return TimedBlockFeedback();
        }

        auto feedbackOpt = m_feedbackData.Find(formIDOpt.value());
        if (feedbackOpt.has_value()) {
            return feedbackOpt.value();
        }

        return TimedBlockFeedback();
    }
}
