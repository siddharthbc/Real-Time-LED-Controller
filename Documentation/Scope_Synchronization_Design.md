# Scope Waveform Synchronization - Design Document

**Course:** ECE 460/560 - Embedded System Architectures  
**Project:** ESA 2025 Final Project - Shields Up!  
**Author:** [Your Name]  
**Date:** November 2025  

---

## Table of Contents

1. [Introduction](#1-introduction)
2. [System Architecture](#2-system-architecture)
3. [Problem Statement](#3-problem-statement)
4. [Solution Design](#4-solution-design)
5. [Implementation Details](#5-implementation-details)
6. [Comparison of Approaches](#6-comparison-of-approaches)
7. [Testing and Verification](#7-testing-and-verification)
8. [Appendix A: State Machine Diagram](#appendix-a-state-machine-diagram)
9. [Appendix B: Code Snippets](#appendix-b-code-snippets)

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

## 2. System Architecture

### 2.1 Hardware/Software Overview

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           HARDWARE LAYER                                     │
│  ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐    ┌───────────┐  │
│  │   ADC   │    │   DAC   │    │   PWM   │    │  LCD +  │    │Accelero-  │  │
│  │ (sense) │    │(setpoint│    │  (LED)  │    │Touchscrn│    │  meter    │  │
│  └────┬────┘    └────▲────┘    └────▲────┘    └────┬────┘    └─────┬─────┘  │
└───────┼──────────────┼──────────────┼──────────────┼────────────────┼───────┘
        │              │              │              │                │
        ▼              │              │              ▼                ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                              SOFTWARE LAYER                                │
│                                                                            │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    ISR: Control_HBLED (~10.7 kHz)                   │   │
│  │  • Reads ADC (measured current)                                     │   │
│  │  • Runs PID control algorithm                                       │   │
│  │  • Updates PWM duty cycle                                          │   │
│  │  • Fills waveform sample buffers                                    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                              ▲                                             │
│                              │ ADC Interrupt                               │
│  ┌───────────────────────────┴─────────────────────────────────────────┐   │
│  │                         RTOS KERNEL                                 │   │
│  │   ┌──────────────┐ ┌──────────────┐ ┌───────────────┐ ┌──────────┐  │   │
│  │   │Thread_Update │ │Thread_Draw_  │ │Thread_Draw_UI │ │Thread_   │  │   │
│  │   │  Setpoint    │ │  Waveforms   │ │   Controls    │ │Read_Accel│  │   │
│  │   │  (1ms)       │ │  (100ms)     │ │   (100ms)     │ │ (varies) │  │   │
│  │   └──────────────┘ └──────────────┘ └───────────────┘ └──────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Component Responsibilities

#### ISR: Control_HBLED() (~10.7 kHz)

**This is the heart of the system - runs as an interrupt, not a thread!**

| Step | Action | Description |
|------|--------|-------------|
| 1 | **Read ADC** | Get ADC result, convert to `g_measured_current_mA` |
| 2 | **Fill Waveform Buffers** | Store samples in `g_meas_sample[]` and `g_set_sample[]` |
| 3 | **Run PID Control** | Calculate error, run PID algorithm, get duty cycle |
| 4 | **Update PWM** | Clamp duty cycle, write to TPM0 hardware |

**Key Characteristics:**
- Runs at **~10.7 kHz** (every ~93 microseconds)
- Triggered by ADC conversion complete interrupt
- Must be FAST - no blocking or waiting allowed
- Directly controls LED current via PWM

#### Thread_Update_Setpoint (1ms period, High Priority)

| Step | Action | Description |
|------|--------|-------------|
| 1 | **Feed Watchdog** | `WDT_Feed()` - prevents system reset |
| 2 | **Validate Data** | Check PID gains, clamp setpoint/flash period |
| 3 | **Update Setpoint** | Generate flash waveform pattern, update DAC |

**Key Characteristics:**
- **Highest priority thread** - runs every 1ms reliably
- Generates the **flash waveform pattern** (on/off timing)
- Central location for **all fault protections**

#### Thread_Draw_Waveforms (100ms period, Above Normal Priority)

| Step | Action | Description |
|------|--------|-------------|
| 1 | **Check for Data** | Poll `g_scope_state == Full` or wait for event flag |
| 2 | **Acquire LCD Mutex** | Get exclusive LCD access |
| 3 | **Draw Waveforms** | `UI_Draw_Waveforms()` - render to LCD |
| 4 | **Re-arm** | Set state back to Armed for next trigger |

**Key Characteristics:**
- Reads the **shared waveform buffers** filled by ISR
- Must **coordinate with ISR** to avoid data corruption
- Uses **LCD mutex** for exclusive LCD access

#### Thread_Draw_UI_Controls (100ms period, Normal Priority)

| Step | Action | Description |
|------|--------|-------------|
| 1 | **Acquire LCD Mutex** | Wait for exclusive LCD access |
| 2 | **Update UI** | Refresh current values, sliders, buttons |
| 3 | **Release LCD Mutex** | Allow other threads to use LCD |

#### Thread_Read_Accelerometer (Variable period)

| Step | Action | Description |
|------|--------|-------------|
| 1 | **Read MMA8451** | I2C communication with accelerometer |
| 2 | **Calculate Tilt** | Convert to roll and pitch angles |
| 3 | **Update Flash Period** | `g_flash_period = 30 + roll` (2-180ms range) |

**Key Characteristic:** Tilting the board changes LED flash rate!

### 2.3 Shared Data Flow

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         SHARED VARIABLES                                 │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  CONTROL DATA (written by threads, read by ISR)                 │    │
│  │  • g_set_current_mA      - target LED current                   │    │
│  │  • g_flash_period        - flash timing (from accelerometer)    │    │
│  │  • g_enable_control      - control on/off                       │    │
│  │  • plantPID_FX           - PID gain parameters                  │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  FEEDBACK DATA (written by ISR, read by threads)                │    │
│  │  • g_measured_current_mA - actual LED current                   │    │
│  │  • g_duty_cycle          - current PWM duty cycle               │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────────────┐    │
│  │  WAVEFORM BUFFERS (written by ISR, read by Thread_Draw_Waveforms)│   │
│  │  • g_meas_sample[960]    - measured current history             │    │
│  │  • g_set_sample[960]     - setpoint history                     │    │
│  │  • g_scope_state         - synchronization state machine        │    │
│  └─────────────────────────────────────────────────────────────────┘    │
│                                                                          │
└─────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Problem Statement

### 3.1 Original System Behavior (Before Synchronization)

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

### 3.2 The Race Condition Problem

#### Visual Representation of the Original Problem

```
TIME ──────────────────────────────────────────────────────────────────────►

ISR (10.7 kHz):    ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃ ┃W┃
                   ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤ ├─┤
                   ~93µs each write to g_meas_sample[]

Thread (100ms):                          ┃───────── R ─────────┃
                                         │                     │
                                       START                  END
                                       READ                   READ
                                         
  PROBLEM: While thread reads buffer[200-600], ISR writes buffer[300-500]!
           Result: Corrupted data, visual glitches
```

#### What Happens Without Synchronization

```
BEFORE SYNC: Random Waveform Position (No Trigger)

Time 0ms:    [────────⌇⌇⌇────────]    Waveform in middle
             └─ buffer read ─────┘

Time 100ms:  [⌇⌇⌇────────────────]    Waveform at left  
             └─ buffer read ─────┘

Time 200ms:  [────────────────⌇⌇⌇]    Waveform at right
             └─ buffer read ─────┘

Result: Waveform "jumps around" on LCD - impossible to analyze!
```

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

### 3.3 Synchronization Requirements

From the project specification (Page 10):

1. `Control_HBLED` must NOT start filling buffers until setpoint exceeds threshold
2. `Control_HBLED` must STOP filling buffers when they are full
3. TDW must NOT start plotting until buffers are full
4. `Control_HBLED` must NOT start filling again until TDW has finished plotting

---

## 4. Solution Design

### 4.1 The Solution: Coordinated Handshaking

#### Visual Representation of the Solution

```
TIME ──────────────────────────────────────────────────────────────────────►

State:    [────── ARMED ──────][── TRIGGERED ──][FULL][─ PLOTTING ─][ARMED]
                               │                │    │             │
ISR:            NO WRITES      │◄── WRITES ───►│    │  NO WRITES  │
                               │   960 samples  │    │             │
                               │                │    │             │
Thread:         POLLING        │    POLLING     │    │◄── READS ──►│ RE-ARM
                               │                │    │             │
                               ▲                     ▲             ▲
                            Trigger              Buffer         Thread
                           Detected               Full          Done

Result: Clean handoff - ISR and Thread NEVER access buffer simultaneously!
```

#### After Sync: Stable Triggered Waveform

```
AFTER SYNC: Consistent Trigger Position

Time 0ms:    [⌇⌇⌇────────────────]    Rising edge at left
             └─ triggered capture ─┘

Time 100ms:  [⌇⌇⌇────────────────]    Rising edge at left  
             └─ triggered capture ─┘

Time 200ms:  [⌇⌇⌇────────────────]    Rising edge at left
             └─ triggered capture ─┘

Result: Waveform is STABLE - always triggered at same point!
        Just like a real oscilloscope!
```

### 4.2 State Machine Architecture

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

### 4.3 State Descriptions

| State | Description | ISR Action | Thread Action |
|-------|-------------|------------|---------------|
| **Armed** | Waiting for trigger condition | Monitor setpoint, do NOT write | Sleep/poll state |
| **Triggered** | Actively capturing samples | Write to buffers | Sleep/poll state |
| **Full** | Buffer complete, waiting for thread | Do NOT write | Detect and take ownership |
| **Plotting** | Thread reading buffers | Do NOT write | Read buffers, draw LCD |

### 4.4 Trigger Condition

The trigger fires when the setpoint current crosses the threshold from low to high:

```c
if ((prev_set_current_mA < threshold_mA) && (g_set_current_mA >= threshold_mA))
```

Where `threshold_mA = 1` (defined as `SCOPE_TRIGGER_THRESHOLD_mA`)

### 4.5 Two Implementation Approaches

Per the project requirements, ECE 560 students implement both approaches:

1. **Approach 1**: State Machine without RTOS mechanisms (polling)
2. **Approach 2**: State Machine with RTOS Event Flags

A preprocessor switch selects the active approach:

```c
#define SCOPE_SYNC_WITH_RTOS  (0)  // 0 = State Machine, 1 = Event Flags
```

---

## 5. Implementation Details

### 5.1 Files Modified

| File | Changes |
|------|---------|
| `config.h` | Added `SCOPE_SYNC_WITH_RTOS` configuration switch |
| `control.h` | Updated `SCOPE_STATE_E` enum, added event flag declarations |
| `control.c` | Implemented state machine in `Control_HBLED()` |
| `threads.c` | Implemented synchronization in `Thread_Draw_Waveforms()` |

### 5.2 Approach 1: State Machine (Without RTOS)

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

### 5.3 Approach 2: RTOS Event Flags

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

## 6. Comparison of Approaches

### 6.1 Feature Comparison

| Feature | Approach 1 (State Machine) | Approach 2 (Event Flags) |
|---------|---------------------------|-------------------------|
| Sync mechanism | Shared volatile variable | RTOS event flags |
| ISR notification | Sets `g_scope_state = Full` | Calls `osEventFlagsSet()` |
| Thread detection | Polls variable | Calls `osEventFlagsWait()` |
| RTOS required | No | Yes |
| Flag management | Manual | Auto-clear on read |
| Portability | High (any system) | RTOS-dependent |
| Complexity | Lower | Slightly higher |

### 6.2 Before vs After Comparison

| Aspect | Before (No Sync) | After (With Sync) |
|--------|------------------|-------------------|
| Display stability | Waveform jumps randomly | Stable, fixed position |
| Trigger point | Random | Always at left edge |
| Data coherence | May be corrupted (torn) | Always complete |
| Buffer access | Uncoordinated | Mutually exclusive |
| Analysis capability | Difficult | Easy |

### 6.3 Timing Analysis

**Buffer Fill Time:**
- 960 samples at ~10.7 kHz ISR rate
- Fill time ≈ 960 / 10,700 = **~90 ms**

**Thread Period:**
- `THREAD_DRAW_WAVEFORM_PERIOD_TICKS` = 100ms

**Synchronization Latency:**
- Maximum delay from Full to Plotting: ~100ms (one thread period)

---

## 7. Testing and Verification

### 7.1 Expected Behavior

With synchronization enabled:

1. LCD waveform should be **stable** (not jumping)
2. Rising edge of setpoint should appear at **left edge** of display
3. Blue (setpoint) and orange (measured) traces should be **coherent**
4. Display should update approximately every flash period

### 7.2 Debug Signals

Use these debug signals with a logic analyzer to verify timing:

| Signal | Pin | Purpose |
|--------|-----|---------|
| DBG_CONTROLLER | PTB8 | Control_HBLED execution |
| DBG_T_DRAW_WVFMS | PTB10 | Thread_Draw_Waveforms execution |
| DBG_PENDING_WVFM | PTB9 | Scope buffer filling (Triggered state) |

### 7.3 Switching Between Approaches

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
