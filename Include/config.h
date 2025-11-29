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
#define SCOPE_SYNC_WITH_RTOS  (1)

// Fault Protection Configuration
// Set to 1 to enable PID gain validation (protects against TR_PID_FX_Gains fault)
// Set to 0 to disable validation (to observe fault behavior without protection)
#define ENABLE_PID_GAIN_VALIDATION  (1)

// Set to 1 to enable COP Watchdog Timer (protects against TR_Disable_All_IRQs fault)
// Set to 0 to disable watchdog (to observe fault behavior without protection)
// WARNING: Once enabled, COP cannot be disabled without a reset
#define ENABLE_COP_WATCHDOG  (1)

// LCD and Graphics Optimizations
#define LCD_BUS_DEFAULTS_TO_DATA 1 
#define DRAW_LINE_RUNS_AS_RECTANGLES 1 
#define USE_TEXT_BITMAP_RUNS 1 

// I2C Configuration
#define READ_FULL_XYZ 1 
#define I2C_ICR_VALUE 0x20 

#endif // CONFIG_H
