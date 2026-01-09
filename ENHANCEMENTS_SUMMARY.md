# Enhancements Summary

## Overview
This document summarizes the major enhancements made to CombatAI-NG:
1. Precision Integration for weapon reach
2. Combat Style Enhancement System
3. Jump Evasion System
4. Backoff and Advancing Actions
5. Tie-Breaking System
6. Performance Optimizations

## 1. Precision Integration

### What It Does
- Integrates with Precision mod to get accurate weapon reach
- Uses Precision's collision system for precise reach calculations
- Falls back gracefully if Precision is not installed

### Implementation
- **PrecisionIntegration** class handles API communication
- Requests Precision V4 API on plugin initialization
- `GetWeaponReach()` method:
  1. Tries Precision API first
  2. Falls back to weapon stat
  3. Falls back to race unarmed reach
  4. Ultimate fallback: 150 game units

### Benefits
- Accurate weapon reach for interrupt decisions
- Weapon-specific reach (different weapons = different reach)
- Actor scale aware
- Works with or without Precision

### Files
- `src/PrecisionIntegration.h/cpp`
- Integrated into `ActorStateObserver` and `DecisionMatrix`

## 2. Combat Style Enhancement System

### What It Does
- **Enhances** NPC combat behavior based on their combat style
- Each NPC type behaves uniquely
- Works **with** vanilla AI instead of suppressing it

### Implementation
- **CombatStyleEnhancer** class modifies decisions
- Reads combat style from:
  - CombatController (runtime)
  - Actor base (default)
- Applies style-specific enhancements:
  - Dueling Style: More bash, less retreat
  - Flanking Style: More movement, prefers strafe
  - Aggressive Style: Less defensive, more offensive
  - Defensive Style: More cautious, earlier retreat
  - Magic Style: Maintains distance
  - Ranged Style: Prefers optimal range

### Benefits
- Unique NPC behavior per combat style
- Respects vanilla design
- No suppression - vanilla AI still works
- Dynamic - respects runtime combat style changes
- Configurable via Creation Kit

### Files
- `src/CombatStyleEnhancer.h/cpp`
- Integrated into `CombatDirector`

## Architecture Changes

### Before
```
Base Decision → Execute → (Optionally Suppress Vanilla AI)
```

### After
```
Base Decision → Enhance Based on Combat Style → Execute → (Vanilla AI Runs Normally)
```

## Key Differences

### Weapon Reach
- **Before**: Hardcoded 150 game units
- **After**: Uses Precision API or weapon stats

### Vanilla AI
- **Before**: Could suppress vanilla AI (limited implementation)
- **After**: Enhances decisions, vanilla AI runs normally

### NPC Uniqueness
- **Before**: All NPCs behaved similarly
- **After**: Each NPC type has unique behavior based on combat style

## Integration Points

1. **PrecisionIntegration** initialized in `CombatDirector::Initialize()`
2. **ActorStateObserver** uses Precision for weapon reach
3. **DecisionMatrix** uses actual weapon reach for interrupt decisions
4. **CombatStyleEnhancer** enhances decisions in `CombatDirector::ProcessActor()`

## Configuration

No additional configuration needed - both systems work automatically:
- Precision integration is optional (falls back if not available)
- Combat style enhancement uses existing combat styles

## 3. Jump Evasion System

### What It Does
- NPCs can jump to evade ranged attacks (bows/crossbows)
- Uses dodge system with `CombatAI_NG_Jump` flag for OAR animation replacement
- Triggers when target is using ranged weapon and within evasion distance

### Implementation
- **ActionType::Jump** - New action type
- Uses `DodgeSystem::ExecuteEvasionDodge` with `CombatAI_NG_Jump=true`
- OAR can detect the flag and replace dodge animation with jump animation
- Jump variable is reset when actor lands or when executing other actions

### Benefits
- NPCs can evade ranged attacks more effectively
- Works with animation replacement mods (OAR)
- Uses existing dodge system infrastructure

### Files
- `src/ActionExecutor.cpp` - ExecuteJump implementation
- `package/SKSE/Plugins/BehaviorDataInjector/CombatAI-NG_BDI.json` - Graph variable registration

## 4. Backoff Action

### What It Does
- NPCs back away when target is casting magic or drawing bow
- Prevents NPCs from standing still while target prepares ranged/magic attack
- Uses CPR backoff or direct movement control

### Implementation
- **ActionType::Backoff** - New action type
- **EvaluateBackoff()** - Detects target casting (`isCasting`) or drawing bow (`isDrawingBow`)
- **ExecuteBackoff()** - Uses CPR backoff or falls back to direct movement
- Priority 2 (between Evasion and Survival)

### Benefits
- More reactive NPCs
- Better survival against magic/ranged attacks
- Works with CPR for smooth movement

### Files
- `src/DecisionMatrix.cpp` - EvaluateBackoff implementation
- `src/ActionExecutor.cpp` - ExecuteBackoff implementation
- `src/ActorStateObserver.cpp` - isDrawingBow detection

## 5. Advancing Action

### What It Does
- NPCs close distance when too far from target
- Uses CPR advancing feature for smooth gap closing
- Triggers when distance exceeds sprint attack max distance

### Implementation
- **ActionType::Advancing** - New action type
- **EvaluateOffense()** - Checks if distance > sprintAttackMaxDistance
- **ExecuteAdvancing()** - Uses CPR advance radius override
- Disables other CPR behaviors to avoid conflicts
- Sets outer radius larger than current distance to trigger advancing

### Benefits
- NPCs maintain engagement range
- Prevents NPCs from staying too far away
- Smooth gap closing via CPR

### Files
- `src/DecisionMatrix.cpp` - Advancing logic in EvaluateOffense
- `src/ActionExecutor.cpp` - ExecuteAdvancing and SetCPRAdvancing

## 6. Tie-Breaking System

### What It Does
- When multiple decisions have the same priority, intelligently selects the best one
- Uses multiple factors: intensity, health, distance, action type
- Adds variety through random selection when scores are equal

### Implementation
- **SelectBestFromTie()** - Selects best decision from equal-priority candidates
- **CalculateDecisionScore()** - Calculates score based on:
  - Intensity (higher = better)
  - Health status (offensive when healthy, defensive when low)
  - Distance (optimal range for each action)
  - Action type (for variety)
- All decisions now have reasonable intensity values

### Benefits
- More intelligent decision making
- Better context awareness
- Prevents always picking first decision in list

### Files
- `src/DecisionMatrix.cpp` - Tie-breaking implementation
- All evaluation functions now set intensity values

## 7. Performance Optimizations

### Per-Actor Processing Throttling
- Each actor has its own processing timer
- Actors processed at configured interval (default 100ms)
- Reduces CPU usage while maintaining responsiveness

### BFCO State Optimization
- Removed redundant state resets
- Only sets needed flags explicitly (e.g., IsNormalAttacking=1, IsPowerAttacking=0)
- More efficient variable management

### Files
- `src/CombatDirector.cpp` - Per-actor throttling in ShouldProcessActor
- `src/ActionExecutor.cpp` - Optimized BFCO state management

## Testing

To test:
1. **Precision Integration**: Install Precision mod and verify weapon reach is accurate
2. **Combat Style Enhancement**: Test with different NPC types (bandits, guards, mages) and observe unique behaviors
3. **Jump Evasion**: Test with ranged weapons and verify NPCs jump to evade
4. **Backoff**: Test with magic casting and bow drawing to see NPCs back away
5. **Advancing**: Test with NPCs far away to see them close distance
6. **Tie-Breaking**: Observe intelligent selection when multiple actions are available

## Future Enhancements

Potential improvements:
- More combat style types
- Per-NPC configuration overrides
- Dynamic combat style learning
- Style-specific animation preferences
