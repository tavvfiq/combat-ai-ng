# CommonLibSSE-NG API Summary

This document summarizes the key CommonLibSSE-NG APIs used in this project for quick reference.

## Core SKSE API

### Initialization
```cpp
#include "SKSE/SKSE.h"

// In SKSEPlugin_Load
SKSE::Init(a_intfc);
auto* trampoline = SKSE::GetTrampoline();
trampoline->create(128); // Allocate space for hooks
```

### Trampoline (Hooking)
```cpp
#include "SKSE/Trampoline.h"

auto& trampoline = SKSE::GetTrampoline();
// Write 5-byte branch hook
trampoline->write_branch<5>(src_addr, dst_func);
// Write 6-byte branch hook (if needed)
trampoline->write_branch<6>(src_addr, dst_func);
```

### Logging
```cpp
#include "SKSE/Logger.h"

auto logger = SKSE::log::logger();
logger->info("Message: {}", value);
logger->error("Error: {}", error);
logger->warn("Warning: {}", warning);
logger->debug("Debug: {}", debug);
```

## Actor API

### Actor Class
```cpp
#include "RE/A/Actor.h"

// Get actor from handle
RE::Actor* actor = RE::Actor::LookupByHandle(handle).get();

// Check actor state
bool inCombat = actor->IsInCombat();
bool isBlocking = actor->IsBlocking();
bool isAttacking = actor->IsAttacking();
bool isAlive = !actor->IsDead();
bool isNPC = actor->CalculateCachedOwnerIsNPC();

// Get actor base (for level, etc.)
RE::TESNPC* base = actor->GetActorBase();
std::uint16_t level = actor->GetLevel();
```

### Actor Values (Health, Stamina, Magicka)
```cpp
#include "RE/A/ActorValueOwner.h"
#include "RE/A/ActorValues.h"

// Get current value
float currentStamina = actor->GetActorValue(RE::ActorValue::kStamina);
float currentHealth = actor->GetActorValue(RE::ActorValue::kHealth);

// Get base (max) value
float maxStamina = actor->GetBaseActorValue(RE::ActorValue::kStamina);
float maxHealth = actor->GetBaseActorValue(RE::ActorValue::kHealth);

// Calculate percentage
float staminaPercent = (maxStamina > 0) ? (currentStamina / maxStamina) : 0.0f;
```

### Actor State
```cpp
#include "RE/A/ActorState.h"

RE::ActorState* state = actor->AsActorState();

// Attack state
RE::ATTACK_STATE_ENUM attackState = state->GetAttackState();
// Values: kNone, kDraw, kSwing, kHit, kNextAttack, kFollowThrough, kBash, etc.

// Life state
RE::ACTOR_LIFE_STATE lifeState = state->GetLifeState();
// Values: kAlive, kDying, kDead, kUnconcious, kBleedout, etc.

// Knock state (stagger/recoil)
RE::KNOCK_STATE_ENUM knockState = state->GetKnockState();
// Values: kNormal, kOut, kDown, kGetUp, etc.

// Movement flags
bool isSneaking = state->IsSneaking();
bool isSprinting = state->IsSprinting();
bool isWalking = state->IsWalking();
```

### Actor Runtime Data
```cpp
// Access runtime data (combat controller, movement controller, etc.)
auto& runtimeData = actor->GetActorRuntimeData();

// Combat controller
RE::CombatController* combatController = runtimeData.combatController;

// Movement controller
BSTSmartPointer<RE::MovementControllerNPC> movementController = 
    runtimeData.movementController;

// Current combat target
RE::ActorHandle targetHandle = runtimeData.currentCombatTarget;
```

## Combat API

### CombatController
```cpp
#include "RE/C/CombatController.h"

RE::CombatController* controller = actor->GetActorRuntimeData().combatController;

// Get target
RE::ActorHandle targetHandle = controller->targetHandle;
RE::NiPointer<RE::Actor> target = RE::Actor::LookupByHandle(targetHandle);

// Check if fleeing
bool isFleeing = controller->IsFleeing();

// Combat state
RE::CombatState* state = controller->state;
bool isFleeing = state->isFleeing;
```

### CombatState
```cpp
#include "RE/C/CombatState.h"

// Access via CombatController
RE::CombatState* combatState = combatController->state;
bool isFleeing = combatState->isFleeing;
```

## Animation API

### Animation Graph
```cpp
#include "RE/I/IAnimationGraphManagerHolder.h"

// Actor implements IAnimationGraphManagerHolder
// Trigger animation events
bool success = actor->NotifyAnimationGraph(RE::BSFixedString("bashStart"));
bool success = actor->NotifyAnimationGraph(RE::BSFixedString("attackStart"));
bool success = actor->NotifyAnimationGraph(RE::BSFixedString("attackPowerStart"));

// Get/set animation graph variables
float value;
bool hasVar = actor->GetGraphVariableFloat(RE::BSFixedString("staggerDirection"), value);
actor->SetGraphVariableFloat(RE::BSFixedString("someVar"), 1.0f);
```

## Movement API

### MovementControllerNPC
```cpp
#include "RE/M/MovementControllerNPC.h"

// Get movement controller from runtime data
BSTSmartPointer<RE::MovementControllerNPC> movementController = 
    actor->GetActorRuntimeData().movementController;

// Movement controller implements IMovementDirectControl
// Note: Direct movement control may require specific interfaces
// Alternative: Use animation graph variables for movement
```

### Position & Orientation
```cpp
#include "RE/N/NiPoint3.h"

// Get actor position
RE::NiPoint3 position = actor->GetPosition();

// Get target position
RE::NiPoint3 targetPos = target->GetPosition();

// Calculate distance
float distance = position.GetDistance(targetPos);

// Calculate direction vector
RE::NiPoint3 direction = (targetPos - position);
direction.Unitize();

// Dot product (for orientation check)
RE::NiPoint3 selfForward = /* get forward vector */;
float dot = direction.Dot(selfForward);
// dot > 0.7 means target is roughly in front
```

## Utility Functions

### Actor Lookup
```cpp
// From handle
RE::NiPointer<RE::Actor> actor = RE::Actor::LookupByHandle(handle);

// Check if valid
if (!actor || !actor.get()) {
    return; // Invalid actor
}
```

### Safe Casting
```cpp
// Use skyrim_cast for RTTI-safe casting
RE::Actor* actor = skyrim_cast<RE::Actor*>(someRef);
```

### BSFixedString
```cpp
#include "RE/B/BSFixedString.h"

// Create fixed string for animation events
RE::BSFixedString eventName("bashStart");
// Or use string literals (implicit conversion)
actor->NotifyAnimationGraph("bashStart");
```

## Memory Safety Notes

1. **Never delete engine pointers** - The game manages all RE::* objects
2. **Use NiPointer for safety** - `RE::NiPointer<RE::Actor>` for actor references
3. **Validate pointers** - Always check for null before use
4. **Handle validation** - Resolve handles to pointers before use

## Common Patterns

### Safe Actor Access
```cpp
RE::ActorHandle handle = /* get handle */;
RE::NiPointer<RE::Actor> actor = RE::Actor::LookupByHandle(handle);
if (!actor || !actor.get()) {
    return; // Invalid
}
// Use actor.get() to access
```

### Combat Target Access
```cpp
auto& runtimeData = actor->GetActorRuntimeData();
RE::CombatController* controller = runtimeData.combatController;
if (!controller) {
    return; // No combat controller
}

RE::ActorHandle targetHandle = controller->targetHandle;
RE::NiPointer<RE::Actor> target = RE::Actor::LookupByHandle(targetHandle);
if (!target || !target.get()) {
    return; // No valid target
}
```

### Animation Event Trigger
```cpp
if (actor->NotifyAnimationGraph(RE::BSFixedString("bashStart"))) {
    // Success
} else {
    // Failed - actor may not have animation graph
}
```

## Version Notes

- CommonLibSSE-NG uses REL::Relocation for version-independent offsets
- Use `REL::RelocationID` for function addresses
- Use `REL::VariantID` for VTABLE offsets
- Check `SKSE::RUNTIME_SSE_*` constants for version detection
