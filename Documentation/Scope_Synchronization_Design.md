# Scope Waveform Synchronization - Design Document

**Course:** ECE 460/560 - Embedded System Architectures  
**Project:** ESA 2025 Final Project - Shields Up!  
**Author:** [Your Name]  
**Date:** November 2025  

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [Problem Statement](#2-problem-statement)
3. [Solution Design](#3-solution-design)
4. [Implementation Details](#4-implementation-details)
5. [Comparison of Approaches](#5-comparison-of-approaches)
6. [Testing and Verification](#6-testing-and-verification)

---

## 1. Introduction

### 1.1 System Overview

The Real-Time LED Controller system uses a buck converter to control LED current. The system includes:

- **ADC0 IRQ Handler**: Runs at ~10.7 kHz, executes `Control_HBLED()` for PID control
- **Thread_Draw_Waveforms (TDW)**: Runs every 100ms, displays waveforms on LCD
- **Shared Buffers**: `g_set_sample[960]` and `g_meas_sample[960]` store waveform data

### 1.2 Purpose of This Document

This document describes the synchronization mechanism implemented between the ADC ISR and the waveform display thread to ensure:

1. Stable, triggered waveform display (like an oscilloscope)
2. Data integrity (no corruption from concurrent access)

---

## 2. Problem Statement

### 2.1 Original System Behavior (Before Synchronization)

In the original implementation, the ISR and thread operated independently without coordination:

**ISR Behavior (Control_HBLED):**
```c
// ORIGINAL CODE - Always writes to buffer
g_meas_sample[sample_idx] = res;
g_set_sample[sample_idx] = g_set_current_code;
sample_idx++;
if (sample_idx >= SAM_BUF_SIZE) { 
    sample_idx = 0;  // Wrap around and continue
}
```

**Thread Behavior (Thread_Draw_Waveforms):**
```c
// ORIGINAL CODE - Always reads buffer
while (1) {
    osDelayUntil(tick);
    osMutexAcquire(LCD_mutex, osWaitForever);
    UI_Draw_Waveforms();  // Always draw
    osMutexRelease(LCD_mutex);
}
```

### 2.2 Problems Identified

#### Problem 1: Data Tearing (Race Condition)

The ISR continuously writes to the circular buffer at ~10.7 kHz. When the thread reads the buffer, the ISR may be in the middle of a write cycle, resulting in:

- **Torn data**: Part of the buffer contains new data, part contains old data
- **Inconsistent captures**: No guarantee of a coherent 960-sample window

```
Buffer state when thread reads:
[NEW][NEW][NEW]...[NEW] | [OLD][OLD][OLD]...[OLD]
     indices 0-500      |      indices 501-959
     (current cycle)    |      (previous cycle)
```

#### Problem 2: Unstable Display (No Triggering)

Without a trigger mechanism:

- Waveform appears at random positions on the display
- Each refresh shows a different portion of the signal
- Makes analysis of the control system response impossible

### 2.3 Synchronization Requirements

From the project specification (Page 10):

1. `Control_HBLED` must NOT start filling buffers until setpoint exceeds threshold
2. `Control_HBLED` must STOP filling buffers when they are full
3. TDW must NOT start plotting until buffers are full
4. `Control_HBLED` must NOT start filling again until TDW has finished plotting

---

## 3. Solution Design

### 3.1 State Machine Architecture

We implemented a 4-state finite state machine to coordinate the ISR and thread:

```
┌─────────┐   trigger    ┌───────────┐   buffer full   ┌──────┐
│  Armed  │ ───────────► │ Triggered │ ──────────────► │ Full │
└─────────┘              └───────────┘                 └──────┘
     ▲                                                     │
     │                   ┌──────────┐   thread starts      │
     └────── done ────── │ Plotting │ ◄────────────────────┘
                         └──────────┘
```

### 3.2 State Descriptions

| State | Description | ISR Action | Thread Action |
|-------|-------------|------------|---------------|
| **Armed** | Waiting for trigger condition | Monitor setpoint, do NOT write | Sleep/poll state |
| **Triggered** | Actively capturing samples | Write to buffers | Sleep/poll state |
| **Full** | Buffer complete, waiting for thread | Do NOT write | Detect and take ownership |
| **Plotting** | Thread reading buffers | Do NOT write | Read buffers, draw LCD |

### 3.3 Trigger Condition

The trigger fires when the setpoint current crosses the threshold from low to high:

```c
if ((prev_set_current_mA < threshold_mA) && (g_set_current_mA >= threshold_mA))
```

Where `threshold_mA = 1` (defined as `SCOPE_TRIGGER_THRESHOLD_mA`)

### 3.4 Two Implementation Approaches

Per the project requirements, ECE 560 students implement both approaches:

1. **Approach 1**: State Machine without RTOS mechanisms (polling)
2. **Approach 2**: State Machine with RTOS Event Flags

A preprocessor switch selects the active approach:

```c
#define SCOPE_SYNC_WITH_RTOS  (0)  // 0 = State Machine, 1 = Event Flags
```

---

## 4. Implementation Details

### 4.1 Files Modified

| File | Changes |
|------|---------|
| `config.h` | Added `SCOPE_SYNC_WITH_RTOS` configuration switch |
| `control.h` | Updated `SCOPE_STATE_E` enum, added event flag declarations |
| `control.c` | Implemented state machine in `Control_HBLED()` |
| `threads.c` | Implemented synchronization in `Thread_Draw_Waveforms()` |

### 4.2 Approach 1: State Machine (Without RTOS)

#### Configuration
```c
#define SCOPE_SYNC_WITH_RTOS  (0)
```

#### ISR Code (control.c)
```c
switch (g_scope_state) {
    case Armed:
        // Waiting for trigger - NOT writing to buffers
        if ((prev_set_current_mA < threshold_mA) && 
            (g_set_current_mA >= threshold_mA)) {
            // Trigger detected
            sample_idx = 0;
            g_scope_state = Triggered;
            g_meas_sample[sample_idx] = res;
            g_set_sample[sample_idx] = g_set_current_code;
            sample_idx++;
        }
        break;
        
    case Triggered:
        // Filling buffers
        g_meas_sample[sample_idx] = res;
        g_set_sample[sample_idx] = g_set_current_code;
        sample_idx++;
        
        if (sample_idx >= SAM_BUF_SIZE) {
            sample_idx = 0;
            g_scope_state = Full;  // Signal via state variable
        }
        break;
        
    case Full:
    case Plotting:
        // NOT writing - waiting for thread
        break;
}
```

#### Thread Code (threads.c)
```c
if (g_scope_state == Full) {
    g_scope_state = Plotting;  // Take ownership
    
    osMutexAcquire(LCD_mutex, osWaitForever);
    UI_Draw_Waveforms();
    osMutexRelease(LCD_mutex);
    
    g_scope_state = Armed;  // Re-arm for next trigger
}
```

#### Characteristics
- **Signaling**: Shared volatile variable `g_scope_state`
- **Thread detection**: Polls state variable each period
- **RTOS dependency**: None for synchronization
- **Simplicity**: Straightforward implementation

### 4.3 Approach 2: RTOS Event Flags

#### Configuration
```c
#define SCOPE_SYNC_WITH_RTOS  (1)
```

#### Event Flag Definitions (control.h)
```c
#define SCOPE_FLAG_BUFFER_FULL    (1U << 0)
#define SCOPE_FLAG_PLOT_COMPLETE  (1U << 1)
extern osEventFlagsId_t scope_event_flags;
```

#### ISR Code (control.c)
```c
case Triggered:
    g_meas_sample[sample_idx] = res;
    g_set_sample[sample_idx] = g_set_current_code;
    sample_idx++;
    
    if (sample_idx >= SAM_BUF_SIZE) {
        sample_idx = 0;
        g_scope_state = Full;
        // RTOS: Signal thread with event flag
        osEventFlagsSet(scope_event_flags, SCOPE_FLAG_BUFFER_FULL);
    }
    break;
```

#### Thread Code (threads.c)
```c
uint32_t flags = osEventFlagsWait(scope_event_flags, 
                                   SCOPE_FLAG_BUFFER_FULL,
                                   osFlagsWaitAny, 0);

if (flags == SCOPE_FLAG_BUFFER_FULL) {
    g_scope_state = Plotting;
    
    osMutexAcquire(LCD_mutex, osWaitForever);
    UI_Draw_Waveforms();
    osMutexRelease(LCD_mutex);
    
    g_scope_state = Armed;
}
```

#### Initialization (threads.c)
```c
void Create_OS_Objects(void) {
    LCD_Create_OS_Objects();
    
#if SCOPE_SYNC_WITH_RTOS
    scope_event_flags = osEventFlagsNew(NULL);
#endif
    
    // ... create threads ...
}
```

#### Characteristics
- **Signaling**: RTOS event flags + state variable
- **Thread detection**: `osEventFlagsWait()` with timeout=0 (non-blocking)
- **RTOS dependency**: Requires CMSIS-RTOS2
- **Auto-clear**: Event flag automatically cleared when read

---

## 5. Comparison of Approaches

### 5.1 Feature Comparison

| Feature | Approach 1 (State Machine) | Approach 2 (Event Flags) |
|---------|---------------------------|-------------------------|
| Sync mechanism | Shared volatile variable | RTOS event flags |
| ISR notification | Sets `g_scope_state = Full` | Calls `osEventFlagsSet()` |
| Thread detection | Polls variable | Calls `osEventFlagsWait()` |
| RTOS required | No | Yes |
| Flag management | Manual | Auto-clear on read |
| Portability | High (any system) | RTOS-dependent |
| Complexity | Lower | Slightly higher |

### 5.2 Before vs After Comparison

| Aspect | Before (No Sync) | After (With Sync) |
|--------|------------------|-------------------|
| Display stability | Waveform jumps randomly | Stable, fixed position |
| Trigger point | Random | Always at left edge |
| Data coherence | May be corrupted (torn) | Always complete |
| Buffer access | Uncoordinated | Mutually exclusive |
| Analysis capability | Difficult | Easy |

### 5.3 Timing Analysis

**Buffer Fill Time:**
- 960 samples at ~10.7 kHz ISR rate
- Fill time ≈ 960 / 10,700 = **~90 ms**

**Thread Period:**
- `THREAD_DRAW_WAVEFORM_PERIOD_TICKS` = 100ms

**Synchronization Latency:**
- Maximum delay from Full to Plotting: ~100ms (one thread period)

---

## 6. Testing and Verification

### 6.1 Expected Behavior

With synchronization enabled:

1. LCD waveform should be **stable** (not jumping)
2. Rising edge of setpoint should appear at **left edge** of display
3. Blue (setpoint) and orange (measured) traces should be **coherent**
4. Display should update approximately every flash period

### 6.2 Debug Signals

Use these debug signals with a logic analyzer to verify timing:

| Signal | Pin | Purpose |
|--------|-----|---------|
| DBG_CONTROLLER | PTB8 | Control_HBLED execution |
| DBG_T_DRAW_WVFMS | PTB10 | Thread_Draw_Waveforms execution |
| DBG_PENDING_WVFM | PTB9 | Scope buffer filling (Triggered state) |

### 6.3 Switching Between Approaches

To test each approach:

1. In `config.h`, set:
   - `#define SCOPE_SYNC_WITH_RTOS (0)` for State Machine
   - `#define SCOPE_SYNC_WITH_RTOS (1)` for Event Flags

2. Rebuild and flash the project

3. Verify waveform stability on LCD

---

## Appendix A: State Machine Diagram

```
                              ┌─────────────────────┐
                              │                     │
     ┌────────────────────────┴───────┐             │
     ▼                                │             │
┌──────────┐                          │             │
│  ARMED   │                          │             │
│          │ Waiting for trigger      │             │
│          │ ISR: NOT writing         │             │
└────┬─────┘                          │             │
     │                                │             │
     │ Trigger: prev < 1mA AND        │             │
     │          curr >= 1mA           │             │
     ▼                                │             │
┌───────────┐                         │             │
│ TRIGGERED │                         │             │
│           │ ISR filling buffers     │             │
│           │ (960 samples)           │             │
└─────┬─────┘                         │             │
      │                               │             │
      │ Buffer full                   │             │
      │ (sample_idx >= 960)           │             │
      ▼                               │             │
┌──────────┐                          │             │
│   FULL   │                          │             │
│          │ ISR waiting              │             │
│          │ Thread detects           │             │
└─────┬────┘                          │             │
      │                               │             │
      │ Thread takes ownership        │             │
      ▼                               │             │
┌──────────┐                          │             │
│ PLOTTING │                          │             │
│          │ Thread reading           │             │
│          │ ISR waiting              │             │
└─────┬────┘                          │             │
      │                               │             │
      │ Thread done drawing           │             │
      └───────────────────────────────┘             │
                                                    │
```

---

## Appendix B: Code Snippets

### B.1 config.h Addition
```c
// Scope Synchronization Configuration
// Set to 1 to use RTOS mechanisms (event flags)
// Set to 0 to use state machine approach (polling)
#define SCOPE_SYNC_WITH_RTOS  (0)
```

### B.2 control.h State Definition
```c
typedef enum {Armed, Triggered, Full, Plotting} SCOPE_STATE_E;

#if SCOPE_SYNC_WITH_RTOS
#include <cmsis_os2.h>
#define SCOPE_FLAG_BUFFER_FULL    (1U << 0)
#define SCOPE_FLAG_PLOT_COMPLETE  (1U << 1)
extern osEventFlagsId_t scope_event_flags;
#endif
```

---

**End of Document**
