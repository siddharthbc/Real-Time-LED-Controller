# Fault Test: TR_Slow_TPM

## Fault Description

**Fault Injection:** `TPM0->MOD = 23456`

This fault corrupts the Timer/PWM Module 0 modulo register, changing the PWM period from 750 to 23456. This slows the PWM switching frequency from ~32 kHz to ~1 kHz.

## Protection Mechanism

**Config Flag:** `ENABLE_TPM_SCRUB`

**Protection Code (in `Thread_Update_Setpoint`):**
```c
#if ENABLE_TPM_SCRUB
    if (TPM0->MOD != PWM_PERIOD) {
        TPM0->MOD = PWM_PERIOD;
    }
#endif
```

The protection runs every 1ms and checks if `TPM0->MOD` has been changed. If corrupted, it immediately restores the correct value (`PWM_PERIOD = 750`).

## Test Results

### Before Fault (Normal Operation)
![Before Fault](before_fault_normal_pwm.png)

- PWM frequency: ~32 kHz
- PWM period: ~31.25 µs
- LED current control operating normally

### After Fault WITH Protection Enabled
![After Fault With Protection](after_fault_with_protection.png)

- Protection detects corrupted `TPM0->MOD` within 1ms
- `TPM0->MOD` restored to 750
- PWM frequency returns to ~32 kHz
- LED current control continues operating normally

## Conclusion

The TPM scrub protection successfully detects and recovers from the `TR_Slow_TPM` fault within 1ms, preventing any lasting impact on the PWM switching frequency and LED current control.
