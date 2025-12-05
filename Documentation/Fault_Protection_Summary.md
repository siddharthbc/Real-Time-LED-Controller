# Fault Protection Summary

## Overview

This document summarizes all fault injection tests and their protection mechanisms in the Real-Time LED Controller project.

---

## Fault Protection Status

### ✅ Protected Faults (Software Scrubbing)

| Fault | Config Flag | Protection Mechanism | Location |
|-------|-------------|---------------------|----------|
| `TR_Setpoint_High` | `ENABLE_SETPOINT_VALIDATION` | Clamps `g_set_current_mA` to 0-300mA range | `Thread_Update_Setpoint()` |
| `TR_Flash_Period` | `ENABLE_FLASH_PERIOD_VALIDATION` | Clamps `g_flash_period` to 2-180ms range | `Thread_Update_Setpoint()` |
| `TR_PID_FX_Gains` | `ENABLE_PID_GAIN_VALIDATION` | Validates and restores PID gains if corrupted | `Thread_Update_Setpoint()` → `Validate_PID_Gains()` |
| `TR_Disable_All_IRQs` | `ENABLE_COP_WATCHDOG` | COP watchdog resets MCU if threads stop running | `Thread_Update_Setpoint()` → `WDT_Feed()` |
| `TR_Disable_ADC_IRQ` | `ENABLE_ADC_IRQ_SCRUB` | Re-enables ADC0 IRQ every tick | `Thread_Update_Setpoint()` |
| `TR_Slow_TPM` | `ENABLE_TPM_SCRUB` | Restores `TPM0->MOD` to `PWM_PERIOD` if changed | `Thread_Update_Setpoint()` |
| `TR_Disable_PeriphClocks` | `ENABLE_CLOCK_SCRUB` | Re-enables ADC0, TPM0, DAC0 clocks in SCGC6 | `Thread_Update_Setpoint()` |
| `TR_Change_MCU_Clock` | `ENABLE_MCG_SCRUB` | Detects corrupted MCG->C5 and restores default | `Thread_Update_Setpoint()` |

### ⚪ Safe/Benign Faults

| Fault | Why It's Safe |
|-------|---------------|
| `TR_Setpoint_Zero` | Sets LED current to 0mA - this is a safe state (LED off) |

### ❌ Difficult/Impossible to Protect (Explained Below)

| Fault | Reason |
|-------|--------|
| `TR_LCD_mutex_Hold` | Requires mutex timeout mechanism |
| `TR_LCD_mutex_Delete` | Deleting a mutex is catastrophic - requires recreation |
| `TR_High_Priority_Thread` | COP watchdog can help, but thread monopolizes CPU |
| `TR_osKernelLock` | Kernel lock prevents all scheduling - only watchdog helps |
| `TR_Stack_Overflow` | Hardware fault - requires MPU or stack canaries |
| `TR_Fill_Queue` | Queue deadlock - requires queue monitoring |

---

## Detailed Analysis of Unprotected Faults

### TR_LCD_mutex_Hold

**Fault Behavior:** The fault thread acquires `LCD_mutex` and never releases it. Other threads waiting for the mutex block forever.

**Why Protection is Difficult:**
- Current code uses `osMutexAcquire(LCD_mutex, osWaitForever)` which blocks indefinitely
- Would need to change to timeout-based acquire: `osMutexAcquire(LCD_mutex, 1000)`

**Potential Protection:**
```c
osStatus_t status = osMutexAcquire(LCD_mutex, 1000); // 1 second timeout
if (status == osErrorTimeout) {
    // Mutex held too long - could attempt recovery
    // But we can't force release someone else's mutex
}
```

**Limitation:** Even with timeout detection, we cannot force another thread to release the mutex. The COP watchdog would eventually reset the system.

---

### TR_LCD_mutex_Delete

**Fault Behavior:** The fault deletes the LCD mutex object entirely.

**Why Protection is Difficult:**
- Once deleted, the mutex handle becomes invalid
- Any thread trying to acquire/release will cause undefined behavior
- Would need to detect invalid mutex and recreate it

**Potential Protection:**
- Could add a mutex validity check before each use
- Would require recreating the mutex if detected as invalid
- Complex to implement safely in a multi-threaded environment

---

### TR_High_Priority_Thread

**Fault Behavior:** The fault thread raises its priority to `osPriorityRealtime` and enters an infinite loop.

**Why Protection is Difficult:**
- The fault thread has higher priority than all other threads
- RTOS scheduler never gives other threads CPU time
- Even `Thread_Update_Setpoint` (which feeds the watchdog) cannot run

**Protection Strategy:**
- **COP Watchdog** is the only defense
- The watchdog is fed from `Thread_Update_Setpoint`
- When this thread can't run, watchdog times out and resets the MCU

---

### TR_osKernelLock

**Fault Behavior:** Calls `osKernelLock()` which prevents the RTOS from switching threads.

**Why Protection is Difficult:**
- Similar to disabling interrupts at the RTOS level
- Current thread keeps running, but no context switches occur
- Other threads (including watchdog feeder) are starved

**Protection Strategy:**
- **COP Watchdog** handles this case
- `osKernelLock()` doesn't disable interrupts, so ISRs still run
- But `Thread_Update_Setpoint` can't run to feed the watchdog
- System resets after watchdog timeout

---

### TR_Stack_Overflow

**Fault Behavior:** Deliberately overwrites the stack by decrementing the stack pointer and writing garbage.

**Why Protection is Difficult:**
- Corrupts return addresses, local variables, and RTOS data structures
- Behavior is undefined - may cause hard fault or silent corruption
- No software-only solution can reliably detect arbitrary memory corruption

**Potential Protections (Not Implemented):**
1. **Stack Canaries:** Place known values at stack boundaries, check periodically
2. **MPU (Memory Protection Unit):** Configure MPU to fault on stack overflow
3. **RTOS Stack Overflow Detection:** RTX has built-in stack checking (requires configuration)

---

### TR_Fill_Queue

**Fault Behavior:** Fills the ADC request queue with garbage messages in an infinite loop.

**Why Protection is Difficult:**
- The fault runs in an infinite loop, continuously filling the queue
- Legitimate requests can't be added
- Even if we drain the queue, the fault immediately refills it

**Potential Protections:**
1. **Queue Size Limiting:** The queue has a fixed size (4 entries), so it does fill up
2. **Producer Identification:** Track which thread owns queue entries
3. **Rate Limiting:** Limit how fast any single thread can add to the queue

---

## Configuration Recommendations

### For Testing Fault Behavior (Protection Disabled)

```c
#define ENABLE_PID_GAIN_VALIDATION      (0)
#define ENABLE_COP_WATCHDOG             (0)
#define ENABLE_ADC_IRQ_SCRUB            (0)
#define ENABLE_SETPOINT_VALIDATION      (0)
#define ENABLE_FLASH_PERIOD_VALIDATION  (0)
#define ENABLE_TPM_SCRUB                (0)
#define ENABLE_CLOCK_SCRUB              (0)
#define ENABLE_MCG_SCRUB                (0)
```

### For Production (All Protection Enabled)

```c
#define ENABLE_PID_GAIN_VALIDATION      (1)
#define ENABLE_COP_WATCHDOG             (1)
#define ENABLE_ADC_IRQ_SCRUB            (1)
#define ENABLE_SETPOINT_VALIDATION      (1)
#define ENABLE_FLASH_PERIOD_VALIDATION  (1)
#define ENABLE_TPM_SCRUB                (1)
#define ENABLE_CLOCK_SCRUB              (1)
#define ENABLE_MCG_SCRUB                (1)
```

---

## Summary

| Category | Count | Faults |
|----------|-------|--------|
| **Protected (Software Scrub)** | 8 | Setpoint, Flash Period, PID Gains, All IRQs, ADC IRQ, TPM, Clocks, MCG |
| **Safe/Benign** | 1 | Setpoint Zero |
| **Watchdog-Protected** | 3 | All IRQs, High Priority Thread, Kernel Lock (overlap with software) |
| **Cannot Protect in Software** | 4 | Mutex Hold, Mutex Delete, Stack Overflow, Fill Queue |

The COP watchdog provides a last-resort recovery mechanism for faults that prevent normal thread execution. For faults involving data corruption (stack overflow) or resource destruction (mutex delete), a full system reset via watchdog is the only reliable recovery.

---

## Test Results

### TR_Slow_TPM - TPM Scrub Protection

**Fault:** `TPM0->MOD = 23456` (changes PWM from 32kHz to ~1kHz)

**Protection:** `ENABLE_TPM_SCRUB` - Restores `TPM0->MOD` to `PWM_PERIOD` (750) every 1ms

| Test | Screenshot |
|------|------------|
| Before Fault (Normal PWM) | ![Before](../screenshots/Fault_TR_Slow_TPM/before_fault_normal_pwm.png) |
| After Fault WITH Protection | ![After](../screenshots/Fault_TR_Slow_TPM/after_fault_with_protection.png) |

**Result:** ✅ Protection successfully detects and recovers from corrupted TPM0->MOD within 1ms. PWM frequency remains stable at ~32kHz.
