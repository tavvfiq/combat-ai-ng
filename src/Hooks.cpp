#include "pch.h"
#include "Hooks.h"
#include "CombatDirector.h"
#include "RE/Offsets.h"

namespace CombatAI
{
    namespace Hooks
    {
        // Hook structure (inspired by ProjectGapClose)
        // Post-hook pattern: call original, then our logic
        struct Actor_Update
        {
            static void thunk(RE::Actor* a_actor, float a_delta)
            {
                // Call original function first
                func(a_actor, a_delta);
                
                // Accumulate delta time and call Update when threshold is reached
                static float accumulatedDelta = 0.0f;
                accumulatedDelta += a_delta;
                
                // Call Update approximately once per frame (~60 FPS = 0.016s)
                if (accumulatedDelta >= 0.016f) {
                    CombatDirector::GetInstance().Update(accumulatedDelta);
                    accumulatedDelta = 0.0f;
                }
                
                // Then run our custom logic per-actor (post-hook)
                if (a_actor) {
                    CombatDirector::GetInstance().ProcessActor(a_actor, RE::GetSecondsSinceLastFrame());
                }
            }
            
            static inline REL::Relocation<decltype(thunk)> func;
        };

        void Install()
        {
            LOG_INFO("Installing hooks...");

            // Get trampoline
            auto& trampoline = SKSE::GetTrampoline();
            trampoline.create(128);

            // Hook Character::Update at vtable index 0xAD (inspired by ProjectGapClose)
            // Using RE::Character instead of RE::Actor for more reliable hooking
            REL::Relocation<std::uintptr_t> CharacterVTable{ RE::Character::VTABLE[0] };
            
            // Write our hook function to the vtable
            // write_vfunc returns the original function pointer
            Actor_Update::func = CharacterVTable.write_vfunc(0x0AD, Actor_Update::thunk);

            LOG_INFO("Hooks installed successfully");
        }
    }
}
