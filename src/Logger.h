#pragma once

#include "SKSE/Logger.h"

namespace CombatAI
{
    inline auto& GetLogger()
    {
        static auto logger = SKSE::log::logger();
        return logger;
    }

    // Convenience macros
    #define LOG_INFO(...) CombatAI::GetLogger()->info(__VA_ARGS__)
    #define LOG_ERROR(...) CombatAI::GetLogger()->error(__VA_ARGS__)
    #define LOG_WARN(...) CombatAI::GetLogger()->warn(__VA_ARGS__)
    #define LOG_DEBUG(...) CombatAI::GetLogger()->debug(__VA_ARGS__)
}
