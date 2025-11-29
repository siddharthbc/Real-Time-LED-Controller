/*----------------------------------------------------------------------------
 * Watchdog Timer (COP) Implementation
 *
 * Part 2: Fault Protection for TR_Disable_All_IRQs
 * ECE 460/560 Final Project
 *
 * The COP (Computer Operating Properly) watchdog provides protection against
 * system hangs caused by disabled interrupts or infinite loops.
 *
 * Protection Mechanism:
 * 1. WDT_Init() enables COP with a timeout (e.g., 256ms)
 * 2. Thread_Update_Setpoint calls WDT_Feed() every 100ms
 * 3. If TR_Disable_All_IRQs fault occurs:
 *    - __disable_irq() stops all interrupts
 *    - RTOS scheduler stops (SysTick is disabled)
 *    - Threads stop running
 *    - WDT_Feed() is never called
 *    - COP times out and resets the MCU
 * 4. System restarts in a known-good state
 *----------------------------------------------------------------------------*/

#include <MKL25Z4.h>
#include "wdt.h"

/*----------------------------------------------------------------------------
 * WDT_Init - Initialize COP Watchdog Timer
 *
 * IMPORTANT: COP configuration is write-once after reset!
 * Once configured, the timeout cannot be changed and COP cannot be disabled.
 *----------------------------------------------------------------------------*/
void WDT_Init(uint8_t timeout) {
    // SIM->COPC is write-once after reset
    // Configure COP with selected timeout, using internal 1kHz LPO clock
    // COPCLKS = 0 (use LPO), COPW = 0 (normal mode, not windowed)
    SIM->COPC = (timeout & 0x03);  // Only bits [1:0] are COPT
}

/*----------------------------------------------------------------------------
 * WDT_Feed - Service the COP Watchdog
 *
 * Must write 0x55 followed by 0xAA to SIM->SRVCOP to reset the watchdog.
 * If this sequence is not performed within the timeout period, 
 * COP will force a system reset.
 *----------------------------------------------------------------------------*/
void WDT_Feed(void) {
    // COP service sequence: write 0x55 then 0xAA
    SIM->SRVCOP = 0x55;
    SIM->SRVCOP = 0xAA;
}

/*----------------------------------------------------------------------------
 * WDT_Was_Reset_By_COP - Check if COP caused last reset
 *
 * Checks the RCM (Reset Control Module) SRS0 register.
 * Bit 5 (WDOG) indicates if COP watchdog caused the reset.
 *
 * Note: This status is cleared on read or by writing to RCM_SRS0.
 *----------------------------------------------------------------------------*/
int WDT_Was_Reset_By_COP(void) {
    // RCM->SRS0 bit 5 = WDOG (Watchdog reset indicator)
    return (RCM->SRS0 & RCM_SRS0_WDOG_MASK) ? 1 : 0;
}
