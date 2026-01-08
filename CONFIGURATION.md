# Configuration Guide

## Overview
CombatAI-NG uses an INI configuration file to customize all aspects of the combat AI system. The configuration file is located at:

**`Data/SKSE/Plugins/CombatAI-NG.ini`**

## Installation

1. Copy `CombatAI-NG.ini.template` to your Skyrim `Data/SKSE/Plugins/` directory
2. Rename it to `CombatAI-NG.ini` (remove `.template` extension)
3. Edit the file with your preferred settings
4. Restart Skyrim or reload the plugin

## Configuration Sections

### [General]
General plugin settings.

- **EnablePlugin** (bool, default: `true`)
  - Master switch to enable/disable the entire plugin
  - If `false`, plugin loads but does nothing

- **EnableDebugLog** (bool, default: `false`)
  - Enable detailed debug logging
  - Useful for troubleshooting

- **ProcessingInterval** (float, default: `0.1`)
  - How often to process actors (in seconds)
  - Lower = more responsive but more CPU usage
  - Recommended: `0.1` (100ms)

### [Humanizer]
Settings for making AI feel more organic and human-like.

- **BaseReactionDelayMs** (float, default: `150.0`)
  - Base delay before NPCs react to threats (milliseconds)
  - Higher = slower reactions

- **ReactionVarianceMs** (float, default: `100.0`)
  - Random variance added to reaction delay (0-100ms)
  - Makes reactions feel less robotic

- **Level1MistakeChance** (float, default: `0.4`)
  - Probability that level 1 NPCs will fail reactions (0.0 to 1.0)
  - `0.4` = 40% failure chance

- **Level50MistakeChance** (float, default: `0.0`)
  - Probability that level 50 NPCs will fail reactions (0.0 to 1.0)
  - `0.0` = perfect reactions

- **BashCooldownSeconds** (float, default: `3.0`)
  - Cooldown between bash attempts (seconds)
  - Prevents bash spam

- **DodgeCooldownSeconds** (float, default: `2.0`)
  - Cooldown between dodge attempts (seconds)
  - Prevents dodge spam

### [DodgeSystem]
Settings for TK Dodge integration.

- **DodgeStaminaCost** (float, default: `10.0`)
  - Stamina cost per dodge
  - NPCs must have this much stamina to dodge

- **IFrameDuration** (float, default: `0.3`)
  - Invulnerability frame duration (seconds)
  - How long NPCs are invulnerable during dodge

- **EnableStepDodge** (bool, default: `false`)
  - Use step dodge variant
  - Requires animation graph support

- **EnableDodgeAttackCancel** (bool, default: `true`)
  - Allow dodging during attacks (attack cancel)
  - More responsive but may feel unrealistic

### [DecisionMatrix]
Settings for tactical decision making.

- **InterruptMaxDistance** (float, default: `150.0`)
  - Maximum distance for bash interrupt (game units)
  - Target must be power attacking within this distance

- **EnableEvasionDodge** (bool, default: `true`)
  - Enable evasion dodge when target is blocking
  - If `false`, NPCs won't dodge when target blocks

- **StaminaThreshold** (float, default: `0.2`)
  - Stamina threshold for survival retreat (0.0 to 1.0)
  - NPCs retreat when stamina falls below this percentage
  - `0.2` = 20% stamina

- **HealthThreshold** (float, default: `0.3`)
  - Health threshold for survival retreat (0.0 to 1.0)
  - NPCs retreat when health falls below this percentage
  - `0.3` = 30% health

- **EnableSurvivalRetreat** (bool, default: `true`)
  - Enable survival retreat behavior
  - If `false`, NPCs won't retreat when low on stamina/health

### [Performance]
Performance optimization settings.

- **OnlyProcessCombatActors** (bool, default: `true`)
  - Only process actors currently in combat
  - Set to `false` to process all actors (not recommended)

- **CleanupInterval** (float, default: `5.0`)
  - How often to clean up invalid actor references (seconds)
  - Higher = less frequent cleanup

- **MaxActorsPerFrame** (int, default: `0`)
  - Maximum actors to process per frame
  - `0` = unlimited (let processing interval handle throttling)
  - Use if experiencing performance issues

## Example Configuration

```ini
[General]
EnablePlugin = true
EnableDebugLog = false
ProcessingInterval = 0.1

[Humanizer]
BaseReactionDelayMs = 150.0
ReactionVarianceMs = 100.0
Level1MistakeChance = 0.4
Level50MistakeChance = 0.0
BashCooldownSeconds = 3.0
DodgeCooldownSeconds = 2.0

[DodgeSystem]
DodgeStaminaCost = 10.0
IFrameDuration = 0.3
EnableStepDodge = false
EnableDodgeAttackCancel = true

[DecisionMatrix]
InterruptMaxDistance = 150.0
EnableEvasionDodge = true
StaminaThreshold = 0.2
HealthThreshold = 0.3
EnableSurvivalRetreat = true

[Performance]
OnlyProcessCombatActors = true
CleanupInterval = 5.0
MaxActorsPerFrame = 0
```

## Tips

1. **Start with defaults** - The default values are balanced for most gameplay
2. **Adjust gradually** - Make small changes and test
3. **Use debug logging** - Enable `EnableDebugLog` to see what's happening
4. **Performance tuning** - If experiencing lag, increase `ProcessingInterval` or set `MaxActorsPerFrame`
5. **Difficulty scaling** - Adjust `Level1MistakeChance` and `Level50MistakeChance` to make AI easier/harder

## Troubleshooting

**Plugin not working:**
- Check `EnablePlugin = true` in `[General]` section
- Check log file for errors

**NPCs too reactive/not reactive enough:**
- Adjust `BaseReactionDelayMs` and `ReactionVarianceMs`
- Lower = more reactive, Higher = less reactive

**Too many/few dodges:**
- Adjust `DodgeCooldownSeconds`
- Check `EnableEvasionDodge` is `true`

**Performance issues:**
- Increase `ProcessingInterval`
- Set `MaxActorsPerFrame` to a lower value
- Ensure `OnlyProcessCombatActors = true`
