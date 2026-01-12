# CommonLibSSE vs CommonLibSSE-NG API Comparison

This document compares APIs available in CommonLibSSE (non-NG) that are **not available** or **different** in CommonLibSSE-NG, focusing on APIs beneficial for the EnhancedCombatAI project.

## Summary

**CommonLibSSE (non-NG)** is more frequently updated and includes some utility APIs that NG doesn't have. However, **CommonLibSSE-NG** provides better cross-version compatibility (SE/AE/VR) and some enhanced error handling.

---

## APIs Available Only in CommonLibSSE (non-NG)

### 1. **RE::CombatUtilities** ⭐ **Highly Relevant**

**Location:** `RE/C/CombatUtilities.h`

**Available Function:**
```cpp
namespace RE
{
    struct CombatUtilities
    {
        static RE::NiPoint3 GetAngleToProjectedTarget(
            RE::NiPoint3 a_origin, 
            RE::TESObjectREFR* a_target, 
            float a_speed, 
            float a_gravity, 
            RE::ACTOR_LOS_LOCATION a_location
        );
    };
}
```

**Use Case for EnhancedCombatAI:**
- **Projectile trajectory calculations** for ranged combat
- **Predicting where projectiles will hit** (useful for dodging/evasion)
- **Calculating optimal attack angles** for NPCs using ranged weapons
- **Combat positioning** based on projectile paths

**Benefit:** This could significantly enhance ranged combat AI decision-making, allowing NPCs to:
- Predict projectile paths and dodge accordingly
- Calculate optimal positions to avoid incoming projectiles
- Better coordinate ranged attacks in group combat

---

### 2. **RE::AnimationSystemUtils** ⭐ **Potentially Useful**

**Location:** `RE/A/AnimationSystemUtils.h`

**Available Structures:**
```cpp
namespace RE::AnimationSystemUtils
{
    class UtilsClipData
    {
        const ClipGeneratorData*  clipData;       // 00
        const BoundAnimationData* animData;       // 08
        float                     playbackSpeed;  // 10
        float                     motionWeight;   // 14
        float                     currentTime;    // 18
    };
}
```

**Use Case for EnhancedCombatAI:**
- **Animation timing analysis** for better attack duration estimation
- **Animation state tracking** for more accurate combat state detection
- **Playback speed monitoring** for detecting slow-motion effects

**Benefit:** Could improve the `EstimateAttackDuration` function by providing more accurate animation timing data.

---

### 3. **SKSE::log::init()** ⚠️ **Minor Difference**

**Location:** `SKSE/Logger.h`

**Available Function:**
```cpp
namespace SKSE::log
{
    void init();  // Only in non-NG
}
```

**Difference:**
- **Non-NG:** Has explicit `init()` function
- **NG:** Logging initializes automatically (no explicit init needed)

**Impact:** Minimal - NG handles this automatically, but non-NG gives you more control over when logging initializes.

---

### 4. **SKSE::Init() with Logging Parameter** ⚠️ **Minor Difference**

**Location:** `SKSE/API.h`

**Non-NG Signature:**
```cpp
void Init(const LoadInterface* a_intfc, const bool a_log = true) noexcept;
```

**NG Signature:**
```cpp
void Init(const LoadInterface* a_intfc) noexcept;
```

**Difference:**
- **Non-NG:** Allows disabling logging during initialization via `a_log` parameter
- **NG:** Always enables logging (no option to disable)

**Impact:** Minimal - Most plugins want logging enabled anyway.

---

### 5. **SKSE::GetPluginName/Author/Version** ⚠️ **Conditional in Non-NG**

**Location:** `SKSE/API.h`

**Available Functions (Non-NG only, under `#ifdef SKYRIM_SUPPORT_AE`):**
```cpp
#ifdef SKYRIM_SUPPORT_AE
    std::string_view GetPluginName() noexcept;
    std::string_view GetPluginAuthor() noexcept;
    REL::Version     GetPluginVersion() noexcept;
#endif
```

**Use Case:**
- **Plugin metadata retrieval** for logging/debugging
- **Version checking** for compatibility
- **Plugin identification** in error messages

**Impact:** Low - These are convenience functions, but you can get this info from your plugin's own metadata.

---

## APIs Available Only in CommonLibSSE-NG

### 1. **SKSE::stl::report_and_error()** ⚠️ **Enhanced Error Handling**

**Location:** `SKSE/Impl/PCH.h`

**Available Function:**
```cpp
namespace SKSE::stl
{
    inline bool report_and_error(
        std::string_view a_msg, 
        bool a_fail = true,
        std::source_location a_loc = std::source_location::current()
    );
}
```

**Difference:**
- **NG:** Has `report_and_error()` which allows non-fatal error reporting (`a_fail = false`)
- **Non-NG:** Only has `report_and_fail()` which always terminates

**Benefit:** NG's version allows graceful error handling without crashing, useful for recoverable errors.

---

## APIs Available in Both (Same Implementation)

### ✅ **RE::MagicUtilities**
Both libraries have identical `MagicUtilities` namespace with the same functions:
- `GetAssociatedResource()`
- `GetAssociatedResourceReason()`
- `UsesResourceOnRelease()`
- `UsesResourceWhileCasting()`
- `UsesResourceWhileCharging()`

---

## Recommendations for EnhancedCombatAI

### **High Priority: Consider Non-NG for CombatUtilities**

If you plan to enhance **ranged combat AI** or **projectile dodging**, `RE::CombatUtilities::GetAngleToProjectedTarget()` could be very valuable. This function is **only available in non-NG**.

**Example Use Case:**
```cpp
// In DecisionMatrix or ActorStateObserver
if (targetIsUsingRangedWeapon) {
    // Calculate where projectile will hit
    RE::NiPoint3 hitLocation = RE::CombatUtilities::GetAngleToProjectedTarget(
        target->GetPosition(),
        self,
        projectileSpeed,
        gravity,
        RE::ACTOR_LOS_LOCATION::kHead
    );
    
    // Use this to determine dodge direction
    // or optimal positioning
}
```

### **Medium Priority: AnimationSystemUtils**

If you want to improve attack duration estimation accuracy, `AnimationSystemUtils` could help, but it's less critical since you already have feedback-based learning.

### **Low Priority: Other Differences**

The logging and plugin metadata differences are minor and don't significantly impact functionality.

---

## Migration Considerations

### **If Switching from NG to Non-NG:**

1. ✅ **No changes needed** for most APIs (RE::, SKSE:: core APIs are identical)
2. ✅ **Hook addresses** remain the same (vtable index 0xAD)
3. ⚠️ **Remove** `SKSE::log::init()` call if you're using it (or keep it - it won't hurt)
4. ⚠️ **Update** `SKSE::Init()` call to use optional `a_log` parameter if desired
5. ✅ **Gain access** to `RE::CombatUtilities` for projectile calculations

### **If Staying with NG:**

1. ✅ **Keep** enhanced error handling (`report_and_error`)
2. ✅ **Keep** automatic logging initialization
3. ❌ **Lose** `RE::CombatUtilities` (would need to implement projectile math manually)
4. ❌ **Lose** `RE::AnimationSystemUtils` (less critical)

---

## Conclusion

**For EnhancedCombatAI specifically:**

- **If you want ranged combat enhancements:** Consider non-NG for `CombatUtilities`
- **If you prioritize cross-version compatibility:** Stay with NG
- **If you want both:** You could potentially use non-NG and manually handle version differences, but NG's unified approach is cleaner

**Current Recommendation:** 
Since your project already works well with NG and focuses primarily on melee combat (parry, timed block, dodging), **staying with NG is reasonable**. However, if you plan to add sophisticated ranged combat AI features, **non-NG's CombatUtilities could be very beneficial**.

---

## Version Update Frequency

As you mentioned, **non-NG is more frequently updated**. This means:
- ✅ **More bug fixes** and improvements
- ✅ **Newer API additions** (like CombatUtilities)
- ⚠️ **May require more maintenance** if APIs change
- ⚠️ **Less stable** API surface (though still very stable)

**NG's advantage:** Unified codebase means less maintenance overhead for cross-version support.
