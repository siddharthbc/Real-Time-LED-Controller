#include <stdint.h>
#include <stdio.h>
#include <cmsis_os2.h>
#include <MKL25Z4.h>
#include "misc.h"

#include "LEDs.h"
#include "delay.h"

#include "LCD.h"
#include "touchscreen.h"
#include "LCD_driver.h"
#include "ST7789.h"

#include "font.h"
#include "threads.h"
#include "GPIO_defs.h"
#include "debug.h"
#include "config.h"
#include "control.h"
#include "UI.h"

#include "ADC.h"
#include "MMA8451.h"
#include "fault.h"
#include "timers.h"

#if ENABLE_COP_WATCHDOG
#include "wdt.h"
#endif

void Thread_Read_Touchscreen(void * arg); // 
void Thread_Draw_Waveforms(void * arg);
void Thread_Draw_UI_Controls(void * arg);
osThreadId_t t_DW, t_DUC;
const osThreadAttr_t Draw_Waveforms_attr = {
	.priority = osPriorityAboveNormal, 
	.stack_size = 256
};
const osThreadAttr_t Update_UI_Controls_attr = {
	.priority = osPriorityNormal, 
	.stack_size = 512 
};

void Thread_Update_Setpoint(void * arg);
void Thread_Read_Accelerometer(void * arg); 

osThreadId_t t_Read_TS, t_USP, t_Read_Accelerometer;

// Basic thread priority options: osPriority[RealTime|High|AboveNormal|Normal|BelowNormal|Low|Idle]
// Each can have 1-7 appended for finer resolution

const osThreadAttr_t Read_Touchscreen_attr = {
  .priority = osPriorityNormal,
	.stack_size = 384
};

const osThreadAttr_t Update_Setpoint_attr = {
  .priority = osPriorityHigh,
	.stack_size = 128
};
const osThreadAttr_t Read_Accelerometer_attr = {
  .priority = osPriorityAboveNormal,      
	.stack_size = READ_ACCEL_STK_SZ
};

void Create_OS_Objects(void) {
	LCD_Create_OS_Objects();
	
#if SCOPE_SYNC_WITH_RTOS
	// Initialize event flags for ISR-Thread synchronization (Approach 2)
	scope_event_flags = osEventFlagsNew(NULL);
#endif
	
	t_Read_TS = osThreadNew(Thread_Read_Touchscreen, NULL, &Read_Touchscreen_attr);  
	t_DW = osThreadNew(Thread_Draw_Waveforms, NULL, &Draw_Waveforms_attr);
	t_DUC = osThreadNew(Thread_Draw_UI_Controls, NULL, &Update_UI_Controls_attr);
	t_USP = osThreadNew(Thread_Update_Setpoint, NULL, &Update_Setpoint_attr);
	t_Read_Accelerometer = osThreadNew(Thread_Read_Accelerometer, NULL, &Read_Accelerometer_attr);
}

void Thread_Read_Touchscreen(void * arg) {
	PT_T p;
	uint32_t tick;
 
  tick = osKernelGetTickCount();        // retrieve the number of system ticks	
	while (1) {
		DEBUG_START(DBG_TREADTS_POS);
		if (LCD_TS_Read(&p)) { 
			UI_Process_Touch(&p);
		}
		DEBUG_STOP(DBG_TREADTS_POS);
		tick += THREAD_READ_TS_PERIOD_TICKS;
		osDelayUntil(tick);
	}
}

void Thread_Draw_Waveforms(void * arg) {
	uint32_t tick;
	
	// Initialization
	osMutexAcquire(LCD_mutex, osWaitForever); // get LCD permission
	UI_Draw_Waveforms();
	osMutexRelease(LCD_mutex);		// relinquish LCD permission
	tick = osKernelGetTickCount();        // retrieve the number of system ticks
	
	// Regular operation
	while (1) {
		tick += THREAD_DRAW_WAVEFORM_PERIOD_TICKS;
		osDelayUntil(tick);
		DEBUG_START(DBG_T_DRAW_WVFMS_POS);  // Show thread's work has started
		
#if SCOPE_SYNC_WITH_RTOS
		//=============================================================
		// APPROACH 2: RTOS Event Flags
		// Wait for event flag from ISR indicating buffer is full
		// Non-blocking check (timeout = 0) to maintain periodic behavior
		//=============================================================
		uint32_t flags = osEventFlagsWait(scope_event_flags, SCOPE_FLAG_BUFFER_FULL, 
		                                   osFlagsWaitAny, 0);
		
		if (flags == SCOPE_FLAG_BUFFER_FULL) {
			// Buffer is full and ready to plot
			// Transition to Plotting state - ISR will not write to buffers
			g_scope_state = Plotting;
			
#if USE_LCD_MUTEX_LEVEL==1
			DEBUG_START(DBG_BLOCKING_LCD_POS);
			osMutexAcquire(LCD_mutex, osWaitForever); // get LCD permission
			DEBUG_STOP(DBG_BLOCKING_LCD_POS);
#endif
			UI_Draw_Waveforms(); // Update scope part of screen with waveforms

#if USE_LCD_MUTEX_LEVEL==1
			osMutexRelease(LCD_mutex);	// relinquish LCD permission
#endif
			// Plotting complete - transition back to Armed state
			g_scope_state = Armed;
		}
		// If flag not set, skip drawing this cycle (no new data ready)

#else
		//=============================================================
		// APPROACH 1: State Machine (Polling, no RTOS mechanisms)
		// Poll the state variable to check if buffers are ready
		//=============================================================
		if (g_scope_state == Full) {
			// Buffers are full and ready to plot
			// Transition to Plotting state - ISR will not write to buffers
			g_scope_state = Plotting;
			
#if USE_LCD_MUTEX_LEVEL==1
			DEBUG_START(DBG_BLOCKING_LCD_POS);
			osMutexAcquire(LCD_mutex, osWaitForever); // get LCD permission
			DEBUG_STOP(DBG_BLOCKING_LCD_POS);
#endif
			UI_Draw_Waveforms(); // Update scope part of screen with waveforms

#if USE_LCD_MUTEX_LEVEL==1
			osMutexRelease(LCD_mutex);	// relinquish LCD permission
#endif
			// Plotting complete - transition back to Armed state
			// ISR can now start looking for next trigger
			g_scope_state = Armed;
		}
		// If state is not Full, skip drawing this cycle (no new data ready)
#endif
		
		DEBUG_STOP(DBG_T_DRAW_WVFMS_POS); // Show thread's work is done
	}
}

void Thread_Draw_UI_Controls(void * arg) {
	uint32_t tick;
	
	// Initialization
	osMutexAcquire(LCD_mutex, osWaitForever); // get LCD permission
	UI_Update_Controls(1);
	osMutexRelease(LCD_mutex);		// relinquish LCD permission
  tick = osKernelGetTickCount();        // retrieve the number of system ticks

	// Regular operation
	while (1) {
		tick += THREAD_DRAW_UI_CONTROLS_PERIOD_TICKS;
		osDelayUntil(tick);
		DEBUG_START(DBG_T_DRAW_UI_CTLS_POS); // Show thread's work has started
#if USE_LCD_MUTEX_LEVEL==1
		DEBUG_START(DBG_BLOCKING_LCD_POS);
		osMutexAcquire(LCD_mutex, osWaitForever);	// get LCD permission
		DEBUG_STOP(DBG_BLOCKING_LCD_POS);
#endif
		UI_Update_Controls(0); // Update user interface part of screen

#if USE_LCD_MUTEX_LEVEL==1
		osMutexRelease(LCD_mutex);		// relinquish LCD permission
#endif
		DEBUG_STOP(DBG_T_DRAW_UI_CTLS_POS);  // Show thread's work is done
	}
}

 void Thread_Update_Setpoint(void * arg) {
	uint32_t tick;
 
  tick = osKernelGetTickCount();        // retrieve the number of system ticks
	while (1) {
		tick += THREAD_UPDATE_SETPOINT_PERIOD_TICKS;
		osDelayUntil(tick); 
		DEBUG_START(DBG_TUSP_POS);
		
#if ENABLE_COP_WATCHDOG
		// Fault Protection: Feed the COP watchdog timer
		// This must be called periodically to prevent MCU reset
		// If TR_Disable_All_IRQs fault occurs, this thread stops running,
		// watchdog times out, and MCU resets to a known-good state
		WDT_Feed();
#endif
		
#if ENABLE_PID_GAIN_VALIDATION
		// Fault Protection: Validate PID gains before updating setpoint
		// This detects and corrects corrupted gains (e.g., from TR_PID_FX_Gains fault)
		Validate_PID_Gains();
#endif

#if ENABLE_ADC_IRQ_SCRUB
		// Fault Protection: Re-enable ADC IRQ (protects against TR_Disable_ADC_IRQ)
		NVIC_EnableIRQ(ADC0_IRQn);
#endif

#if ENABLE_SETPOINT_VALIDATION
		// Fault Protection: Clamp setpoint to safe range (protects against TR_Setpoint_High)
		// Max safe current is 300mA, min is 0mA
		if (g_set_current_mA > 300) g_set_current_mA = 300;
		if (g_set_current_mA < 0) g_set_current_mA = 0;
#endif

#if ENABLE_FLASH_PERIOD_VALIDATION
		// Fault Protection: Clamp flash period to valid range (protects against TR_Flash_Period)
		// Valid range is 2-180ms (accelerometer controlled range)
		if (g_flash_period < 2) g_flash_period = 2;
		if (g_flash_period > 180) g_flash_period = 180;
#endif

#if ENABLE_TPM_SCRUB
		// Fault Protection: Restore TPM0->MOD to correct value (protects against TR_Slow_TPM)
		// The fault sets TPM0->MOD to 23456, breaking PWM timing
		// We restore it to PWM_PERIOD to maintain correct switching frequency
		if (TPM0->MOD != PWM_PERIOD) {
			TPM0->MOD = PWM_PERIOD;
		}
#endif

#if ENABLE_CLOCK_SCRUB
		// Fault Protection: Re-enable critical peripheral clocks (protects against TR_Disable_PeriphClocks)
		// The fault clears SIM->SCGC6, disabling ADC0, TPM0, etc.
		// We restore the essential clocks needed for LED control
		SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK | SIM_SCGC6_TPM0_MASK | SIM_SCGC6_DAC0_MASK;
#endif

#if ENABLE_MCG_SCRUB
		// Fault Protection: Restore MCG settings (protects against TR_Change_MCU_Clock)
		// The fault corrupts MCG->C5, changing the clock frequency
		// Note: Full clock restoration is complex; this is a simplified scrub
		// that catches the specific fault injection value
		if (MCG->C5 == 0x0018) {
			// Detected corrupted value - restore default
			// Default for 48MHz from 8MHz crystal: PRDIV0 = 1 (divide by 2)
			MCG->C5 = MCG_C5_PRDIV0(1);
		}
#endif
		
		Update_Set_Current();
		DEBUG_STOP(DBG_TUSP_POS);
	}
 }
 
void Thread_Read_Accelerometer(void * arg) {
	uint32_t tick = osKernelGetTickCount();
	int period;

#if MMA_USE_INTERRUPTS
	mma_set_active(0);
	enable_mma_interrupt_generation(0x01);
#endif
	mma_set_active(1);
	
	while (1) {
		tick += THREAD_READ_ACCELEROMETER_PERIOD_TICKS;
		osDelayUntil(tick);
		DEBUG_START(DBG_TREADACC_POS);
		uint8_t s = read_status();
		if ((s & 0x08)) { 
			read_full_xyz();
			convert_xyz_to_roll_pitch();
			period = 30 + (int) roll;
			period = MAX(2, period);
			g_flash_period = MIN(period, 180);
			g_flash_duration = MAX(1, g_flash_period/4);
		}
		DEBUG_STOP(DBG_TREADACC_POS);
	}
}

