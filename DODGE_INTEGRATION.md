# TK Dodge Integration Guide

## Overview
The Combat AI system now integrates with TK Dodge RE to provide NPCs with dodge/evasion capabilities. When NPCs detect a blocking or attacking target, they will perform tactical dodges instead of simple strafing.

## How It Works

### DodgeSystem Class
The `DodgeSystem` class handles all TK Dodge integration:

1. **CanDodge()** - Checks if an actor can dodge:
   - Not already dodging
   - Has sufficient stamina
   - Not in invalid states (swimming, jumping, kill move, etc.)
   - Can cancel attacks if configured

2. **ExecuteEvasionDodge()** - Executes a dodge for evasion:
   - Calculates best dodge direction based on target position
   - Chooses perpendicular direction to target's attack (if attacking)
   - Triggers appropriate TK Dodge animation event

3. **DetermineDodgeDirection()** - Maps dodge vector to TK Dodge event:
   - `TKDodgeForward` - Forward relative to actor
   - `TKDodgeBack` - Backward relative to actor
   - `TKDodgeLeft` - Left relative to actor
   - `TKDodgeRight` - Right relative to actor

### Integration Points

1. **DecisionMatrix** - When evaluating evasion (Priority 2):
   - Trigger: Target is blocking + Self is idle
   - Action: `ActionType::Strafe` (now uses dodge)

2. **ActionExecutor** - Executes dodge:
   - `ExecuteDodge()` calls `DodgeSystem::ExecuteEvasionDodge()`
   - Sets animation graph variables:
     * `iStep = 2` (if step dodge enabled)
     * `TKDR_IframeDuration` (i-frame duration)

3. **Humanizer** - Manages cooldowns:
   - 2 second cooldown for dodge actions
   - Prevents dodge spam

## Configuration

The `DodgeSystem::Config` structure allows customization:

```cpp
struct Config {
    float dodgeStaminaCost = 10.0f;      // Stamina cost per dodge
    float iFrameDuration = 0.3f;         // Invulnerability frame duration
    bool enableStepDodge = false;         // Use step dodge variant
    bool enableDodgeAttackCancel = true; // Allow dodging during attacks
};
```

## Animation Graph Requirements

For NPCs to dodge, their animation graphs must support:

1. **Animation Events**:
   - `TKDodgeForward`
   - `TKDodgeBack`
   - `TKDodgeLeft`
   - `TKDodgeRight`
   - `TKDR_DodgeStart` (for stamina cost application)

2. **Graph Variables**:
   - `bIsDodging` (bool) - Current dodge state
   - `bInJumpState` (bool) - Jump state check
   - `iStep` (int) - Step dodge flag (optional)
   - `TKDR_IframeDuration` (float) - I-frame duration

## Usage Example

When an NPC detects a blocking target:

1. **State Observation**: `ActorStateObserver` detects target is blocking
2. **Decision Making**: `DecisionMatrix` evaluates evasion priority
3. **Action Execution**: `ActionExecutor` calls `DodgeSystem::ExecuteEvasionDodge()`
4. **Dodge Execution**: 
   - Calculates dodge direction (perpendicular to target)
   - Checks if actor can dodge
   - Sets animation graph variables
   - Triggers TK Dodge animation event
5. **Cooldown**: `Humanizer` tracks dodge cooldown

## Testing

To test the dodge integration:

1. Ensure TK Dodge RE is installed and working
2. Enter combat with an NPC
3. Block with your weapon/shield
4. NPC should dodge when you're blocking (if they're idle)
5. Check logs for "Dodge executed" messages

## Troubleshooting

**NPCs not dodging:**
- Check if TK Dodge is installed and active
- Verify NPC animation graphs have TK Dodge events
- Check stamina - NPCs need sufficient stamina to dodge
- Check cooldowns - NPCs may be on dodge cooldown
- Verify NPC is in valid state (not swimming, jumping, etc.)

**Wrong dodge direction:**
- Dodge direction is calculated based on target position
- For attacking targets, dodge is perpendicular to attack direction
- For blocking targets, dodge is perpendicular to line between actors

**Performance issues:**
- Dodge checks are lightweight (animation graph variable reads)
- Cooldowns prevent excessive dodge attempts
- Only processes actors in combat
