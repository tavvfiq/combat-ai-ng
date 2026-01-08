# Implementation Notes

## Status
✅ **Core implementation complete!** All major components have been created and integrated.

## Architecture Overview

The plugin follows a modular architecture with clear separation of concerns:

1. **Hooks** - Intercepts `RE::Character::Update` at vtable index 0xAD (post-hook pattern)
2. **CombatDirector** - Singleton manager that processes actors and coordinates components
3. **ActorStateObserver** - Gathers comprehensive combat state data (self + target)
4. **DecisionMatrix** - Evaluates state against prioritized rules (Interrupt > Evasion > Survival)
5. **ActionExecutor** - Executes decisions via animations and movement control
6. **Humanizer** - Adds organic feel (reaction latency, mistake probability, cooldowns)
7. **DodgeSystem** - Handles TK Dodge integration for evasion
8. **CombatStyleEnhancer** - Enhances NPC behavior based on combat styles
9. **Config** - Configuration system using SimpleINI

## Important Notes

### Hook Implementation
✅ **Implemented using `stl::write_vfunc` on `RE::Character` vtable index 0xAD**
- Uses **post-hook pattern** (call original, then custom logic)
- Inspired by ProjectGapClose implementation
- More reliable than direct Actor vtable hooking

**Why Post-Hook?**
- **State Consistency**: Vanilla Update() runs first, ensuring accurate actor state
- **Less Interference**: We enhance vanilla AI rather than replace it
- **Animation Accuracy**: Animation states are fully updated before our logic runs
- **Better Compatibility**: Works with vanilla and other mods without conflicts
- **Proven Pattern**: ProjectGapClose uses the same approach successfully

See `HOOK_PATTERN_EXPLANATION.md` for detailed comparison of pre-hook vs post-hook patterns.

### Movement Control
✅ **Implemented via CPR (Combat Pathing Revolution) integration**
- Uses animation graph variables for movement control
- Automatically detects CPR availability
- Falls back gracefully if CPR is not installed
- Supports: Circling, Fallback, Advance, Backoff

### Power Attack Detection
✅ **Implemented using `HighProcessData->attackData->data.flags`**
- Proper method via `currentProcess->high->attackData->data.flags`
- Checks for `RE::AttackData::AttackFlag::kPowerAttack`
- Based on ProjectGapClose implementation

### Configuration System
✅ **Implemented using SimpleINI**
- Configuration file: `CombatAI-NG.ini`
- Template provided: `CombatAI-NG.ini.template`
- Supports all major settings (humanizer, dodge, decision matrix, performance)

### Testing Checklist
- [ ] Verify hook installation (check logs)
- [ ] Test bash interrupt functionality
- [ ] Test strafe/evasion with CPR and TK Dodge
- [ ] Test retreat/survival behavior with CPR fallback
- [ ] Verify no crashes or performance issues
- [ ] Test with different NPC levels (mistake probability)
- [ ] Test cooldowns work correctly
- [ ] Test with/without CPR installed
- [ ] Test with/without BFCO installed
- [ ] Test with/without Precision installed
- [ ] Test with/without TK Dodge installed

## Completed Enhancements

✅ **Precision Integration** - Weapon reach now uses Precision mod API
✅ **Combat Style Enhancement** - NPCs have unique AI based on combat style (replaces suppression)
✅ **Power Attack Detection** - Now uses proper method via `HighProcessData->attackData->data.flags`
✅ **Hook Implementation** - Updated to use `stl::write_vfunc` for more reliable hooking
✅ **CPR Integration** - Support for Combat Pathing Revolution via animation graph variables
✅ **BFCO Integration** - Support for BFCO attack framework for attack animations

## Mod Integrations

### Combat Pathing Revolution (CPR)
- **Purpose**: Enhanced movement control during combat
- **Method**: Animation graph variables (CPR_EnableCircling, CPR_EnableFallback, etc.)
- **Usage**: Automatically detected and used for strafe/retreat actions
- **Documentation**: See `CPR_INTEGRATION.md`

### BFCO Attack Framework
- **Purpose**: Enhanced attack animations and state management
- **Method**: Animation events (BFCOAttackStart_Comb, attackStartSprint) and graph variables
- **Usage**: Automatically detected and used for attack animations
- **Documentation**: See `BFCO_INTEGRATION.md`

### TK Dodge RE
- **Purpose**: Evasion system for dodging attacks
- **Method**: Animation events and TK Dodge API
- **Usage**: Integrated into DodgeSystem for evasion actions
- **Documentation**: See `DODGE_INTEGRATION.md`

### Precision
- **Purpose**: Accurate weapon reach calculations
- **Method**: Precision API (GetAttackCollisionCapsuleLength)
- **Usage**: Used in ActorStateObserver for accurate distance calculations
- **Documentation**: See `PRECISION_INTEGRATION.md`

## TK Dodge Integration

✅ **Dodge System Implemented!**

The evasion system now uses TK Dodge for NPCs:
- **DodgeSystem**: Handles TK Dodge integration for NPCs
- **Evasion**: When target is blocking, NPCs will dodge instead of strafe
- **Direction Calculation**: Automatically determines best dodge direction (Forward/Back/Left/Right)
- **Stamina Cost**: Respects TK Dodge stamina requirements
- **Cooldowns**: Integrated with Humanizer cooldown system

### Requirements
- TK Dodge RE must be installed and active
- NPCs need the TK Dodge animation events in their animation graphs
- Animation graph variables: `bIsDodging`, `bInJumpState`, `iStep`, `TKDR_IframeDuration`

### Dodge Events Used
- `TKDodgeForward` - Dodge forward
- `TKDodgeBack` - Dodge backward  
- `TKDodgeLeft` - Dodge left
- `TKDodgeRight` - Dodge right

## Next Steps

1. **In-Game Testing** - Verify all functionality works correctly
2. **Performance Optimization** - Profile and optimize if needed
3. **Additional Features** (Optional):
   - Sprint attack support (gap closing)
   - Jump attack support
   - More sophisticated threat calculations
   - Per-NPC configuration overrides
4. **Documentation** - Keep documentation updated as features are added

## Known Limitations

1. **Movement Control**: Relies on CPR for advanced movement. Falls back to basic control if CPR not available.
2. **Animation Graph Variables**: Some features require animation graphs to support specific variables (CPR, TK Dodge, BFCO).
3. **Combat Style Requirements**: Some CPR features require "Close Range - Dueling" in combat style.

## Performance Considerations

- Processing is throttled via `ProcessInterval` config setting
- Actors are cleaned up when not in combat
- State gathering is optimized to avoid heavy operations
- Cooldowns prevent action spam

## File Structure

```
src/
├── pch.h                         ✅ Precompiled header
├── main.cpp                      ✅ Plugin entry point
├── Logger.h                      ✅ Logging utilities
├── Config.h/cpp                   ✅ Configuration system (SimpleINI)
├── ActorStateData.h               ✅ State data structures
├── DecisionResult.h               ✅ Decision enums
├── ActorStateObserver.h/cpp       ✅ State gathering (with Precision integration)
├── DecisionMatrix.h/cpp           ✅ Tactical logic (Interrupt/Evasion/Survival)
├── ActionExecutor.h/cpp           ✅ Action execution (CPR/BFCO/TK Dodge)
├── Humanizer.h/cpp                ✅ Organic feel (latency/mistakes/cooldowns)
├── CombatDirector.h/cpp           ✅ Singleton manager
├── Hooks.h/cpp                    ✅ Hook implementations (Character::Update)
├── DodgeSystem.h/cpp              ✅ TK Dodge integration
├── CombatStyleEnhancer.h/cpp      ✅ Combat style enhancement
└── PrecisionIntegration.h/cpp     ✅ Precision mod integration
```

## Documentation Files

- `README.md` - Main project documentation
- `PLAN.md` - Original implementation plan
- `brief.md` - Project brief and goals
- `role.md` - Development guidelines
- `CPR_INTEGRATION.md` - Combat Pathing Revolution integration guide
- `BFCO_INTEGRATION.md` - BFCO attack framework integration guide
- `DODGE_INTEGRATION.md` - TK Dodge integration guide
- `PRECISION_INTEGRATION.md` - Precision mod integration guide
- `COMBAT_STYLE_ENHANCEMENT.md` - Combat style enhancement system
- `PROJECTGAPCLOSE_INSIGHTS.md` - Insights from ProjectGapClose analysis
- `ENHANCEMENTS_SUMMARY.md` - Summary of major enhancements
- `CONFIGURATION.md` - Configuration file documentation
- `COMMONLIB_API.md` - CommonLibSSE-NG API reference

## Building

Use xmake as configured:
```bash
xmake build
```

The plugin will be built according to the `xmake.lua` configuration.

## Debugging

Enable debug logging by modifying the log level in your SKSE configuration or by using the SKSE logger directly. All components use the `LOG_*` macros from `Logger.h`.
