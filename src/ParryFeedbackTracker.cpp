#include "pch.h"
#include "ParryFeedbackTracker.h"
#include "ActorUtils.h"
#include "Logger.h"
#include <algorithm>

namespace CombatAI
{
    void ParryFeedbackTracker::RecordParryAttempt(RE::Actor* a_parrier, RE::Actor* a_target, float a_estimatedAttackDuration, float a_timeUntilHit)
    {
        if (!a_parrier || !a_target) {
            return;
        }

        auto parrierFormIDOpt = ActorUtils::SafeGetFormID(a_parrier);
        auto targetFormIDOpt = ActorUtils::SafeGetFormID(a_target);
        
        if (!parrierFormIDOpt.has_value() || !targetFormIDOpt.has_value()) {
            return;
        }

        RE::FormID parrierFormID = parrierFormIDOpt.value();
        RE::FormID targetFormID = targetFormIDOpt.value();

        ParryAttempt attempt;
        attempt.parrierFormID = parrierFormID;
        attempt.targetFormID = targetFormID;
        attempt.estimatedAttackDuration = a_estimatedAttackDuration;
        attempt.timeUntilHit = a_timeUntilHit;
        attempt.attemptTime = std::chrono::steady_clock::now();
        attempt.matched = false;

        // Add to recent attempts (keyed by target FormID for easy matching)
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto& attempts = attemptsMap[targetFormID];
            attempts.push_back(attempt);
            
            // Keep only recent attempts (limit size)
            if (attempts.size() > MAX_ATTEMPTS_PER_TARGET) {
                attempts.erase(attempts.begin());
            }
        });

        // Update attempt count in feedback
        m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
            auto& feedback = feedbackMap[parrierFormID];
            feedback.parryAttemptCount++;
            feedback.timeSinceLastParryAttempt = 0.0f;
            feedback.lastParryEstimatedDuration = a_estimatedAttackDuration;
        });
    }

    void ParryFeedbackTracker::OnParrySuccess(RE::Actor* a_attacker)
    {
        if (!a_attacker) {
            return;
        }

        auto attackerFormIDOpt = ActorUtils::SafeGetFormID(a_attacker);
        if (!attackerFormIDOpt.has_value()) {
            return;
        }

        RE::FormID attackerFormID = attackerFormIDOpt.value();

        // Find the most recent unmatched attempt for this attacker (they were the target)
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto it = attemptsMap.find(attackerFormID);
            if (it == attemptsMap.end()) {
                return; // No attempts found for this attacker
            }

            auto& attempts = it->second;
            
            // Find most recent unmatched attempt
            for (auto attemptIt = attempts.rbegin(); attemptIt != attempts.rend(); ++attemptIt) {
                if (!attemptIt->matched) {
                    // Found unmatched attempt - mark as matched
                    attemptIt->matched = true;
                    
                    // Update feedback for the parrier
                    m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
                        auto& feedback = feedbackMap[attemptIt->parrierFormID];
                        feedback.lastParrySuccess = true;
                        feedback.parrySuccessCount++;
                        feedback.lastParryEstimatedDuration = attemptIt->estimatedAttackDuration;
                    });

                    // Log parry success match (commented out to avoid spam, enable for debugging)
                    // LOG_DEBUG("Parry success matched: parrier=0x{:08X}, target=0x{:08X}, estimatedDuration={}", 
                    //          static_cast<std::uint32_t>(attemptIt->parrierFormID),
                    //          static_cast<std::uint32_t>(attackerFormID),
                    //          attemptIt->estimatedAttackDuration);
                    break;
                }
            }
        });
    }

    void ParryFeedbackTracker::Update(float a_deltaTime)
    {
        auto now = std::chrono::steady_clock::now();

        // Clean up old attempts and update feedback timers
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            for (auto it = attemptsMap.begin(); it != attemptsMap.end(); ++it) {
                auto& attempts = it->second;
                // Remove old attempts
                attempts.erase(
                    std::remove_if(attempts.begin(), attempts.end(),
                        [&](const ParryAttempt& attempt) {
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
                it->second.timeSinceLastParryAttempt += a_deltaTime;
            }
        });
    }

    ParryFeedbackTracker::ParryFeedback ParryFeedbackTracker::GetFeedback(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return ParryFeedback();
        }

        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return ParryFeedback();
        }

        auto feedbackOpt = m_feedbackData.Find(formIDOpt.value());
        if (feedbackOpt.has_value()) {
            return feedbackOpt.value();
        }

        return ParryFeedback();
    }
}
