# BFCO Attack Framework Integration

## Overview
BFCO (Attack Framework) provides animation events and graph variables for controlling combat animations, including attacks, power attacks, sprint attacks, and jump attacks.

## BFCO Animation Events

### Attack Events
- `BFCOAttackStart_Comb` - Combined/combo attack start
- `BFCOAttackstart_1` - Single attack start
- `attackStart` - Standard attack (vanilla)
- `attackStartSprint` - Sprint attack start
- `MCO_EndAnimation` - End current animation (MCO integration)

### Block Events
- `blockStart` - Start blocking
- `blockStop` - Stop blocking
- `bashFail` - Bash failure

### Other Events
- `Bfco_AttackStartFX` - Attack start FX event (triggers reset of attack state)

## BFCO Animation Graph Variables

### Attack State
- `NEW_BFCO_IsPowerAttacking` (int) - Set to 1 for power attack, 0 otherwise
- `NEW_BFCO_IsNormalAttacking` (int) - Set to 1 for normal attack, 0 otherwise
- `BFCONG_PARMB` (int) - Parameter for attack framework
- `NEW_BFCO_DisablePALMB` (int) - Disable power attack on LMB

### Settings (via graph variables)
- `bfcoDebug_DisJumpAttack` (float) - Disable jump attack (1.0 = disabled, 0.0 = enabled)
- `bfcoTG_DirPowerAttack` (float) - Enable directional power attack
- `bfcoTG_LmbPowerAttackNUM` (float) - LMB power attack setting
- `bfcoTG_RmbPowerAttackNUM` (float) - RMB power attack setting

## Integration Methods

### Normal Attack
```cpp
// Set attack state
actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);

// Trigger attack animation
actor->NotifyAnimationGraph("BFCOAttackStart_Comb");
// or
actor->NotifyAnimationGraph("attackStart");
```

### Power Attack
```cpp
// Set power attack state
actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);

// Trigger power attack
actor->NotifyAnimationGraph("BFCOAttackstart_1");
// or use vanilla
actor->NotifyAnimationGraph("attackStart");
```

### Sprint Attack
```cpp
// Ensure actor is sprinting
if (!actor->AsActorState()->IsSprinting()) {
    actor->NotifyAnimationGraph("SprintStart");
    actor->AsActorState()->actorState1.sprinting = 1;
}

// Trigger sprint attack
actor->NotifyAnimationGraph("attackStartSprint");
actor->NotifyAnimationGraph("attackStart");
```

### Jump Attack
```cpp
// Check if jump attack is enabled
float disJumpAttack = 0.0f;
bool jumpAttackDisabled = false;
if (actor->GetGraphVariableFloat("bfcoDebug_DisJumpAttack", disJumpAttack)) {
    jumpAttackDisabled = (disJumpAttack > 0.5f);
}

if (!jumpAttackDisabled) {
    // Trigger jump attack (if supported by animation graph)
    actor->NotifyAnimationGraph("attackStart");
    // Note: Jump attack typically requires actor to be in air/jumping state
}
```

### Resetting Attack State
```cpp
// Reset attack state (typically done on animation event)
actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
actor->SetGraphVariableInt("BFCONG_PARMB", 0);
actor->SetGraphVariableInt("NEW_BFCO_DisablePALMB", 0);
```

## Usage in Our Plugin

### In ActionExecutor::ExecuteBash
```cpp
void ActionExecutor::ExecuteBash(RE::Actor* a_actor)
{
    if (!a_actor) return;
    
    // Reset attack state
    a_actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
    a_actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
    
    // Trigger bash
    a_actor->NotifyAnimationGraph("bashStart");
}
```

### In ActionExecutor::ExecuteAttack (if we add it)
```cpp
void ActionExecutor::ExecuteAttack(RE::Actor* a_actor, bool a_isPowerAttack)
{
    if (!a_actor) return;
    
    if (a_isPowerAttack) {
        // Power attack
        a_actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 1);
        a_actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 0);
        a_actor->NotifyAnimationGraph("BFCOAttackstart_1");
    } else {
        // Normal attack
        a_actor->SetGraphVariableInt("NEW_BFCO_IsNormalAttacking", 1);
        a_actor->SetGraphVariableInt("NEW_BFCO_IsPowerAttacking", 0);
        a_actor->NotifyAnimationGraph("BFCOAttackStart_Comb");
    }
}
```

### For Sprint Attack (Gap Closing)
```cpp
void ActionExecutor::ExecuteSprintAttack(RE::Actor* a_actor)
{
    if (!a_actor) return;
    
    // Start sprinting if not already
    if (!a_actor->AsActorState()->IsSprinting()) {
        a_actor->NotifyAnimationGraph("SprintStart");
        a_actor->AsActorState()->actorState1.sprinting = 1;
    }
    
    // Trigger sprint attack
    a_actor->NotifyAnimationGraph("attackStartSprint");
    a_actor->NotifyAnimationGraph("attackStart");
}
```

## MCO Integration

BFCO works with MCO (Modern Combat Overhaul). When using MCO animations:

```cpp
// End current MCO animation before starting new attack
actor->NotifyAnimationGraph("MCO_EndAnimation");

// Then trigger attack
actor->NotifyAnimationGraph("BFCOAttackStart_Comb");
```

## Important Notes

1. **State Management**: Always reset attack state variables when starting a new attack to avoid conflicts.

2. **Animation Graph Events**: BFCO listens to `Bfco_AttackStartFX` event to reset attack state. This is typically fired by the animation graph itself.

3. **MCO Compatibility**: Use `MCO_EndAnimation` before triggering new attacks if MCO is installed.

4. **Sprint State**: For sprint attacks, ensure the actor is in sprinting state before triggering sprint attack animations.

5. **Vanilla Fallback**: If BFCO is not installed, fall back to vanilla animation events:
   - `attackStart` for normal attacks
   - `bashStart` for bashes
   - `attackStartSprint` for sprint attacks

## Detection

To check if BFCO is installed:
```cpp
bool IsBFCOInstalled()
{
    // Check if BFCO plugin is loaded
    auto bfcoPlugin = RE::TESDataHandler::GetSingleton()->LookupModByName("BFCO.esp");
    return bfcoPlugin != nullptr;
}
```

## References
- BFCO Source: Uses animation graph events and variables to control attack framework
- MCO Integration: Works with Modern Combat Overhaul for animation control
