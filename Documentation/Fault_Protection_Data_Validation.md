# Fault Protection: Data Validation (TR_Setpoint_High & TR_Flash_Period)

## ECE 460/560 Final Project - Part 2: Improving Robustness (Extra Credit)

---

## 1. Problem Statement

### Fault 1: TR_Setpoint_High
```c
case TR_Setpoint_High:
    g_set_current_mA = 1000;  // Sets to dangerous 1000mA
    break;
```

**Consequences:**
- LED current shoots up to 1000mA (unsafe level)
- Could damage the LED or exceed hardware limits
- System continues running with dangerous current level

### Fault 2: TR_Flash_Period
```c
case TR_Flash_Period:
    g_flash_period = 100;  // Changes flash timing
    break;
```

**Consequences:**
- Flash period changes unexpectedly
- Could cause rapid flashing (annoying/seizure risk)
- System timing becomes unpredictable

---

## 2. Solution: Periodic Data Validation

### Protection Mechanism

The solution uses **range checking** to detect and correct invalid values:

```
┌─────────────────────────────────────────────────────────────────┐
│                      NORMAL OPERATION                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Thread_Update_Setpoint (100ms period)                        │
│                                                                 │
│   loop:                                                         │
│     WDT_Feed()                                                 │
│     Validate_PID_Gains()                                       │
│     Validate_System_Data()  ◄── Check setpoint & flash period  │
│     Update_Set_Current()                                       │
│     osDelayUntil(100ms)                                        │
│                                                                 │
│   g_set_current_mA: 0-300 mA (valid range)                     │
│   g_flash_period: 10-500 ms (valid range)                      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                      FAULT: TR_Setpoint_High                    │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│   Fault Injector: g_set_current_mA = 1000  ◄── Exceeds max!    │
│                                                                 │
│   Next Thread_Update_Setpoint cycle:                           │
│     Validate_System_Data() detects 1000 > 300 (max)            │
│     → Clamps to g_set_current_mA = 300                         │
│                                                                 │
│   Result: Current limited to safe 300mA, system continues      │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 3. Implementation Details

### Configuration Switch (`config.h`)
```c
// Set to 1 to enable setpoint/flash period validation 
// (protects against TR_Setpoint_High and TR_Flash_Period faults)
#define ENABLE_DATA_VALIDATION  (1)
```

### Validation Limits (`control.h`)
```c
// Setpoint current limits
#define SET_CURRENT_MA_MIN      (0)
#define SET_CURRENT_MA_MAX      (300)   // Max safe LED current
#define SET_CURRENT_MA_DEFAULT  (0)

// Flash period limits
#define FLASH_PERIOD_MIN        (10)    // Minimum 10ms
#define FLASH_PERIOD_MAX        (500)   // Maximum 500ms
#define FLASH_PERIOD_DEFAULT    (100)   // Default value
```

### Validation Function (`control.c`)
```c
void Validate_System_Data(void) {
    // Validate g_set_current_mA (protects against TR_Setpoint_High)
    if (g_set_current_mA < SET_CURRENT_MA_MIN) {
        g_set_current_mA = SET_CURRENT_MA_MIN;
    } else if (g_set_current_mA > SET_CURRENT_MA_MAX) {
        g_set_current_mA = SET_CURRENT_MA_MAX;  // Clamp to safe max
    }
    
    // Validate g_peak_set_current_mA as well
    if (g_peak_set_current_mA < SET_CURRENT_MA_MIN) {
        g_peak_set_current_mA = SET_CURRENT_MA_MIN;
    } else if (g_peak_set_current_mA > SET_CURRENT_MA_MAX) {
        g_peak_set_current_mA = SET_CURRENT_MA_MAX;
    }
    
    // Validate g_flash_period (protects against TR_Flash_Period)
    if (g_flash_period < FLASH_PERIOD_MIN || g_flash_period > FLASH_PERIOD_MAX) {
        g_flash_period = FLASH_PERIOD_DEFAULT;
    }
    
    // Validate g_flash_duration
    if (g_flash_duration < 1 || g_flash_duration > g_flash_period) {
        g_flash_duration = g_flash_period / 4;
    }
}
```

### Thread Integration (`threads.c`)
```c
void Thread_Update_Setpoint(void * arg) {
    while (1) {
        // ... delay ...
        
#if ENABLE_DATA_VALIDATION
        Validate_System_Data();  // Check and correct data
#endif
        
        Update_Set_Current();
    }
}
```

---

## 4. Testing Procedure

### Test WITHOUT Protection
1. In `config.h`, set:
   ```c
   #define ENABLE_DATA_VALIDATION  (0)
   ```
2. Enable `TR_Setpoint_High` in `fault.c`
3. Build and run
4. **Expected**: LED current spikes to 1000mA and stays high

### Test WITH Protection
1. In `config.h`, set:
   ```c
   #define ENABLE_DATA_VALIDATION  (1)
   ```
2. Build and run
3. **Expected**: LED current briefly spikes but immediately clamps to 300mA

---

## 5. Summary

| Fault | Effect | Protection | Recovery |
|-------|--------|------------|----------|
| TR_Setpoint_High | Sets current to 1000mA | Clamp to 300mA max | Seamless |
| TR_Flash_Period | Changes flash timing | Reset to default 100ms | Seamless |

### Advantages
✅ **Seamless recovery** - No system reset needed  
✅ **Low overhead** - Simple range checks  
✅ **Protects hardware** - Prevents dangerous current levels  
✅ **Configurable** - Can disable for testing  

This protection runs every 100ms, so the maximum exposure to a corrupted value is ~100ms before correction.
