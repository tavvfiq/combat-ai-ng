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
                
                // Then run our custom logic (post-hook)
                if (a_actor) {
                    CombatDirector::GetInstance().ProcessActor(a_actor, a_delta);
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
            REL::Relocation<std::uintptr_t> CharacterVTable{ RE::VTABLE_Character[0] };
            
            // Write our hook function to the vtable
            // write_vfunc returns the original function pointer
            std::uintptr_t originalAddr = CharacterVTable.write_vfunc(0xAD, 
                reinterpret_cast<std::uintptr_t>(Actor_Update::thunk));
            
            // Store original function pointer
            Actor_Update::func = reinterpret_cast<decltype(Actor_Update::thunk)>(originalAddr);

            LOG_INFO("Hooks installed successfully");
        }
    }
}
