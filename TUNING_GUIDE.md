# EnhancedCombatAI — Tuning Guide

This guide explains how to tune EnhancedCombatAI for your setup, whether you run an animation-driven combat overhaul (**MCO / BFCO / SCAR**) or **vanilla / lightweight** melee. It covers what each important setting does, how the pieces interact, and gives ready-to-use presets.

Everything here maps to `Data/SKSE/Plugins/EnhancedCombatAI.ini`. If you have **SKSE Menu Framework** installed you can change all of it live in-game (Mod Control Panel → **Enhanced Combat AI → Config**) and hit **Save to INI** when you like the result — no restarts, no recompiles.

---

## 1. The one setting most people need: reach

NPCs decide "am I close enough to attack?" by comparing the distance to their target against their **effective attack range**:

```
effective range = WeaponReach × OffenseReachMultiplier + target body radius
```

- **WeaponReach** comes from **Precision** if installed (accurate, per-weapon capsule length), otherwise a vanilla fallback (~130 units for a typical weapon).
- **target body radius** is added automatically so the check works against large enemies (trolls, giants) as well as humans.
- **`OffenseReachMultiplier`** (in `[DecisionMatrix]`) is *your* knob. It's the single most important value for how aggressively NPCs commit to attacks.

> **Install Precision.** Without it, reach is a rough approximation and every reach-based decision degrades. It's the single biggest quality improvement for this mod.

### How to tune it
- NPCs **hang back / circle without swinging** → raise `OffenseReachMultiplier`.
- NPCs **lunge / attack from clearly too far** → lower it.

Because animation-driven attacks (MCO/BFCO) physically travel the character forward during the swing, they connect from farther than vanilla's near-instant hits. So overhauls generally want a **higher** multiplier than vanilla.

| Setup | `OffenseReachMultiplier` |
|---|---|
| Vanilla / lightweight melee | `1.0 – 1.1` |
| MCO / BFCO / SCAR (lunging attacks) | `1.3 – 1.5` |

---

## 2. Integrations — match them to your load order

Section `[ModIntegrations]`. These auto-detect, but set them explicitly so your intent is clear. **These apply at game load — restart after changing them.**

| Setting | Turn on when you run… |
|---|---|
| `EnableBFCOIntegration` | BFCO / MCO (via BFCO) / any MCO-framework attacks |
| `EnablePrecisionIntegration` | Precision (strongly recommended for everyone) |
| `EnableCPRIntegration` | Combat Pathing Revolution (adds circling / fallback / advance) |
| `EnableTKDodgeIntegration` | TK Dodge RE (lets NPCs dodge) |

If you run **MCO without BFCO's NPC attack framework**, keep `EnableBFCOIntegration = true` anyway — the mod falls back gracefully and still drives normal/power attacks; it just won't fire BFCO-specific directional attacks.

---

## 3. Offense: attacks, power attacks, sprint attacks

Section `[DecisionMatrix]`.

- **`EnableSprintAttack`** — the gap-closer. Feels great with MCO/BFCO (they have dedicated sprint-attack animations). On vanilla it uses the vanilla sprint attack, which is weaker/janky — some players prefer it off so NPCs just run in and swing.
- **`SprintAttackMinDistance` / `SprintAttackMaxDistance`** — the distance band where a sprint attack is considered. Default `220–400`. Widen the max if you want gap-closers from farther out.
- **`EnablePowerAttackStaminaCheck` / `EnableSprintAttackStaminaCheck`** — when `true`, NPCs won't power/sprint attack on empty stamina. Keep `true` for fairness; set `false` for relentless pressure.
- **Normal attacks are never stamina-gated** — an NPC at 0 stamina can still swing.

---

## 4. Scoring weights — shape the AI's personality

Section `[ScoringWeights]`. When several actions are valid, each gets a **priority**; the highest wins. Base priorities decide the default lean; modifiers push priority up when an opening exists.

Base priorities (`0.0–5.0`, higher = chosen more often):

| Key | Action |
|---|---|
| `AttackBase` | Normal/power attack in range |
| `SprintAttackBase` | Gap-closer attack |
| `AdvancingBase` | Run toward target |
| `InterruptPowerAttackBase` | Bash to interrupt a power attack |
| `EvasionDodgeBase` | Dodge / strafe spacing |
| `BackoffBase` | Back off from caster/archer |
| `FlankingBase` | Flank with allies |

Offense modifiers (`0.0–3.0`, added when the situation applies): `TargetStaggeredBonus`, `TargetCastingBonus`, `TargetRecoveryBonus`, `TargetFleeingBonus`, `TargetLowHealthFinisherBonus`, `OpeningRiskPenalty`, `FlankingAttackBonus`, `AllyCoverBonus`.

**Want more aggression?** Raise `AttackBase` / `SprintAttackBase`, lower `OpeningRiskPenalty` and `EvasionDodgeBase`.
**Want more tactical/defensive?** Raise `EvasionDodgeBase`, `OpeningRiskPenalty`, `BackoffBase`; lower `AttackBase`.

Tuning these live in the menu is the fastest way to find a feel — sliders update the AI instantly.

---

## 5. Humanizer — how "human" they feel

Section `[Humanizer]`. This is combat *feel*, independent of your combat overhaul.

- **`BaseReactionDelayMs`** — delay before reacting (at level 1, shrinks with NPC level). Lower = sharper/harder. `150–200` is a good band.
- **`MinMistakeChance`** — floor on mistake chance so even max-level NPCs occasionally slip. `0.0` = robotic-perfect elites; `0.05` = they stay human.
- **Cooldowns** (`BashCooldownSeconds`, `DodgeCooldownSeconds`) — prevent spam. Lower bash cooldown = more counter-play.
- **Mistake multipliers** — per-action error scaling. Raise `DodgeMistakeMultiplier` if NPCs dodge too perfectly, etc.

---

## 6. Presets

Copy the relevant block into the matching INI sections. Adjust to taste afterward.

### A) MCO / BFCO / SCAR (animation-driven, committed attacks)

```ini
[DecisionMatrix]
OffenseReachMultiplier      = 1.4
EnableSprintAttack          = true
SprintAttackMinDistance     = 220.0
SprintAttackMaxDistance     = 450.0
EnablePowerAttackStaminaCheck  = true
EnableSprintAttackStaminaCheck = true

[Humanizer]
BaseReactionDelayMs = 180.0
MinMistakeChance    = 0.05
BashCooldownSeconds = 2.5

[ScoringWeights]
AttackBase       = 1.0
SprintAttackBase = 1.3
OpeningRiskPenalty = 0.4
```
Rationale: overhaul attacks lunge forward, so a wider reach (`1.4`) keeps NPCs committing instead of whiffing at the edge. Sprint attacks shine with BFCO gap-closer animations.

### B) Vanilla / lightweight melee

```ini
[DecisionMatrix]
OffenseReachMultiplier      = 1.05
EnableSprintAttack          = false
EnablePowerAttackStaminaCheck  = true
EnableSprintAttackStaminaCheck = true

[Humanizer]
BaseReactionDelayMs = 180.0
MinMistakeChance    = 0.05
BashCooldownSeconds = 2.5

[ScoringWeights]
AttackBase       = 1.1
SprintAttackBase = 1.0
OpeningRiskPenalty = 0.4
```
Rationale: vanilla hits are near-instant and short-range, so a tight reach (`1.05`) avoids attacking from too far. Sprint attack often feels off in vanilla — leaving it off makes NPCs advance and swing normally.

### C) "Souls-like" harder duels (either combat base)

```ini
[DecisionMatrix]
EnableEvasionDodge = true

[Humanizer]
BaseReactionDelayMs = 120.0
MinMistakeChance    = 0.0
BashCooldownSeconds = 2.0
DodgeCooldownSeconds = 1.5

[ScoringWeights]
EvasionDodgeBase   = 1.5
OpeningRiskPenalty = 0.5
BackoffBase        = 1.9
```
Rationale: fast reactions, no mistakes, more spacing/dodging and interrupts. Pair with a higher `OffenseReachMultiplier` from preset A/B for your combat base.

---

## 7. Quick troubleshooting

| Symptom | Fix |
|---|---|
| NPC won't attack even point-blank | Install Precision; raise `OffenseReachMultiplier`. Check log `Misc: WeaponReach=` — should be ~100+, not ~1.3. |
| NPCs attack from visibly too far | Lower `OffenseReachMultiplier`. |
| Too passive / circle forever | Raise `AttackBase` / `SprintAttackBase`; lower `EvasionDodgeBase`, `OpeningRiskPenalty`. |
| Too aggressive / suicidal | Raise `OpeningRiskPenalty`, `EvasionDodgeBase`, `BackoffBase`; enable stamina checks. |
| Feels robotic | Raise `MinMistakeChance` (e.g. `0.05`) and `BaseReactionDelayMs`. |
| Sprint attacks look janky (vanilla) | `EnableSprintAttack = false`. |

To diagnose reach/range issues, set `EnableDebugLog = true` and read the per-actor lines in `Documents\My Games\Skyrim Special Edition\SKSE\EnhancedCombatAI.log`: `RangeCat`, `InAtkRange`, `WeaponReach`, and `TargetBoundRadius` tell you exactly what the AI thinks the geometry is.

---

*Tip: with SKSE Menu Framework, tune everything live, then Save to INI. It's far faster than editing the file and reloading.*
