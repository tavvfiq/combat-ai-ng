#pragma once

#include "RE/B/BSTEvent.h"
#include "SKSE/SKSE.h"
#include "RE/T/TESHitEvent.h"

namespace CombatAI
{
    // Event sink for EldenParry mod callback events
    class EldenParryEventSink : public RE::BSTEventSink<SKSE::ModCallbackEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, [[maybe_unused]] RE::BSTEventSource<SKSE::ModCallbackEvent>* a_eventSource) override;
    };

    // Event sink for Simple Timed Block mod callback events
    class TimedBlockEventSink : public RE::BSTEventSink<SKSE::ModCallbackEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* a_event, [[maybe_unused]] RE::BSTEventSource<SKSE::ModCallbackEvent>* a_eventSource) override;
    };

    // Event sink for TESHitEvent to detect when NPC attacks successfully hit the player
    class AttackHitEventSink : public RE::BSTEventSink<RE::TESHitEvent>
    {
    public:
        RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, [[maybe_unused]] RE::BSTEventSource<RE::TESHitEvent>* a_eventSource) override;
    };
}
