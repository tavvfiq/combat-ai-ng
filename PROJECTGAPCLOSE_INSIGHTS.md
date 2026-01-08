# ProjectGapClose Insights

## Overview
This document summarizes insights gained from analyzing ProjectGapClose, particularly regarding power attack detection and movement control.

## Key Findings

### 1. Power Attack Detection

**Proper Method:**
```cpp
bool isPowerAttacking(RE::Actor* a_actor)
{
    auto currentProcess = a_actor->GetActorRuntimeData().currentProcess;
    if (currentProcess) {
        auto highProcess = currentProcess->high;
        if (highProcess) {
            auto attackData = highProcess->attackData;
            if (attackData) {
                auto flags = attackData->data.flags;
                return flags.any(RE::AttackData::AttackFlag::kPowerAttack);
            }
        }
    }
    return false;
}
```

**Key Points:**
- Access `HighProcessData` via `currentProcess->high`
- `attackData` is a `NiPointer<BGSAttackData>` at offset 0x258
- Check `attackData->data.flags` for `kPowerAttack` flag
- This is the **reliable** way to detect power attacks

### 2. Movement Control via Animation Graph Variables

**ProjectGapClose Approach:**
Instead of directly controlling movement, they use animation graph variables:

- `CPR_EnableCircling` (bool) - Enable circling behavior
- `CPR_EnableAdvanceRadius` (bool) - Enable advancing
- `CPR_EnableBackoff` (bool) - Enable backoff
- `CPR_EnableFallback` (bool) - Enable fallback
- `CPR_CirclingAngleMin` (float) - Minimum circling angle
- `CPR_CirclingAngleMax` (float) - Maximum circling angle
- `CPR_CirclingDistMax` (float) - Maximum circling distance
- `CPR_InnerRadiusMax` (float) - Inner radius for advancing
- `CPR_OuterRadiusMin/Mid/Max` (float) - Outer radius values
- `CPR_BackoffMinDistMult` (float) - Backoff distance multiplier
- `CPR_BackoffChance` (float) - Backoff probability
- `CPR_FallbackDistMin/Max` (float) - Fallback distance range
- `CPR_FallbackWaitTimeMin/Max` (float) - Fallback wait time

**How It Works:**
1. Set boolean flags to enable behaviors
2. Set float variables to configure behavior parameters
3. Animation graph reads these variables and controls movement
4. This works **with** vanilla AI, not against it

### 3. Sprint Attack Implementation

**Sprint Start:**
```cpp
if(!a_actor->AsActorState()->IsSprinting()){
    a_actor->NotifyAnimationGraph("SprintStart");
    a_actor->AsActorState()->actorState1.sprinting = 1;
}
```

**Sprint Attack:**
```cpp
a_actor->NotifyAnimationGraph("MCO_SprintPowerAttackInitiate");
a_actor->NotifyAnimationGraph("attackStartSprint");
a_actor->NotifyAnimationGraph("attackStart");
```

**Key Points:**
- Use `SprintStart` animation event to start sprinting
- Directly set `actorState1.sprinting = 1` for immediate effect
- Use `attackStartSprint` for sprint attacks
- Use `MCO_SprintPowerAttackInitiate` for sprint power attacks (MCO integration)

### 4. Actor::Update Hook

**Proper Hook Method:**
```cpp
struct Actor_Update
{
    static void thunk(RE::Actor* a_actor, float a_delta)
    {
        func(a_actor, a_delta);
        GetSingleton()->Update(a_actor, g_deltaTime);
    }
    static inline REL::Relocation<decltype(thunk)> func;
};

static void Install_Update(){
    stl::write_vfunc<RE::Character, 0xAD, Actor_Update>();
}
```

**Key Points:**
- Hook `RE::Character` (not `RE::Actor`) at vtable index 0xAD
- Use `stl::write_vfunc` helper from CommonLibSSE-NG
- Post-hook pattern (call original, then custom logic)
- Access delta time via global: `RELOCATION_ID(523660, 410199)`

### 5. Animation Graph Variables for State

**Combat State:**
- `bPGC_IsInCombat` (bool) - Track combat state
- Set via `TESCombatEvent` listener

**Casting State:**
- `IsCastingRight` (bool)
- `IsCastingLeft` (bool)
- `IsCastingDual` (bool)

**Other Useful Variables:**
- `bAnimationDriven` (bool) - Check if animation-driven
- `bInJumpState` (bool) - Check if jumping
- `bIsDodging` (bool) - Check if dodging (TK Dodge)

### 6. Threat and Survival Calculations

**ProjectGapClose calculates:**
- `personal_threatRatio` - Personal threat vs target threat
- `group_threatRatio` - Team threat vs enemy team threat
- `personal_survivalRatio` - Personal survival calculation

**Uses these for:**
- Enabling/disabling circling based on threat
- Enabling advancing when group threat is high
- Enabling fallback when survival ratio is low

## Integration Opportunities

### For Our Project

1. **Power Attack Detection** ✅
   - Already implemented using proper method
   - Uses `HighProcessData->attackData->data.flags`

2. **Movement Control** (Potential Enhancement)
   - Could use animation graph variables like ProjectGapClose
   - Would require animation graph support
   - Alternative: Continue using dodge system for evasion

3. **Sprint Attack** (Future Feature)
   - Could add sprint attack capability
   - Useful for gap closing when target is far

4. **Threat-Based Decisions** (Future Enhancement)
   - Could incorporate threat calculations
   - More sophisticated decision making

## Notes

- ProjectGapClose uses animation graph variables extensively
- This approach works **with** vanilla AI, not against it
- Requires animation graph to support the variables
- More flexible than direct movement control
