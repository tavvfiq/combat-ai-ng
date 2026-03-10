#pragma once

#include "pch.h"
#include <mutex>
#include <unordered_set>

namespace CombatAI
{
    // Manages WaitYourTurn AI package overrides for held-back NPCs.
    //
    // When an NPC is held back by the pacing system, we call
    //   ActorUtil.AddPackageOverride(npc, WYT_CirclePackage, 100, 1)
    // which forces Skyrim's own AI to run the WYT circling behaviour.
    // When the slot opens again we remove the override and the NPC re-engages.
    //
    // Requires WaitYourTurn.esp to be installed. Falls back gracefully if absent.
    class PacingPackageManager
    {
      public:
        static PacingPackageManager &GetInstance()
        {
            static PacingPackageManager instance;
            return instance;
        }

        PacingPackageManager(const PacingPackageManager &) = delete;
        PacingPackageManager &operator=(const PacingPackageManager &) = delete;

        // Call at kDataLoaded — looks up WaitYourTurn.esp and its circle package.
        // Returns true if WYT is available and the package was found.
        bool Initialize();

        // Returns true if WYT is available and the package was located.
        bool IsAvailable() const { return m_package != nullptr; }

        // Force the NPC into the circle package (held-back state).
        // Safe to call if already locked — no double-lock.
        void LockEnemy(RE::Actor *a_actor);

        // Remove the circle package override, returning the NPC to combat AI.
        // Safe to call if not locked — no-op.
        void UnlockEnemy(RE::Actor *a_actor);

        // Unlock all currently locked NPCs (call on combat end / cleanup).
        void UnlockAll();

      private:
        PacingPackageManager() = default;
        ~PacingPackageManager() = default;

        // WYT_CirclePackage — form 0x800 in WaitYourTurn.esp
        static constexpr RE::FormID kWYTCirclePackageLocalID = 0x800;
        static constexpr const char *kWYTPluginName = "WaitYourTurn.esp";
        static constexpr int kPackagePriority = 100;

        RE::TESPackage *m_package = nullptr; // null if WYT not installed

        using Lock = std::mutex;
        using Locker = std::lock_guard<Lock>;
        mutable Lock m_lock;
        std::unordered_set<RE::FormID> m_locked; // FormIDs of currently locked NPCs
    };
} // namespace CombatAI
