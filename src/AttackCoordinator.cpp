#include "AttackCoordinator.h"
#include "Logger.h"

#include <random>

namespace CombatAI
{
    namespace
    {
        // Thread-local RNG for slot window jitter (cheap, no shared state).
        float RandRange(float a_min, float a_max)
        {
            if (a_max <= a_min) {
                return a_min;
            }
            static thread_local std::mt19937 gen{std::random_device{}()};
            std::uniform_real_distribution<float> dist(a_min, a_max);
            return dist(gen);
        }
    } // namespace

    bool AttackCoordinator::TryAcquire(RE::FormID a_target, RE::FormID a_attacker, int a_maxSlots, float a_windowMin,
                                       float a_windowMax)
    {
        if (a_target == 0 || a_attacker == 0) {
            return true; // Can't pace without valid ids - allow the attack
        }
        if (a_maxSlots <= 0) {
            return false; // Pacing set to block all attackers
        }

        const float now = m_now.load();
        bool granted = false;

        m_slots.ModifyOrCreate(a_target, [&](SlotBucket &bucket) {
            // Rotate out expired slots so waiting attackers get their turn.
            for (auto it = bucket.begin(); it != bucket.end();) {
                if (now - it->second.acquireTime > it->second.window) {
                    it = bucket.erase(it);
                } else {
                    ++it;
                }
            }

            auto existing = bucket.find(a_attacker);
            if (existing != bucket.end()) {
                // Still within its window - keep attacking. Do NOT refresh acquireTime,
                // otherwise a holder that keeps attacking would never rotate out.
                granted = true;
                return;
            }

            if (static_cast<int>(bucket.size()) < a_maxSlots) {
                bucket.emplace(a_attacker, SlotInfo{now, RandRange(a_windowMin, a_windowMax)});
                granted = true;
            }
        });

        return granted;
    }

    void AttackCoordinator::Release(RE::FormID a_target, RE::FormID a_attacker)
    {
        if (a_target == 0 || a_attacker == 0) {
            return;
        }
        m_slots.Modify(a_target, [&](SlotBucket &bucket) { bucket.erase(a_attacker); });
    }

    void AttackCoordinator::Cleanup()
    {
        // Drop empty buckets so the map doesn't grow unbounded across encounters.
        m_slots.WithWriteLock([](std::unordered_map<RE::FormID, SlotBucket> &map) {
            for (auto it = map.begin(); it != map.end();) {
                if (it->second.empty()) {
                    it = map.erase(it);
                } else {
                    ++it;
                }
            }
        });
    }

    void AttackCoordinator::Reset() { m_slots.Clear(); }
} // namespace CombatAI
