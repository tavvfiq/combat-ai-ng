#pragma once

#include "ThreadSafeMap.h"
#include "pch.h"
#include <unordered_map>

namespace CombatAI
{
    // Attack-slot pacing ("Wait Your Turn"): limits how many attackers may commit an
    // attack on the same target at once. Others are held back to circle/wait until a
    // slot frees. Slots expire after a randomized window so attackers rotate.
    //
    // Thread-safe: combat hooks run on multiple threads. Keyed by target FormID; for
    // player-only pacing the key is simply the player's FormID (0x14).
    class AttackCoordinator
    {
      public:
        static AttackCoordinator &GetSingleton()
        {
            static AttackCoordinator instance;
            return instance;
        }

        AttackCoordinator(const AttackCoordinator &) = delete;
        AttackCoordinator &operator=(const AttackCoordinator &) = delete;

        // Try to acquire (or keep) an attack slot on a_target for a_attacker.
        // Returns true if the attacker holds a slot this frame (may attack), false if it
        // must wait. Expired slots are rotated out so waiting attackers get a turn.
        bool TryAcquire(RE::FormID a_target, RE::FormID a_attacker, int a_maxSlots, float a_windowMin, float a_windowMax);

        // Free the slot a_attacker holds on a_target (call when it stops attacking it).
        void Release(RE::FormID a_target, RE::FormID a_attacker);

        // Advance the internal clock (call once per frame from CombatDirector::Update).
        void Update(float a_deltaTime) { m_now.store(m_now.load() + a_deltaTime); }

        // Drop empty target buckets (call periodically from CombatDirector::Cleanup).
        void Cleanup();

        // Wipe all slots (e.g. on load).
        void Reset();

      private:
        AttackCoordinator() = default;
        ~AttackCoordinator() = default;

        struct SlotInfo
        {
            float acquireTime = 0.0f;
            float window = 0.0f; // how long this slot is held before it can rotate out
        };

        // For each target: which attackers currently hold a slot.
        using SlotBucket = std::unordered_map<RE::FormID, SlotInfo>;

        ThreadSafeMap<RE::FormID, SlotBucket> m_slots;
        std::atomic<float> m_now{0.0f};
    };
} // namespace CombatAI
