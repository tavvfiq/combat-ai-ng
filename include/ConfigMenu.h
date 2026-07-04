#pragma once

namespace CombatAI
{
    // Runtime configuration menu backed by SKSE Menu Framework 2.
    // Registers a page in the Mod Control Panel that edits the live Config in
    // memory, with Save-to-INI and Reload-from-INI. Self-disables when the
    // framework is not installed.
    class ConfigMenu
    {
      public:
        // Register the menu section/page. Safe to call unconditionally.
        static void Register();

      private:
        // SKSE Menu Framework render callback (must be __stdcall).
        // A member of ConfigMenu so it can access Config's private fields (friend).
        static void __stdcall Render();
    };
} // namespace CombatAI
