# Hook Pattern: Pre-Hook vs Post-Hook

## Overview

We switched from a **pre-hook pattern** (as originally planned in `brief.md`) to a **post-hook pattern** (as implemented, inspired by ProjectGapClose). This document explains why and the benefits.

## Pattern Comparison

### Pre-Hook Pattern (Original Plan)
```cpp
void ActorUpdate_Hook(RE::Actor* a_actor, float a_delta)
{
    // 1. Run our custom logic FIRST
    if (a_actor) {
        CombatDirector::ProcessActor(a_actor, a_delta);
    }
    
    // 2. Then call vanilla function
    ActorUpdate_Original(a_actor, a_delta);
}
```

### Post-Hook Pattern (Current Implementation)
```cpp
void ActorUpdate_Hook(RE::Actor* a_actor, float a_delta)
{
    // 1. Call vanilla function FIRST
    func(a_actor, a_delta);
    
    // 2. Then run our custom logic
    if (a_actor) {
        CombatDirector::ProcessActor(a_actor, a_delta);
    }
}
```

## Why Post-Hook is Better

### 1. **State Consistency** ✅

**Post-Hook Benefit:**
- Vanilla `Actor::Update()` has already processed the actor
- All actor state is fully updated (position, animation, combat state, etc.)
- We work with **current, accurate state** rather than potentially stale state

**Pre-Hook Problem:**
- We run before vanilla processing
- Actor state might not be fully updated yet
- We might make decisions based on outdated information

**Example:**
```cpp
// Post-hook: Actor position is already updated by vanilla Update()
float distance = actor->GetPosition().GetDistance(target->GetPosition());
// This distance is accurate!

// Pre-hook: Actor position might not be updated yet
float distance = actor->GetPosition().GetDistance(target->GetPosition());
// This distance might be from last frame!
```

### 2. **Less Interference** ✅

**Post-Hook Benefit:**
- Vanilla AI runs normally without interference
- We enhance behavior rather than replace it
- More compatible with other mods
- Less likely to break vanilla functionality

**Pre-Hook Problem:**
- We run before vanilla, potentially interfering with its logic
- If we modify state, vanilla might not work correctly
- Higher risk of conflicts with other mods

**Example:**
```cpp
// Post-hook: Vanilla AI already decided what to do
// We just add our tactical enhancements on top
if (shouldBash) {
    actor->NotifyAnimationGraph("bashStart");
}

// Pre-hook: We might prevent vanilla from doing something
// Then vanilla tries to do it anyway, causing conflicts
if (shouldBash) {
    actor->NotifyAnimationGraph("bashStart");
    // But vanilla might also try to attack, causing animation conflicts
}
```

### 3. **Animation State Accuracy** ✅

**Post-Hook Benefit:**
- Animation states are fully updated by vanilla
- We can accurately check if actor is attacking, blocking, etc.
- Better decision making based on current animation state

**Pre-Hook Problem:**
- Animation states might not be updated yet
- We might think actor is idle when they're actually attacking
- Decisions based on incorrect animation state

**Example:**
```cpp
// Post-hook: Animation state is accurate
RE::ActorState* state = actor->AsActorState();
if (state->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone) {
    // This is accurate - actor is truly idle
    ExecuteBash(actor);
}

// Pre-hook: Animation state might be stale
RE::ActorState* state = actor->AsActorState();
if (state->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone) {
    // But vanilla Update() might change this in the next line!
    ExecuteBash(actor); // Might conflict with vanilla's attack
}
```

### 4. **Combat Controller State** ✅

**Post-Hook Benefit:**
- Combat controller state is fully updated
- Target information is current
- Distance calculations are accurate

**Pre-Hook Problem:**
- Combat controller might not have updated target yet
- Distance calculations might be off
- Target state might be stale

### 5. **Enhancement vs Replacement** ✅

**Post-Hook Philosophy:**
- We **enhance** vanilla AI, not replace it
- Vanilla AI does its thing, we add tactical layer on top
- Works **with** vanilla, not **against** it

**Pre-Hook Philosophy:**
- We **intercept** before vanilla runs
- Might need to suppress vanilla behavior
- Works **against** vanilla, potentially conflicting

**Our Goal:**
> "Enhance NPC combat behavior based on combat styles, not suppress vanilla AI"

This aligns perfectly with post-hook pattern!

### 6. **Proven Pattern** ✅

**ProjectGapClose uses post-hook:**
- ProjectGapClose is a proven, working mod
- Uses post-hook pattern successfully
- We followed their approach for reliability

**Example from ProjectGapClose:**
```cpp
struct Actor_Update
{
    static void thunk(RE::Actor* a_actor, float a_delta)
    {
        func(a_actor, a_delta);  // Call original first
        GetSingleton()->Update(a_actor, g_deltaTime);  // Then custom logic
    }
};
```

### 7. **Better for CPR/BFCO Integration** ✅

**Post-Hook Benefit:**
- Vanilla has already processed movement/animation
- We can set CPR/BFCO variables that work with vanilla's decisions
- Animation graph is in correct state for our enhancements

**Pre-Hook Problem:**
- We might set CPR variables before vanilla processes them
- Vanilla might override our settings
- Animation graph might not be ready

## When Pre-Hook Would Be Better

Pre-hook is better when you want to:
- **Completely replace** vanilla behavior
- **Prevent** vanilla from doing something
- **Modify** vanilla's input before it processes

But that's **not our goal** - we want to **enhance**, not replace!

## Summary

| Aspect | Pre-Hook | Post-Hook |
|--------|----------|-----------|
| **State Accuracy** | Stale state | Current state ✅ |
| **Interference** | High risk | Low risk ✅ |
| **Animation State** | Might be outdated | Always current ✅ |
| **Compatibility** | Lower | Higher ✅ |
| **Philosophy** | Replace/Intercept | Enhance ✅ |
| **Proven Pattern** | Less common | ProjectGapClose ✅ |

## Conclusion

**Post-hook is better for our use case** because:
1. We work with accurate, up-to-date state
2. We enhance vanilla AI rather than replace it
3. We're more compatible with other mods
4. We follow a proven pattern (ProjectGapClose)
5. Better integration with CPR/BFCO

The original brief mentioned pre-hook, but that was before we decided to **enhance** rather than **suppress** vanilla AI. Post-hook aligns perfectly with our enhancement philosophy!
