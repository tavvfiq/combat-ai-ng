#pragma once

#include "pch.h"

namespace CombatAI
{
    void ConsolePrint(const char *a_fmt, ...) {
        auto console = RE::ConsoleLog::GetSingleton();
        if (console) {
            std::va_list args;
            va_start(args, a_fmt);
            console->VPrint(a_fmt, args);
            va_end(args);
        }
    }
}