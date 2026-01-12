# Combat Style Enhancement System

## Overview
Instead of suppressing vanilla AI, EnhancedCombatAI **enhances** NPC combat behavior based on their combat style. Each NPC's combat style determines how they react to situations, making combat feel more unique and varied.

## How It Works

### CombatStyleEnhancer Class
The `CombatStyleEnhancer` class modifies decisions based on combat style:

1. **GetCombatStyle()** - Retrieves combat style from:
   - CombatController (runtime, can be changed)
   - Actor base (default)

2. **EnhanceDecision()** - Modifies base decision based on:
   - Combat style flags (Dueling, Flanking)
   - Combat style multipliers (Offensive, Defensive, etc.)
   - Style-specific behavior patterns

### Style Types

#### Dueling Style (`kDuelingStyle` flag)
- **Behavior**: Focused 1v1 combat
- **Enhancements**:
  - Higher priority for bash interrupts
  - Prefers dodge and power attacks
  - Less likely to retreat (only at very low health)
  - Cancels sprint attacks (prefers precision)
  - Prefers standing ground

#### Flanking Style (`kFlankingStyle` flag)
- **Behavior**: Prefers movement and positioning
- **Enhancements**:
  - Higher priority for strafe/dodge
  - More aggressive movement
  - More likely to use evasion

#### Aggressive Style (High `offensiveMult`)
- **Behavior**: Offensive-focused
- **Enhancements**:
  - Less defensive (retreats only at very low health)
  - Higher priority for bash/interrupt
  - More likely to press attacks
  - Boosts Advancing action priority

#### Defensive Style (High `defensiveMult`)
- **Behavior**: Cautious and defensive
- **Enhancements**:
  - Higher priority for retreat and backoff
  - Retreats earlier (at 30% stamina or 40% health)
  - Prefers evasion
  - Less likely to bash (only when very close)

#### Magic Style (High `magicScoreMult`)
- **Behavior**: Prefers distance for casting
- **Enhancements**:
  - Less likely to bash
  - More likely to retreat to maintain distance
  - Retreats when target gets too close

#### Ranged Style (High `rangedScoreMult`)
- **Behavior**: Prefers optimal range
- **Enhancements**:
  - Less likely to bash
  - Prefers strafe to maintain range
  - Similar to magic style

### Combat Style Multipliers

The system applies multipliers from combat style data:

- **Bash Multiplier** (`meleeData.bashMult`):
  - > 1.0: Increases bash priority
  - < 0.5: Decreases bash priority

- **Circle Multiplier** (`closeRangeData.circleMult`):
  - Affects strafe intensity
  - Higher = more aggressive movement

- **Fallback Multiplier** (`closeRangeData.fallbackMult`):
  - > 1.0: Increases retreat priority
  - < 0.5: Decreases retreat priority

- **Avoid Threat Chance** (`generalData.avoidThreatChance`):
  - > 0.5: Increases evasion priority
  - Affects how likely NPC is to dodge

### Supported Action Types

The enhancement system supports all action types:
- **Bash** - Interrupt actions
- **Dodge, Strafe, Jump** - Evasion actions
- **Retreat, Backoff** - Defensive actions
- **Attack, PowerAttack, SprintAttack** - Offensive actions
- **Advancing** - Gap closing action

Each style modifies priorities and can cancel certain actions based on style preferences.

## Integration

The enhancement happens in `CombatDirector::ProcessActor()`:

```cpp
// Make base decision
DecisionResult decision = m_decisionMatrix.Evaluate(state);

// Enhance based on combat style
decision = m_styleEnhancer.EnhanceDecision(a_actor, decision, state);
```

This means:
1. Base tactical decision is made (interrupt, evasion, survival)
2. Decision is enhanced based on NPC's combat style
3. Enhanced decision is executed
4. Vanilla AI still runs normally (not suppressed)

## Benefits

1. **Unique NPC Behavior** - Each NPC type behaves differently
2. **Respects Vanilla Design** - Works with existing combat styles
3. **No Suppression** - Vanilla AI still functions
4. **Configurable** - Combat styles can be modified in Creation Kit
5. **Dynamic** - Runtime combat style changes are respected

## Example Behaviors

### Bandit (Aggressive Style)
- Less likely to retreat
- More likely to bash/interrupt
- Presses attacks

### Guard (Defensive Style)
- Retreats earlier
- Prefers evasion
- More cautious

### Mage (Magic Style)
- Maintains distance
- Less melee engagement
- Retreats when approached

### Archer (Ranged Style)
- Maintains optimal range
- Prefers strafe
- Less likely to bash

## Technical Details

Combat styles are read from:
1. `CombatController.combatStyle` (runtime, can change)
2. `TESNPC.combatStyle` (base, default)

The enhancement modifies:
- Decision priority (higher/lower)
- Action type (can change or cancel)
- Action intensity (movement speed)
- Action direction (movement direction)

All modifications respect the base decision logic while adding style-specific flavor.
