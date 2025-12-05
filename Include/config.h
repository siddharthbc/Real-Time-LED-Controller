#ifndef CONFIG_H
#define CONFIG_H

#include "GPIO_defs.h"

// Platform
// Select one compiler
// #define USING_AC5
#define USING_AC6

#define USING_RTOS

// Application Program
#define USE_ADC_SERVER 				(1)
#define USE_LCD_MUTEX_LEVEL  	(1) // Change to 2, 3 as you try out different mutex levels within the threads

// Scope Synchronization Configuration
// Set to 1 to use RTOS mechanisms (event flags) for ISR-Thread synchronization
// Set to 0 to use state machine approach (polling without RTOS mechanisms)
#define SCOPE_SYNC_WITH_RTOS  (0)

// Fault Protection Configuration
// Set to 1 to enable PID gain validation (protects against TR_PID_FX_Gains fault)
// Set to 0 to disable validation (to observe fault behavior without protection)
#define ENABLE_PID_GAIN_VALIDATION  (0)

// Set to 1 to enable COP Watchdog Timer (protects against TR_Disable_All_IRQs fault)
// Set to 0 to disable watchdog (to observe fault behavior without protection)
// WARNING: Once enabled, COP cannot be disabled without a reset
#define ENABLE_COP_WATCHDOG  (1)

// Set to 1 to enable ADC IRQ scrubbing (protects against TR_Disable_ADC_IRQ fault)
#define ENABLE_ADC_IRQ_SCRUB  (0)

// Set to 1 to enable setpoint validation (protects against TR_Setpoint_High fault)
#define ENABLE_SETPOINT_VALIDATION  (0)

// Set to 1 to enable flash period validation (protects against TR_Flash_Period fault)
#define ENABLE_FLASH_PERIOD_VALIDATION  (0)

// Set to 1 to enable TPM scrubbing (protects against TR_Slow_TPM fault)
// Periodically restores TPM0->MOD to correct value
#define ENABLE_TPM_SCRUB  (0)

// Set to 1 to enable peripheral clock scrubbing (protects against TR_Disable_PeriphClocks fault)
// Periodically re-enables critical peripheral clocks in SCGC6
#define ENABLE_CLOCK_SCRUB  (0)

// Set to 1 to enable MCU clock validation (protects against TR_Change_MCU_Clock fault)
// Restores MCG settings if corrupted
#define ENABLE_MCG_SCRUB  (1)

// LCD and Graphics Optimizations
#define LCD_BUS_DEFAULTS_TO_DATA 1 
#define DRAW_LINE_RUNS_AS_RECTANGLES 1 
#define USE_TEXT_BITMAP_RUNS 1 

// I2C Configuration
#define READ_FULL_XYZ 1 
#define I2C_ICR_VALUE 0x20 

#endif // CONFIG_H
