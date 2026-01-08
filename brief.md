# Project: Reactive Combat AI (SKSE Plugin)

## 1. Environment & Architecture
* **Tech Stack:** C++20/23, CMake, [CommonLibSSE-NG](https://github.com/CharmedBaryon/CommonLibSSE-NG).
* **Pattern:** Singleton `CombatDirector` class to manage logic across all active actors.
* **Goal:** Intercept vanilla AI decision-making and inject high-priority tactical overrides.

## 2. The Hook (Engine Injection)
Identify and hook the game loop where Actor processing occurs.
* **Target Function:** `RE::Actor::Update` (or specific combat behavior subroutines).
* **Mechanism:** Use a **Trampoline** hook (code detour).
    * *Pre-Hook:* Run your custom `ScanAndDecide()` logic.
    * *Vanilla Call:* If your logic took action, suppress or modify the vanilla call; otherwise, let it run normally.

## 3. The Observer (Data Gathering)
Every frame (or tick), gather state data for the NPC and their current target.
* **Self State:**
    * Stamina % (Low stamina = defensive mode).
    * Current Animation State (Are we already swinging?).
* **Target State:**
    * **Action:** Is Target Blocking? Power Attacking? Casting?
    * **Orientation:** Is Target looking at me? (Dot Product check).
    * **Vulnerability:** Is Target staggered or recoiling?

## 4. The Brain (Decision Matrix)
Evaluate the gathered data against a prioritized rule set.
* **Priority 1: Interrupt (Counter-Play)**
    * *Trigger:* Target is winding up a Power Attack + Distance < Reach.
    * *Decision:* Force **Bash**.
* **Priority 2: Evasion (Tactical)**
    * *Trigger:* Target is Blocking + I am Idle.
    * *Decision:* Force **Strafe/Circle** (Move perpendicular to target).
* **Priority 3: Survival (Fallback)**
    * *Trigger:* Stamina < 20% or Health Critical.
    * *Decision:* Force **Retreat** (Backpedal) & Suppress Attack.

## 5. The Actuator (Execution)
Bypass the slow vanilla AI package system and force actions directly.
* **Animation Injection:**
    * Use `NotifyAnimationGraph("bashStart")` or `("attackStart")` to trigger moves instantly.
* **Movement Override:**
    * Hijack the `MovementController`.
    * Manually set the `InputVector` (X, Y) to steer the NPC (e.g., set X=1.0 for right strafe) instead of relying on pathfinding.

## 6. The Humanizer (Tuning)
Ensure the AI feels fair and organic, not robotic.
* **Reaction Latency:** Implement a customized delay (e.g., `150ms + random(0, 100ms)`) between detecting a threat and reacting.
* **Mistake Probability:** Inverse scaling with NPC Level (Level 1 Bandit = 40% chance to fail the counter; Level 50 Boss = 0% chance).
* **Cooldowns:** Prevent spam (e.g., "Bash" cooldown of 3 seconds) to stop stunlock loops.

## NOTES
we will be using xmake to build the project. and i already create the xmake.lua as a base that you can expands. the commonlibsse-ng will be on ./lib directory