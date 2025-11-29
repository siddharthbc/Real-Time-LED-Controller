/*----------------------------------------------------------------------------
 * Watchdog Timer (COP) Header
 *
 * Part 2: Fault Protection for TR_Disable_All_IRQs
 * ECE 460/560 Final Project
 *----------------------------------------------------------------------------*/
#ifndef WDT_H
#define WDT_H

#include <stdint.h>

/*----------------------------------------------------------------------------
 * COP (Computer Operating Properly) Watchdog Configuration
 * 
 * The KL25Z uses the COP module as its watchdog timer.
 * SIM->COPC register controls COP operation:
 *   - COPW: Windowed mode (0 = normal, 1 = windowed)
 *   - COPCLKS: Clock source (0 = internal 1kHz LPO, 1 = bus clock)
 *   - COPT[1:0]: Timeout selection
 *     00 = Disabled
 *     01 = 2^5 LPO cycles (~32ms at 1kHz)
 *     10 = 2^8 LPO cycles (~256ms at 1kHz)
 *     11 = 2^10 LPO cycles (~1024ms at 1kHz)
 *
 * Feeding the watchdog:
 *   Write 0x55 then 0xAA to SIM->SRVCOP to service (feed) the watchdog
 *----------------------------------------------------------------------------*/

// COP Timeout configuration
// These values are written to SIM->COPC
#define COP_DISABLED        (0x00)  // COP disabled
#define COP_TIMEOUT_32MS    (0x01)  // ~32ms timeout (2^5 LPO cycles)
#define COP_TIMEOUT_256MS   (0x02)  // ~256ms timeout (2^8 LPO cycles)  
#define COP_TIMEOUT_1024MS  (0x03)  // ~1024ms timeout (2^10 LPO cycles)

// Default timeout for fault protection
// Use 256ms to give threads time to run while still catching stuck conditions
#define COP_DEFAULT_TIMEOUT  COP_TIMEOUT_256MS

/*----------------------------------------------------------------------------
 * Function Prototypes
 *----------------------------------------------------------------------------*/

/**
 * @brief Initialize the COP Watchdog Timer
 * 
 * Configures the COP with the specified timeout value.
 * IMPORTANT: Once COP is enabled, it cannot be disabled (write-once).
 * 
 * @param timeout COP timeout value (COP_TIMEOUT_32MS, COP_TIMEOUT_256MS, 
 *                or COP_TIMEOUT_1024MS)
 */
void WDT_Init(uint8_t timeout);

/**
 * @brief Feed (service) the COP Watchdog Timer
 * 
 * Must be called periodically to prevent system reset.
 * Writes the required 0x55, 0xAA sequence to SIM->SRVCOP.
 * 
 * Call this from a periodic thread (e.g., Thread_Update_Setpoint).
 * If interrupts are disabled (TR_Disable_All_IRQs fault), this
 * function won't be called, and the watchdog will reset the MCU.
 */
void WDT_Feed(void);

/**
 * @brief Check if last reset was caused by COP timeout
 * 
 * @return 1 if COP caused last reset, 0 otherwise
 */
int WDT_Was_Reset_By_COP(void);

#endif // WDT_H
