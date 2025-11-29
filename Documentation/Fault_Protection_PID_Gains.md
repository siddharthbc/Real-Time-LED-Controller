# Fault Protection: PID Gain Validation

**Course:** ECE 460/560 - Embedded System Architectures  
**Project:** ESA 2025 Final Project - Shields Up!  
**Part:** Part 2 - Improving Robustness  
**Fault:** TR_PID_FX_Gains  
**Date:** November 2025  

---

## 1. Overview

This document describes the implementation of fault protection for the **TR_PID_FX_Gains** fault, which corrupts the PID controller's integral gain.

---

## 2. The Fault

### 2.1 What the Fault Does

The fault injector corrupts the PID controller's integral gain:

```c
case TR_PID_FX_Gains:
    plantPID_FX.iGain = -1000;  // Corrupts integral gain to invalid value
    break;
```

### 2.2 Impact Without Protection

| Aspect | Normal Operation | After Fault |
|--------|-----------------|-------------|
| **iGain value** | ~1406 (I_GAIN_FX) | -1000 |
| **Feedback type** | Negative (stable) | Positive (unstable) |
| **LED current** | Stable at setpoint | Oscillates wildly or saturates |
| **System behavior** | Controlled | Uncontrolled, potentially dangerous |

### 2.3 Why It's Dangerous

The PID controller uses the formula:
```
Output = P_term + I_term - D_term
```

With a **negative iGain**:
- Positive error → Negative I_term → Duty cycle DECREASES (wrong direction!)
- This creates **positive feedback** instead of negative feedback
- System becomes unstable

---

## 3. Solution: Data Validation

### 3.1 Approach

Periodically validate PID gains against known valid ranges. If a gain is outside the valid range, restore it to the default value.

```
┌─────────────────────────────────────────────────────────────────┐
│                    VALIDATION FLOW                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Thread_Update_Setpoint (every 1ms):                          │
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐  │
│   │  Validate_PID_Gains()                                   │  │
│   │                                                         │  │
│   │  Is pGain valid?  ──► No ──► Restore P_GAIN_FX         │  │
│   │  Is iGain valid?  ──► No ──► Restore I_GAIN_FX         │  │
│   │  Is dGain valid?  ──► No ──► Restore D_GAIN_FX         │  │
│   │                                                         │  │
│   └─────────────────────────────────────────────────────────┘  │
│                           │                                     │
│                           ▼                                     │
│                  Update_Set_Current()                          │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Valid Ranges

| Gain | Minimum | Maximum | Default Value |
|------|---------|---------|---------------|
| pGain | 0 | 10 × P_GAIN_FX | P_GAIN_FX (87.5 × CTL_PERIOD) |
| iGain | 0 | 10 × I_GAIN_FX + 10000 | I_GAIN_FX (0.625 × CTL_PERIOD) |
| dGain | 0 | 10 × D_GAIN_FX + 10000 | D_GAIN_FX (0.0 × CTL_PERIOD) |

### 3.3 Detection Logic

```c
// The fault sets iGain = -1000
// Our check: if (iGain < 0 || iGain > MAX)
// -1000 < 0 → TRUE → Fault detected!
// Action: Restore iGain = I_GAIN_FX
```

---

## 4. Implementation

### 4.1 Files Modified

| File | Changes |
|------|---------|
| `config.h` | Added `ENABLE_PID_GAIN_VALIDATION` switch |
| `control.h` | Added validation limits and function declaration |
| `control.c` | Added `Validate_PID_Gains()` function |
| `threads.c` | Added validation call in `Thread_Update_Setpoint` |
| `fault.c` | Enabled `TR_PID_FX_Gains` test |

### 4.2 Configuration Switch

In `config.h`:
```c
// Set to 1 to enable PID gain validation
// Set to 0 to disable (for testing fault behavior)
#define ENABLE_PID_GAIN_VALIDATION  (1)
```

### 4.3 Validation Function

In `control.c`:
```c
void Validate_PID_Gains(void) {
    // Check P gain
    if (plantPID_FX.pGain < P_GAIN_FX_MIN || plantPID_FX.pGain > P_GAIN_FX_MAX) {
        plantPID_FX.pGain = P_GAIN_FX;  // Restore default
    }
    
    // Check I gain (detects -1000 from TR_PID_FX_Gains)
    if (plantPID_FX.iGain < I_GAIN_FX_MIN || plantPID_FX.iGain > I_GAIN_FX_MAX) {
        plantPID_FX.iGain = I_GAIN_FX;  // Restore default
    }
    
    // Check D gain
    if (plantPID_FX.dGain < D_GAIN_FX_MIN || plantPID_FX.dGain > D_GAIN_FX_MAX) {
        plantPID_FX.dGain = D_GAIN_FX;  // Restore default
    }
}
```

### 4.4 Thread Integration

In `threads.c`:
```c
void Thread_Update_Setpoint(void * arg) {
    while (1) {
        osDelayUntil(tick);
        
#if ENABLE_PID_GAIN_VALIDATION
        Validate_PID_Gains();  // Check and fix corrupted gains
#endif
        
        Update_Set_Current();
    }
}
```

---

## 5. Testing

### 5.1 Test Procedure

**Test 1: Without Protection**
1. Set `ENABLE_PID_GAIN_VALIDATION = 0` in `config.h`
2. Build and flash
3. Observe LED behavior when fault is injected (every 2 seconds)
4. Expected: LED becomes unstable, does NOT recover

**Test 2: With Protection**
1. Set `ENABLE_PID_GAIN_VALIDATION = 1` in `config.h`
2. Build and flash
3. Observe LED behavior when fault is injected
4. Expected: Brief glitch, then immediate recovery (~1ms)

### 5.2 Expected Results

| Metric | Without Protection | With Protection |
|--------|-------------------|-----------------|
| Recovery time | Never | ~1ms |
| LED stability | Unstable | Stable |
| System safety | Compromised | Maintained |

---

## 6. Timing Analysis

| Parameter | Value |
|-----------|-------|
| Validation period | 1ms (Thread_Update_Setpoint period) |
| Worst-case detection time | 1ms |
| Validation overhead | Minimal (few comparisons) |
| Fault injection period | 2 seconds |

---

## 7. Summary

The PID gain validation provides **automatic detection and correction** of corrupted PID gains. The system recovers within 1ms of fault injection, maintaining safe LED current control.

**Key Benefits:**
- ✅ Automatic fault detection
- ✅ Immediate recovery (~1ms)
- ✅ No manual intervention required
- ✅ Minimal performance overhead
- ✅ Configurable via preprocessor switch

---

**End of Document**
