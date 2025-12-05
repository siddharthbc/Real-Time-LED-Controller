# ECE 460/560 Project Reflections

## Lessons Learned

**The most important thing learned was the layered nature of fault protection - some faults can be caught with simple software checks, while others require hardware-level protection that runs independently of the software.**

Specifically:

- **Shared data corruption** (like `g_set_current_mA`, `g_flash_period`, PID gains) can cause immediate system misbehavior, but is easily protected with simple range clamping (just 2 lines of code per variable)

- **Hardware register corruption** (TPM0->MOD, SIM->SCGC6, MCG->C5) requires periodic "scrubbing" - restoring known-good values every 1ms from the highest-priority application thread

- **Interrupt-related faults** vary in severity - a single disabled IRQ (`ADC0_IRQn`) can be re-enabled by software, but `__disable_irq()` completely kills the system and requires hardware protection

- **RTOS scheduler attacks** (high-priority thread takeover, kernel lock) prevent all other threads from running - only the COP watchdog can recover from these since it runs on hardware

- **The COP watchdog is the last line of defense** - it handles three different fault types (`TR_Disable_All_IRQs`, `TR_High_Priority_Thread`, `TR_osKernelLock`) with a single mechanism

---

## Technical Issues

### 1. DIO 10 (PTE1) Doesn't Work as GPIO
PTE1 is hardwired to OpenSDA UART1_RX on the FRDM-KL25Z board. We had to switch the fault trigger signal from DIO 10 to **DIO 9 (PTE3)**.

**Solution in `debug.h`:**
```c
#define DBG_FAULT_POS  DBG_9  // Changed from DBG_10 to DBG_9 (PTE3)
```

### 2. Logic Analyzer Trigger Timing with System-Freezing Faults
For `TR_Disable_All_IRQs`, using "Single" trigger mode caused the scope to get stuck on "Busy" because the system froze before the trigger could complete.

**Solution:** Use "Run" mode instead of "Single" for faults that freeze the system.

### 3. COP Watchdog Timeout Constraints
Faster timeouts (32ms, 256ms) caused continuous resets because LCD initialization takes longer than the watchdog timeout. The system would reset before completing startup.

**Solution:** Use ~1024ms timeout (COPT=3) to allow initialization to complete:
```c
SIM->COPC = SIM_COPC_COPT(3);  // ~1 second timeout
```

### 4. Fault Pulse Visibility on Scope
The original 2ms fault pulse was invisible when zoomed out to see the full fault effect.

**Solution:** Added delays before fault injection to make the trigger signal clearly visible on scope captures.

### 5. Clock and MCG Scrubbing Order Matters
When scrubbing hardware registers, the order of restoration matters. Clock gates (SCGC6) must be restored before peripheral registers that depend on those clocks.

**Solution:** Restore clocks first, then dependent peripherals:
```c
SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK | SIM_SCGC6_ADC0_MASK | ...;  // Clocks first
TPM0->MOD = TPM0_MOD_VALUE;  // Then peripheral registers
```

---

## Process Changes

### 1. Relied on Logic Analyzer Over Stepping Through Code
Real-time faults can't be debugged with breakpoints since timing is critical. The scope/logic analyzer became the primary debugging tool for:
- Observing thread execution timing (DIO 6, 7, 8)
- Capturing fault injection moments (DIO 9)
- Seeing system behavior before/after faults
- Verifying watchdog recovery cycles

### 2. Systematic Before/After Capture Workflow
Developed a repeatable process for each fault:
1. Disable protection (`ENABLE_xxx = 0`)
2. Rebuild and capture "BEFORE" screenshot
3. Enable protection (`ENABLE_xxx = 1`)
4. Rebuild and capture "AFTER" screenshot
5. Compare to verify protection effectiveness

This was applied to all 10 documented faults, creating a comprehensive visual record.

### 3. Centralized Validation in High-Priority Thread
Rather than validating only at the point of use, we validate and scrub all protected resources every 1ms in `Thread_Update_Setpoint`. This catches corruption quickly regardless of where it originated:
```c
// All validations run every 1ms in Thread_Update_Setpoint
WDT_Feed();                           // Feed watchdog
Validate_PID_Gains();                 // PID corruption
NVIC_EnableIRQ(ADC0_IRQn);            // ADC IRQ scrub
TPM0->MOD = TPM0_MOD_VALUE;           // TPM scrub
SIM->SCGC6 |= REQUIRED_CLOCKS;        // Clock gate scrub
MCG->C5 = EXPECTED_MCG_C5;            // MCG scrub
// Range clamping for setpoint, flash period
```

### 4. Configuration Switches for All Protections
Used `#if ENABLE_...` preprocessor switches for all 8 protection mechanisms. This made it easy to toggle protections on/off for testing without modifying actual protection code:
```c
#define ENABLE_PID_GAIN_VALIDATION     (1)
#define ENABLE_COP_WATCHDOG            (1)
#define ENABLE_ADC_IRQ_SCRUB           (1)
#define ENABLE_SETPOINT_VALIDATION     (1)
#define ENABLE_FLASH_PERIOD_VALIDATION (1)
#define ENABLE_TPM_SCRUB               (1)
#define ENABLE_CLOCK_SCRUB             (1)
#define ENABLE_MCG_SCRUB               (1)
```

### 5. Using Delays for Scope Synchronization
For faults that freeze the system, added delays before triggering to give time for scope setup. For kernel lock testing, used a 20-second delay to allow scope configuration.

---

## Fault Categories and Protection Strategies

| Category | Faults | Protection | Recovery Time |
|----------|--------|------------|---------------|
| Shared Data | TR_Setpoint_High, TR_Flash_Period, TR_PID_FX_Gains | Range validation/clamping | ~1ms |
| Single IRQ | TR_Disable_ADC_IRQ | Periodic re-enable | ~1ms |
| Hardware Registers | TR_Slow_TPM, TR_Disable_PeriphClocks, TR_Change_MCU_Clock | Register scrubbing | ~1ms |
| System Hang | TR_Disable_All_IRQs, TR_High_Priority_Thread, TR_osKernelLock | COP Watchdog | ~1024ms |

---

## Key Takeaways

1. **Defense in depth works:** Software scrubbing catches most faults in 1ms, hardware watchdog catches the rest in ~1 second

2. **Centralized validation is powerful:** Running all checks from one high-priority thread ensures consistent protection regardless of fault source

3. **Hardware watchdog is essential:** Three different RTOS/interrupt faults all prevent software from running - only hardware protection can save the system

4. **Simple solutions are often best:** Most protections are just 1-3 lines of code, but they're highly effective

5. **Testability matters:** Having configuration switches for each protection made systematic testing practical

**Bottom line:** A combination of periodic software scrubbing (for data and register corruption) plus hardware watchdog (for complete system hangs) provides comprehensive fault tolerance with minimal code complexity.
