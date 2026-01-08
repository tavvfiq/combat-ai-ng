# Precision Integration

## Overview
CombatAI-NG integrates with the Precision mod to get accurate weapon reach information for NPCs. This allows the AI to make better tactical decisions based on actual weapon reach rather than hardcoded values.

## How It Works

### PrecisionIntegration Class
The `PrecisionIntegration` class handles all communication with Precision:

1. **Initialize()** - Requests the Precision API on plugin load
2. **GetWeaponReach()** - Gets weapon reach for an actor:
   - First tries Precision API (`GetAttackCollisionCapsuleLength`)
   - Falls back to weapon stat or race unarmed reach
   - Ultimate fallback: 150 game units

### Integration Points

1. **ActorStateObserver** - Uses Precision to get weapon reach:
   - Calls `PrecisionIntegration::GetInstance().GetWeaponReach()`
   - Stores reach in `ActorStateData.weaponReach`

2. **DecisionMatrix** - Uses actual weapon reach for interrupt decisions:
   - Compares target distance to actual weapon reach
   - More accurate bash interrupt timing

## Requirements

- **Precision mod** must be installed (optional but recommended)
- If Precision is not available, the system falls back to:
  - Weapon reach stat from weapon form
  - Race unarmed reach
  - Default 150 game units

## Benefits

1. **Accurate Reach Calculation** - Uses Precision's collision system for precise reach
2. **Weapon-Specific** - Different weapons have different reach
3. **Actor Scale Aware** - Accounts for actor size
4. **Fallback Support** - Works even without Precision

## API Usage

```cpp
// Get weapon reach for an actor
float reach = PrecisionIntegration::GetInstance().GetWeaponReach(actor);

// Check if Precision is available
bool hasPrecision = PrecisionIntegration::GetInstance().IsAvailable();
```

## Technical Details

The integration uses Precision's V4 API:
- `GetAttackCollisionCapsuleLength()` - Gets current attack collision length
- Falls back gracefully if Precision is not loaded
- No performance impact if Precision unavailable

## Configuration

No configuration needed - the integration is automatic. If Precision is installed, it will be used. Otherwise, fallback calculations are used.
