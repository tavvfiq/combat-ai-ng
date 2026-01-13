# Timed Block Integration Plan

## Overview
Integration plan for adding timed block functionality to EnhancedCombatAI using the Simple Timed Block mod (`SimpleTimedBlock.esp`).

## How Simple Timed Block Works

### Core Mechanism
1. **Timed Block Window**: When an actor starts blocking, a magic effect (`mgef_parry_window`, FormID 0x801) is applied for 0.33 seconds
2. **Hit Detection**: The mod listens to `RE::TESHitEvent` events
3. **Success Condition**: If a hit is blocked (`HitFlag::kHitBlocked`) AND the defender has the parry window effect active, it's a successful timed block
4. **Effects**: On successful timed block:
   - Damage is prevented (via hook)
   - Attacker and nearby enemies are staggered (via spells)
   - Self-buff is applied (`spell_parry_buff`, FormID 0x80B)
   - Mod callback events are sent

### Key Forms (from SimpleTimedBlock.esp)
- `spell_parry_window` (FormID 0x802) - Spell that applies the timed block window effect
- `mgef_parry_window` (FormID 0x801) - Magic effect indicating timed block window is active
- `spell_parry_buff` (FormID 0x80B) - Buff applied on successful timed block
- `spell_parry` (FormID 0x803) - Spell applied to stagger attackers

### Mod Callback Events
- `STBL_OnTimedBlockDefender` - Sent when defender successfully timed blocks
  - `strArg`: Attacker FormID (as string)
  - `numArg`: Attacker level
  - `sender`: Defender actor
- `STBL_OnTimedBlockAttacker` - Sent when attacker's attack gets timed blocked
  - `strArg`: Empty
  - `numArg`: Attacker level
  - `sender`: Attacker actor

## Integration Strategy

### 1. Add Timed Block Action Type
- **New ActionType**: `ActionType::TimedBlock` (value 14)
- Similar to parry, but uses blocking instead of bashing
- Evaluated in `EvaluateInterrupt` (Priority 1) alongside parry

### 2. Form Lookup and Initialization
- **New Integration Class**: `TimedBlockIntegration` (similar to `PrecisionIntegration`)
- **Location**: `include/TimedBlockIntegration.h` and `src/TimedBlockIntegration.cpp`
- **Responsibilities**:
  - Check if SimpleTimedBlock.esp is loaded
  - Lookup required forms (`spell_parry_window`, `mgef_parry_window`)
  - Provide API to apply timed block window spell
  - Check if timed block window effect is active

### 3. Decision Matrix Integration
- **New Function**: `EvaluateTimedBlock()` in `DecisionMatrix`
- **Evaluation Criteria**:
  - Target is attacking (melee attack)
  - Target is within blocking range
  - Attack will hit within timed block window (0.33 seconds)
  - Self is not already attacking
  - Self has a shield or weapon that can block
- **Priority Calculation**:
  - Base priority similar to parry
  - Timing bonus for perfect timing (within 0.33s window)
  - Distance modifier
  - Power attack modifier (higher priority for power attacks)
  - Threat level modifier
  - Stamina check

### 4. Action Execution
- **New Function**: `ExecuteTimedBlock()` in `ActionExecutor`
- **Execution Steps**:
  1. Check if actor can block (has shield or weapon)
  2. Apply `spell_parry_window` spell to create timed block window
  3. Trigger block animation/state
  4. Record timed block attempt for feedback tracking

### 5. Feedback Tracking
- **New Class**: `TimedBlockFeedbackTracker` (similar to `ParryFeedbackTracker`)
- **Responsibilities**:
  - Record timed block attempts
  - Listen to `STBL_OnTimedBlockDefender` and `STBL_OnTimedBlockAttacker` events
  - Match attempts with success events
  - Store feedback data (success rate, timing accuracy)
  - Provide feedback to `ActorStateObserver` for adaptive learning

### 6. Temporal State Extensions
- **Add to `SelfTemporalState`**:
  - `bool lastTimedBlockSuccess = false;`
  - `float lastTimedBlockEstimatedDuration = 0.0f;`
  - `float timeSinceLastTimedBlockAttempt = 999.0f;`
  - `int timedBlockSuccessCount = 0;`
  - `int timedBlockAttemptCount = 0;`
- **Add to `TargetTemporalState`**:
  - (Already has attack timing data used for parry, can reuse)

### 7. Configuration
- **New Section**: `[TimedBlock]` in INI
- **Settings**:
  - `EnableTimedBlock = true` - Enable/disable timed block
  - `TimedBlockWindowStart = 0.05` - Start of timing window (seconds before hit)
  - `TimedBlockWindowEnd = 0.33` - End of timing window (matches mod's window)
  - `TimedBlockMinDistance = 50.0` - Minimum distance for timed block
  - `TimedBlockMaxDistance = 200.0` - Maximum distance for timed block
  - `TimedBlockBasePriority = 1.5` - Base priority for timed block opportunity
  - `TimedBlockTimingBonusMax = 0.3` - Maximum bonus for perfect timing
  - `TimedBlockEarlyPenalty = 0.5` - Penalty for blocking too early
  - `TimedBlockLatePenalty = 0.5` - Penalty for blocking too late

### 8. Mod Callback Registration
- **Extend `CombatDirector::RegisterModCallbacks()`**:
  - Register event sink for `STBL_OnTimedBlockDefender`
  - Register event sink for `STBL_OnTimedBlockAttacker`
  - Only register if timed block is enabled

### 9. Adaptive Learning
- **Similar to Parry**:
  - Use feedback to adjust attack duration estimation
  - Improve timing accuracy over time
  - Track success rates per target

## Implementation Order

1. **Phase 1: Foundation**
   - Create `TimedBlockIntegration` class
   - Add form lookup and initialization
   - Add configuration section

2. **Phase 2: Decision Making**
   - Add `ActionType::TimedBlock`
   - Implement `EvaluateTimedBlock()`
   - Integrate into `EvaluateInterrupt()`

3. **Phase 3: Execution**
   - Implement `ExecuteTimedBlock()`
   - Add action execution case

4. **Phase 4: Feedback**
   - Create `TimedBlockFeedbackTracker`
   - Register mod callbacks
   - Integrate feedback into temporal state

5. **Phase 5: Adaptive Learning**
   - Use feedback to improve timing
   - Update attack duration estimation

## Key Differences from Parry

1. **Action Type**: Blocking vs Bashing
2. **Window Duration**: 0.33 seconds (fixed by mod) vs configurable parry window
3. **Spell Application**: Apply `spell_parry_window` when starting block vs no spell for parry
4. **Effect Check**: Check for `mgef_parry_window` effect vs no effect check for parry
5. **Mod Events**: Different event names (`STBL_OnTimedBlockDefender` vs `EP_MeleeParryEvent`)

## Technical Considerations

1. **Blocking State**: Need to ensure NPCs can actually block (have shield or weapon)
2. **Spell Application**: Apply spell when starting block, not when attack hits
3. **Effect Duration**: The 0.33s window is fixed by the mod, we need to time our block start accordingly
4. **Compatibility**: Works alongside parry - NPCs can choose between timed block and parry based on situation
5. **Priority**: Timed block and parry should be evaluated together, with the best option chosen

## Files to Create/Modify

### New Files
- `include/TimedBlockIntegration.h`
- `src/TimedBlockIntegration.cpp`
- `include/TimedBlockFeedbackTracker.h`
- `src/TimedBlockFeedbackTracker.cpp`

### Modified Files
- `include/DecisionResult.h` - Add `ActionType::TimedBlock`
- `include/Config.h` - Add `TimedBlockSettings` struct
- `src/Config.cpp` - Add `ReadTimedBlockSettings()`
- `include/ActorStateData.h` - Extend temporal state structs
- `include/DecisionMatrix.h` - Add `EvaluateTimedBlock()` declaration
- `src/DecisionMatrix.cpp` - Implement `EvaluateTimedBlock()` and integrate
- `include/ActionExecutor.h` - Add `ExecuteTimedBlock()` declaration
- `src/ActionExecutor.cpp` - Implement `ExecuteTimedBlock()`
- `include/ActorStateObserver.h` - Update temporal state gathering
- `src/ActorStateObserver.cpp` - Integrate timed block feedback
- `include/CombatDirector.h` - Add mod callback registration
- `src/CombatDirector.cpp` - Register timed block callbacks
- `EnhancedCombatAI.ini.template` - Add `[TimedBlock]` section
- `package/SKSE/Plugins/EnhancedCombatAI.ini` - Add `[TimedBlock]` section

## Testing Considerations

1. **Mod Detection**: Verify SimpleTimedBlock.esp is loaded
2. **Form Lookup**: Verify all required forms are found
3. **Timing**: Test that NPCs time their blocks correctly
4. **Feedback**: Verify events are received and matched correctly
5. **Learning**: Verify NPCs improve over time
6. **Compatibility**: Test alongside parry to ensure both work together
