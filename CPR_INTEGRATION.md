# Combat Pathing Revolution (CPR) Integration

## Overview
Combat Pathing Revolution (CPR) uses animation graph variables to control NPC movement during combat. Our plugin can integrate with CPR by setting these variables to enhance NPC pathing behavior.

## CPR Animation Graph Variables

### Circling
Controls NPC circling behavior around targets:
- `CPR_EnableCircling` (bool) - Enable/disable circling
- `CPR_CirclingDistMin` (float) - Minimum distance for circling
- `CPR_CirclingDistMax` (float) - Maximum distance for circling
- `CPR_CirclingAngleMin` (float) - Minimum circling angle (0-180)
- `CPR_CirclingAngleMax` (float) - Maximum circling angle (0-180)
- `CPR_CirclingViewConeAngle` (float) - View cone angle (0-360, recommend 360)

### Advance
Controls NPC advancing behavior:
- `CPR_EnableAdvanceRadius` (bool) - Enable/disable advance radius override
- `CPR_InnerRadiusMin` (float) - Minimum inner radius
- `CPR_InnerRadiusMid` (float) - Medium inner radius
- `CPR_InnerRadiusMax` (float) - Maximum inner radius
- `CPR_OuterRadiusMin` (float) - Minimum outer radius
- `CPR_OuterRadiusMid` (float) - Medium outer radius
- `CPR_OuterRadiusMax` (float) - Maximum outer radius
- `CPR_InterruptAction` (bool) - Interrupt current action (set to true, auto-resets)

### Backoff
Controls NPC backoff behavior:
- `CPR_EnableBackoff` (bool) - Enable/disable backoff
- `CPR_BackoffMinDistMult` (float) - Backoff distance multiplier
- `CPR_BackoffChance` (float) - Probability of backoff (0.0-1.0)

### Fallback
Controls NPC fallback behavior:
- `CPR_EnableFallback` (bool) - Enable/disable fallback
- `CPR_FallbackDistMin` (float) - Minimum fallback distance
- `CPR_FallbackDistMax` (float) - Maximum fallback distance
- `CPR_FallbackWaitTimeMin` (float) - Minimum wait time after fallback
- `CPR_FallbackWaitTimeMax` (float) - Maximum wait time after fallback

## Integration Methods

### Method 1: Direct Graph Variable Setting
```cpp
// Enable circling
actor->SetGraphVariableBool("CPR_EnableCircling", true);
actor->SetGraphVariableFloat("CPR_CirclingDistMin", 90.0f);
actor->SetGraphVariableFloat("CPR_CirclingDistMax", 1200.0f);
actor->SetGraphVariableFloat("CPR_CirclingAngleMin", 30.0f);
actor->SetGraphVariableFloat("CPR_CirclingAngleMax", 90.0f);
actor->SetGraphVariableFloat("CPR_CirclingViewConeAngle", 360.0f);

// Disable when done
actor->SetGraphVariableBool("CPR_EnableCircling", false);
```

### Method 2: Via Animation Annotations
CPR supports animation annotations:
- `CPR.EnableAdvance|InnerMin|InnerMid|InnerMax|OuterMin|OuterMid|OuterMax`
- `CPR.EnableBackoff|BackoffMult|BackoffChance`
- `CPR.EnableCircling|DistMin|DistMax|AngleMin|AngleMax|ViewCone`
- `CPR.EnableFallback|DistMin|DistMax|WaitMin|WaitMax`
- `CPR.DisableAll`

## Usage in Our Plugin

### For Strafe/Circling Actions
When our `DecisionMatrix` decides on a "Strafe" action, we can enable CPR circling:

```cpp
// In ActionExecutor::ExecuteStrafe
actor->SetGraphVariableBool("CPR_EnableCircling", true);
actor->SetGraphVariableFloat("CPR_CirclingDistMin", calculatedMinDist);
actor->SetGraphVariableFloat("CPR_CirclingDistMax", calculatedMaxDist);
// ... set other circling parameters
```

### For Retreat Actions
When retreating, we can use CPR fallback:

```cpp
// In ActionExecutor::ExecuteRetreat
actor->SetGraphVariableBool("CPR_EnableFallback", true);
actor->SetGraphVariableFloat("CPR_FallbackDistMin", retreatDistance);
actor->SetGraphVariableFloat("CPR_FallbackDistMax", retreatDistance * 1.5f);
```

### For Advance Actions
When NPC needs to close distance:

```cpp
// Enable advance radius override
actor->SetGraphVariableBool("CPR_EnableAdvanceRadius", true);
actor->SetGraphVariableFloat("CPR_InnerRadiusMin", innerMin);
actor->SetGraphVariableFloat("CPR_OuterRadiusMax", outerMax);
// ... set other advance parameters
```

## Important Notes

1. **Weapon Reach**: CPR automatically adds weapon reach to distance calculations, so you don't need to account for it manually.

2. **Combat Style Requirement**: Circling and Fallback require "Close Range - Dueling" to be checked in the NPC's combat style.

3. **Disable When Done**: Always disable CPR variables when the action is complete to avoid conflicts:
   ```cpp
   actor->SetGraphVariableBool("CPR_EnableCircling", false);
   actor->SetGraphVariableBool("CPR_EnableAdvanceRadius", false);
   actor->SetGraphVariableBool("CPR_EnableBackoff", false);
   actor->SetGraphVariableBool("CPR_EnableFallback", false);
   ```

4. **Interrupt Action**: Use `CPR_InterruptAction` to interrupt current advance action:
   ```cpp
   actor->SetGraphVariableBool("CPR_InterruptAction", true);
   // CPR automatically resets this to false after processing
   ```

## Example: Enhanced Strafe with CPR

```cpp
void ActionExecutor::ExecuteStrafe(RE::Actor* a_actor, const DecisionResult& a_result)
{
    if (!a_actor) return;
    
    // Calculate strafe parameters based on distance and weapon reach
    float minDist = a_result.distance * 0.8f;
    float maxDist = a_result.distance * 1.2f;
    
    // Enable CPR circling for smooth strafe movement
    a_actor->SetGraphVariableBool("CPR_EnableCircling", true);
    a_actor->SetGraphVariableFloat("CPR_CirclingDistMin", minDist);
    a_actor->SetGraphVariableFloat("CPR_CirclingDistMax", maxDist);
    a_actor->SetGraphVariableFloat("CPR_CirclingAngleMin", 45.0f);
    a_actor->SetGraphVariableFloat("CPR_CirclingAngleMax", 135.0f);
    a_actor->SetGraphVariableFloat("CPR_CirclingViewConeAngle", 360.0f);
    
    // CPR will handle the actual movement
    // No need for direct movement control
}
```

## References
- CPR Developer Guidelines: `CombatPathingRevolution/doc/en/Developers Guidelines of CPR.md`
- CPR Source: Uses hooks to read graph variables and override vanilla pathing calculations
