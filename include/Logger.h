#pragma once

#include "pch.h"

namespace logger = SKSE::log;

namespace CombatAI
{
    // Convenience macros
    #define LOG_INFO(...) logger::info(__VA_ARGS__)
    #define LOG_ERROR(...) logger::error(__VA_ARGS__)
    #define LOG_WARN(...) logger::warn(__VA_ARGS__)
    #define LOG_DEBUG(...) logger::debug(__VA_ARGS__)

    void ConsolePrint(const char* a_fmt, ...);
}
