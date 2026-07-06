#include "Hooks.h"
#include "ActorUtils.h"
#include "CombatDirector.h"
#include "Logger.h"
#include "RE/Offsets.h"
#include "pch.h"

namespace CombatAI
{
    namespace Hooks
    {
        // Hook structure (inspired by ProjectGapClose)
        // Post-hook pattern: call original, then our logic
        struct Actor_Update
        {
            static void thunk(RE::Actor *a_actor, float a_delta)
            {
                // Call original function first
                func(a_actor, a_delta);

                CombatDirector::GetInstance().ProcessActor(a_actor, RE::GetSecondsSinceLastFrame());

                // Global systems must tick exactly once per frame, not once per actor.
                // This hook fires for EVERY actor each frame, so gate the global tick on
                // the player (whose Character::Update runs exactly once per frame) and use
                // the true frame delta. No accumulator, no per-actor multiplication - the
                // old accumulator ran Update() ~N times per frame (N = actor count),
                // advancing all timers/decay N times too fast.
                if (ActorUtils::SafeIsPlayerRef(a_actor)) {
                    CombatDirector::GetInstance().Update(RE::GetSecondsSinceLastFrame());
                }
            }

            static inline REL::Relocation<decltype(thunk)> func;
        };

        void Install()
        {
            LOG_INFO("Installing hooks...");

            // Get trampoline
            auto &trampoline = SKSE::GetTrampoline();
            trampoline.create(128);

            // Hook Character::Update at vtable index 0xAD (inspired by ProjectGapClose)
            // Using RE::Character instead of RE::Actor for more reliable hooking
            REL::Relocation<std::uintptr_t> CharacterVTable{RE::Character::VTABLE[0]};

            // Write our hook function to the vtable
            // write_vfunc returns the original function pointer
            Actor_Update::func = CharacterVTable.write_vfunc(0x0AD, Actor_Update::thunk);

            LOG_INFO("Hooks installed successfully");
        }
    } // namespace Hooks
} // namespace CombatAI
