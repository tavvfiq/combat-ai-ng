#include "pch.h"
#include "AttackDefenseFeedbackTracker.h"
#include "ActorUtils.h"
#include "Logger.h"
#include <algorithm>

namespace CombatAI
{
    void AttackDefenseFeedbackTracker::RecordAttackAttempt(RE::Actor* a_attacker, RE::Actor* a_target, bool a_isPowerAttack)
    {
        if (!a_attacker || !a_target) {
            return;
        }

        auto attackerFormIDOpt = ActorUtils::SafeGetFormID(a_attacker);
        auto targetFormIDOpt = ActorUtils::SafeGetFormID(a_target);
        
        if (!attackerFormIDOpt.has_value() || !targetFormIDOpt.has_value()) {
            return;
        }

        RE::FormID attackerFormID = attackerFormIDOpt.value();
        RE::FormID targetFormID = targetFormIDOpt.value();

        AttackAttempt attempt;
        attempt.attackerFormID = attackerFormID;
        attempt.targetFormID = targetFormID;
        attempt.isPowerAttack = a_isPowerAttack;
        attempt.attemptTime = std::chrono::steady_clock::now();
        attempt.matchedParry = false;
        attempt.matchedTimedBlock = false;
        attempt.matchedHit = false;

        size_t totalAttempts = 0;
        // Add to recent attempts (keyed by attacker FormID)
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto& attempts = attemptsMap[attackerFormID];
            attempts.push_back(attempt);
            totalAttempts = attempts.size();
            
            // Keep only recent attempts (limit size)
            if (attempts.size() > MAX_ATTEMPTS_PER_ATTACKER) {
                attempts.erase(attempts.begin());
            }
        });


        // Update attack count in feedback
        m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
            auto& feedback = feedbackMap[attackerFormID];
            feedback.totalAttackCount++;
            
            // Recalculate rates
            if (feedback.totalAttackCount > 0) {
                feedback.parryRate = static_cast<float>(feedback.parriedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                feedback.timedBlockRate = static_cast<float>(feedback.timedBlockedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                feedback.hitRate = static_cast<float>(feedback.hitAttackCount) / static_cast<float>(feedback.totalAttackCount);
                feedback.missRate = static_cast<float>(feedback.missedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                feedback.totalDefenseRate = feedback.parryRate + feedback.timedBlockRate;
            }
        });
    }

    void AttackDefenseFeedbackTracker::OnAttackParried(RE::Actor* a_attacker)
    {
        if (!a_attacker) {
            return;
        }

        auto attackerFormIDOpt = ActorUtils::SafeGetFormID(a_attacker);
        if (!attackerFormIDOpt.has_value()) {
            return;
        }

        RE::FormID attackerFormID = attackerFormIDOpt.value();

        bool foundMatch = false;
        // Find the most recent unmatched attack attempt for this attacker
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto it = attemptsMap.find(attackerFormID);
            if (it == attemptsMap.end()) {
                return; // No attempts found for this attacker
            }

            auto& attempts = it->second;
            
            // Find most recent unmatched attack attempt
            for (auto attemptIt = attempts.rbegin(); attemptIt != attempts.rend(); ++attemptIt) {
                // Parry and timed block are mutually exclusive - can't match if already matched as timed block
                // But allow matching if not matched yet (timed block mod events can override parry matches later)
                if (!attemptIt->matchedTimedBlock) {
                    // Found attempt that hasn't been matched as timed block yet
                    // Allow matching as parry (timed block mod events can override this later if they arrive)
                    bool wasAlreadyParryMatch = attemptIt->matchedParry;
                    attemptIt->matchedParry = true;
                    foundMatch = true;
                    
                    // Update feedback for the attacker
                    m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
                        auto& feedback = feedbackMap[attackerFormID];
                        feedback.lastAttackParried = true;
                        feedback.lastAttackTimedBlocked = false; // Reset timed block flag
                        feedback.lastAttackHit = false; // Reset hit flag (parry takes precedence)
                        feedback.lastAttackMissed = false; // Reset miss flag
                        feedback.timeSinceLastParriedAttack = 0.0f;
                        
                        // Only increment if not already matched (avoid double-counting)
                        if (!wasAlreadyParryMatch) {
                            feedback.parriedAttackCount++;
                        }
                        
                        // Recalculate rates
                        if (feedback.totalAttackCount > 0) {
                            feedback.parryRate = static_cast<float>(feedback.parriedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.timedBlockRate = static_cast<float>(feedback.timedBlockedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.hitRate = static_cast<float>(feedback.hitAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.missRate = static_cast<float>(feedback.missedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.totalDefenseRate = feedback.parryRate + feedback.timedBlockRate;
                        }
                    });

                    break;
                }
            }
        });
    }

    void AttackDefenseFeedbackTracker::OnAttackTimedBlocked(RE::Actor* a_attacker)
    {
        if (!a_attacker) {
            return;
        }

        auto attackerFormIDOpt = ActorUtils::SafeGetFormID(a_attacker);
        if (!attackerFormIDOpt.has_value()) {
            return;
        }

        RE::FormID attackerFormID = attackerFormIDOpt.value();

        // Find the most recent unmatched attack attempt for this attacker
        bool foundMatch = false;
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto it = attemptsMap.find(attackerFormID);
            if (it == attemptsMap.end()) {
                return; // No attempts found for this attacker
            }

            auto& attempts = it->second;
            
            // Find most recent attack attempt (allow overriding parry match if timed block mod event arrives)
            // Timed block mod events are authoritative - if they fire, it's a timed block, not a parry
            for (auto attemptIt = attempts.rbegin(); attemptIt != attempts.rend(); ++attemptIt) {
                if (!attemptIt->matchedTimedBlock) {
                    // Found attempt that hasn't been matched as timed block yet
                    // Allow overriding parry match if timed block mod event arrives (mod events are authoritative)
                    bool wasParryMatch = attemptIt->matchedParry;
                    attemptIt->matchedTimedBlock = true;
                    foundMatch = true;
                    
                    // Update feedback for the attacker
                    m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
                        auto& feedback = feedbackMap[attackerFormID];
                        feedback.lastAttackParried = false; // Reset parry flag (timed block takes precedence)
                        feedback.lastAttackTimedBlocked = true;
                        feedback.lastAttackHit = false; // Reset hit flag (timed block takes precedence)
                        feedback.lastAttackMissed = false; // Reset miss flag
                        feedback.timeSinceLastTimedBlockedAttack = 0.0f;
                        feedback.timedBlockedAttackCount++;
                        
                        // If this was previously matched as parry, decrement parry count
                        if (wasParryMatch) {
                            feedback.parriedAttackCount = std::max(0, feedback.parriedAttackCount - 1);
                        }
                        
                        // Recalculate rates
                        if (feedback.totalAttackCount > 0) {
                            feedback.parryRate = static_cast<float>(feedback.parriedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.timedBlockRate = static_cast<float>(feedback.timedBlockedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.hitRate = static_cast<float>(feedback.hitAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.missRate = static_cast<float>(feedback.missedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.totalDefenseRate = feedback.parryRate + feedback.timedBlockRate;
                        }
                    });

                    break;
                }
            }
        });
    }

    void AttackDefenseFeedbackTracker::OnAttackHit(RE::Actor* a_attacker, RE::Actor* a_target)
    {
        if (!a_attacker || !a_target) {
            return;
        }

        auto attackerFormIDOpt = ActorUtils::SafeGetFormID(a_attacker);
        auto targetFormIDOpt = ActorUtils::SafeGetFormID(a_target);
        if (!attackerFormIDOpt.has_value() || !targetFormIDOpt.has_value()) {
            return;
        }

        RE::FormID attackerFormID = attackerFormIDOpt.value();
        RE::FormID targetFormID = targetFormIDOpt.value();

        // Find the most recent unmatched attack attempt for this attacker-target pair
        bool foundMatch = false;
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            auto it = attemptsMap.find(attackerFormID);
            if (it == attemptsMap.end()) {
                return; // No attempts found for this attacker
            }

            auto& attempts = it->second;
            
            // Find most recent unmatched attempt matching this target
            // Hits can only match if not already matched as parry/timed block
            for (auto attemptIt = attempts.rbegin(); attemptIt != attempts.rend(); ++attemptIt) {
                if (attemptIt->targetFormID == targetFormID && 
                    !attemptIt->matchedParry && 
                    !attemptIt->matchedTimedBlock && 
                    !attemptIt->matchedHit) {
                    // Found unmatched attempt for this target
                    attemptIt->matchedHit = true;
                    foundMatch = true;
                    
                    // Update feedback for the attacker
                    m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
                        auto& feedback = feedbackMap[attackerFormID];
                        feedback.lastAttackHit = true;
                        feedback.lastAttackMissed = false;
                        feedback.lastAttackParried = false; // Reset parry flag
                        feedback.lastAttackTimedBlocked = false; // Reset timed block flag
                        feedback.timeSinceLastHitAttack = 0.0f;
                        feedback.hitAttackCount++;
                        
                        // Recalculate rates
                        if (feedback.totalAttackCount > 0) {
                            feedback.parryRate = static_cast<float>(feedback.parriedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.timedBlockRate = static_cast<float>(feedback.timedBlockedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.hitRate = static_cast<float>(feedback.hitAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.missRate = static_cast<float>(feedback.missedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                            feedback.totalDefenseRate = feedback.parryRate + feedback.timedBlockRate;
                        }
                    });

                    break;
                }
            }
        });
    }

    void AttackDefenseFeedbackTracker::Update(float a_deltaTime)
    {
        auto now = std::chrono::steady_clock::now();

        // Detect misses: unmatched attempts older than MISS_DETECTION_TIME
        static constexpr float MISS_DETECTION_TIME = 1.5f; // 1.5 seconds - if no hit/parry/timed block by then, consider it a miss
        
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            for (auto it = attemptsMap.begin(); it != attemptsMap.end(); ++it) {
                RE::FormID attackerFormID = it->first;
                auto& attempts = it->second;
                
                for (auto& attempt : attempts) {
                    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                        now - attempt.attemptTime).count() / 1000.0f;
                    
                    // If attempt is old enough and hasn't been matched, it's a miss
                    if (age >= MISS_DETECTION_TIME && 
                        !attempt.matchedParry && 
                        !attempt.matchedTimedBlock && 
                        !attempt.matchedHit) {
                        // Mark as matched to avoid double-counting
                        attempt.matchedHit = true; // Use matchedHit flag to mark as processed
                        
                        // Update feedback for the attacker
                        m_feedbackData.WithWriteLock([&](auto& feedbackMap) {
                            auto& feedback = feedbackMap[attackerFormID];
                            feedback.lastAttackMissed = true;
                            feedback.lastAttackHit = false;
                            feedback.timeSinceLastMissedAttack = 0.0f;
                            feedback.missedAttackCount++;
                            
                            // Recalculate rates
                            if (feedback.totalAttackCount > 0) {
                                feedback.parryRate = static_cast<float>(feedback.parriedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                                feedback.timedBlockRate = static_cast<float>(feedback.timedBlockedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                                feedback.hitRate = static_cast<float>(feedback.hitAttackCount) / static_cast<float>(feedback.totalAttackCount);
                                feedback.missRate = static_cast<float>(feedback.missedAttackCount) / static_cast<float>(feedback.totalAttackCount);
                                feedback.totalDefenseRate = feedback.parryRate + feedback.timedBlockRate;
                            }
                        });
                    }
                }
            }
        });

        // Clean up old attempts
        m_recentAttempts.WithWriteLock([&](auto& attemptsMap) {
            for (auto it = attemptsMap.begin(); it != attemptsMap.end(); ++it) {
                auto& attempts = it->second;
                // Remove old attempts
                attempts.erase(
                    std::remove_if(attempts.begin(), attempts.end(),
                        [&](const AttackAttempt& attempt) {
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
                it->second.timeSinceLastParriedAttack += a_deltaTime;
                it->second.timeSinceLastTimedBlockedAttack += a_deltaTime;
                it->second.timeSinceLastHitAttack += a_deltaTime;
                it->second.timeSinceLastMissedAttack += a_deltaTime;
            }
        });
    }

    AttackDefenseFeedbackTracker::AttackDefenseFeedback AttackDefenseFeedbackTracker::GetFeedback(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return AttackDefenseFeedback();
        }

        auto formIDOpt = ActorUtils::SafeGetFormID(a_actor);
        if (!formIDOpt.has_value()) {
            return AttackDefenseFeedback();
        }

        auto feedbackOpt = m_feedbackData.Find(formIDOpt.value());
        if (feedbackOpt.has_value()) {
            return feedbackOpt.value();
        }

        return AttackDefenseFeedback();
    }
}
