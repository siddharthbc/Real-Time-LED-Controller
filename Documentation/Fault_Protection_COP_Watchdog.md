# Fault Protection: TR_Disable_All_IRQs (COP Watchdog Timer)

## ECE 460/560 Final Project - Part 2: Improving Robustness

---

## 1. Problem Statement

### The Fault: TR_Disable_All_IRQs
```c
case TR_Disable_All_IRQs:
    __disable_irq();  // Disables ALL interrupts globally
    break;
```

### Consequences
When `__disable_irq()` is called:
1. **All interrupts are masked** - No ISRs can execute
2. **SysTick stops** - RTOS tick interrupt disabled
3. **RTOS scheduler stops** - No thread switching occurs
4. **PIT timer ISR (Control_HBLED) stops** - No LED current control
5. **System appears completely frozen**

### Why This Is Dangerous
- The buck converter operates without feedback control
- LED current could drift to unsafe levels
- System becomes unresponsive to all inputs
- No recovery possible without physical reset

---

## 2. Solution: COP Watchdog Timer

### What is COP?
The KL25Z includes a **COP (Computer Operating Properly)** watchdog timer module:
- Hardware timer independent of CPU and interrupts
- Continues running even when interrupts are disabled
- Forces MCU reset if not serviced ("fed") within timeout period

### COP Timeout Values (1kHz LPO Clock)
| COPT | Cycles | Approximate Timeout |
|------|--------|---------------------|
| 00   | -      | Disabled            |
| 01   | 2^5    | ~32ms               |
| 10   | 2^8    | ~256ms              |
| 11   | 2^10   | ~1024ms (actual: ~100ms due to LPO variation) |

**Note**: The actual timeout with COPT=3 is approximately **100ms** due to LPO clock variations.

### Protection Mechanism

```
┌─────────────────────────────────────────────────────────────────┐
│                      NORMAL OPERATION                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   SystemInit() ──► COP enabled (COPT=3, ~100ms timeout)        │
│                                                                 │
│   main() ──► Feed WDT during init ──► osKernelStart()          │
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │ Thread_Update_Setpoint (100ms period)                   │   │
│   │                                                         │   │
│   │   loop:                                                 │   │
│   │     WDT_Feed()  ◄── Resets ~100ms countdown            │   │
│   │     Validate_PID_Gains()                                │   │
│   │     Update_Set_Current()                                │   │
│   │     osDelayUntil(100ms)                                 │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│   COP countdown: 100ms → 0ms → [feed] → 100ms → ...            │
│   (Never reaches zero - system keeps running)                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                      FAULT CONDITION                            │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Thread_Fault_Injector:                                        │
│     __disable_irq()  ◄── All interrupts disabled               │
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │ Thread_Update_Setpoint                                  │   │
│   │                                                         │   │
│   │   (STOPPED - SysTick disabled, RTOS frozen)            │   │
│   │                                                         │   │
│   │   WDT_Feed() is NEVER CALLED!                          │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│   COP countdown: 100ms → 50ms → 0 ──► MCU RESET!               │
│                                                                 │
│   ┌─────────────────────────────────────────────────────────┐   │
│   │                    SYSTEM RESETS                         │   │
│   │   • Interrupts re-enabled                               │   │
│   │   • All peripherals re-initialized                      │   │
│   │   • LCD shows "COP Reset Recovery!"                     │   │
│   │   • Normal operation resumes                            │   │
│   └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Implementation Details

### Files Modified/Created
| File | Purpose |
|------|---------|
| `Source/wdt.h` | COP watchdog header with configuration and prototypes |
| `Source/wdt.c` | COP watchdog implementation (WDT_Init, WDT_Feed, WDT_Was_Reset_By_COP) |
| `RTE/Device/.../system_MKL25Z4.h` | Added DISABLE_WDOG control based on config.h |
| `RTE/Device/.../system_MKL25Z4.c` | Enable COP in SystemInit() when ENABLE_COP_WATCHDOG=1 |
| `Source/main.c` | Feed watchdog during initialization, display recovery message |
| `Source/MMA8451.c` | Feed watchdog during long accelerometer init delays |
| `Source/threads.c` | Feed watchdog from Thread_Update_Setpoint |
| `Include/config.h` | Added ENABLE_COP_WATCHDOG switch |

### Configuration Switch (`config.h`)
```c
// Set to 1 to enable COP Watchdog Timer (protects against TR_Disable_All_IRQs fault)
// Set to 0 to disable watchdog (to observe fault behavior without protection)
// WARNING: Once enabled, COP cannot be disabled without a reset
#define ENABLE_COP_WATCHDOG  (1)
```

---

## 4. Key Implementation Challenges

### Challenge 1: COP is Write-Once
The SIM->COPC register can only be written **once** after reset. This means:
- COP must be configured in `SystemInit()` before `main()` runs
- Cannot change timeout or disable COP after configuration

### Challenge 2: Short Timeout During Initialization
With COPT=3, the timeout is only ~100ms. The initialization sequence includes:
- LCD initialization
- Accelerometer I2C communication
- Long delays (100ms + 500ms in `init_mma()`)

**Solution**: Feed the watchdog during long initialization:
```c
// In init_mma() - break 500ms delay into chunks
for (int i = 0; i < 50; i++) {
    Delay(10);
    WDT_Feed();
}
```

### Challenge 3: System Header Modification
The default `system_MKL25Z4.h` sets `DISABLE_WDOG=1`, which disables COP in `SystemInit()`.

**Solution**: Modified to check our config:
```c
#include "config.h"
#if ENABLE_COP_WATCHDOG
  #define DISABLE_WDOG  0   // Don't disable COP
#else
  #define DISABLE_WDOG  1   // Disable COP (default)
#endif
```

---

## 5. Code Changes

### system_MKL25Z4.c - COP Initialization
```c
void SystemInit (void) {
#if 0  // Force COP enable
  SIM->COPC = (uint32_t)0x00u;  // Disabled
#else
  SIM->COPC = SIM_COPC_COPT(3);  // Enable with max timeout
#endif
  // ... rest of init
}
```

### main.c - Initialization with Watchdog Feeds
```c
int main (void) {
#if ENABLE_COP_WATCHDOG
    int cop_reset = WDT_Was_Reset_By_COP();
#endif
    
    Init_Debug_Signals();
    Init_RGB_LEDs();
    
#if ENABLE_COP_WATCHDOG
    WDT_Feed();  // Feed before LCD init
#endif
    
    LCD_Init();
    
#if ENABLE_COP_WATCHDOG
    WDT_Feed();  // Feed after LCD init
#endif
    
    // ... more init with feeds ...
    
#if ENABLE_COP_WATCHDOG
    if (cop_reset) {
        LCD_Text_PrintStr_RC(1,0, "COP Reset Recovery!");
        // Show message with watchdog feeds during delay
    }
#endif
    
    // ... continue to RTOS start
}
```

### MMA8451.c - Long Delays with Feeds
```c
int init_mma() {
#if ENABLE_COP_WATCHDOG
    // Break 100ms delay into 10ms chunks
    for (int i = 0; i < 10; i++) {
        Delay(10);
        WDT_Feed();
    }
#else
    Delay(100);
#endif
    
    // ... I2C operations ...
    
#if ENABLE_COP_WATCHDOG
    // Break 500ms delay into 10ms chunks
    for (int i = 0; i < 50; i++) {
        Delay(10);
        WDT_Feed();
    }
#else
    Delay(500);
#endif
    
    // ... rest of init
}
```

### threads.c - Periodic Feeding
```c
void Thread_Update_Setpoint(void * arg) {
    while (1) {
        tick += THREAD_UPDATE_SETPOINT_PERIOD_TICKS;  // 100ms
        osDelayUntil(tick);
        
#if ENABLE_COP_WATCHDOG
        WDT_Feed();  // Service watchdog every 100ms
#endif
        
#if ENABLE_PID_GAIN_VALIDATION
        Validate_PID_Gains();
#endif
        
        Update_Set_Current();
    }
}
```

---

## 6. Testing Procedure

### Test WITHOUT Protection (observe fault)
1. In `config.h`, set:
   ```c
   #define ENABLE_COP_WATCHDOG  (0)
   ```
2. In `fault.c`, enable:
   ```c
   TR_Disable_All_IRQs,
   ```
3. Build and run (without debugger)
4. **Expected**: System freezes permanently after fault

### Test WITH Protection
1. In `config.h`, set:
   ```c
   #define ENABLE_COP_WATCHDOG  (1)
   ```
2. Build and run (without debugger)
3. **Expected**: 
   - System runs normally
   - When fault triggers, system freezes briefly (~100ms)
   - MCU resets automatically
   - LCD shows "COP Reset Recovery!"
   - Normal operation resumes
   - Cycle repeats when fault triggers again

---

## 7. Summary

The COP watchdog timer provides **hardware-level protection** against system hangs:

✅ **Hardware-based** - Runs independently of software state  
✅ **Cannot be bypassed** - Even with interrupts disabled  
✅ **Automatic recovery** - MCU resets to known-good state  
✅ **User feedback** - "COP Reset Recovery!" message shown  
✅ **Configurable** - Can be disabled for testing  

### Trade-offs
- Requires feeding watchdog during long initialization delays
- ~100ms timeout means frequent feeds needed
- System files must be modified (write-once COP register)

This completes the fault protection for TR_Disable_All_IRQs as required for ECE 460/560 Part 2.
