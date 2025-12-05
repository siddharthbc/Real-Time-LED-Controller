/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#include <MKL25Z4.h>
#include <stdio.h>
#include "math.h"
#include <cmsis_os2.h>

#include "GPIO_defs.h"
#include "threads.h"

#include "LCD.h"
#include "LCD_driver.h"
#include "font.h"

#include "LEDs.h"
#include "timers.h"
#include "sound.h"
#include "delay.h"
#include "profile.h"
#include "control.h"
#include "fault.h"

#include "I2C.h"
#include "MMA8451.h"

#include "config.h"
#if ENABLE_COP_WATCHDOG
#include "wdt.h"
#endif

volatile CTL_MODE_E control_mode=DEF_CONTROL_MODE;
// extern void	test_acc_int(void);

#define FAIL_FLASH_LEN (70)

// Flash red LED with error code)
void Fail_Flash(int n) {
	int i;
	
	while (1) {
			i = n;
			do {
				Control_RGB_LEDs(1, 0, 0);
				Delay(FAIL_FLASH_LEN);
				Control_RGB_LEDs(0, 0, 0);
				Delay(FAIL_FLASH_LEN*2);
			} while (--i > 0);
			Delay(FAIL_FLASH_LEN*10);
	}
}

/*----------------------------------------------------------------------------
  MAIN function
 *----------------------------------------------------------------------------*/
int main (void) {
#if ENABLE_COP_WATCHDOG
	// Check if we recovered from a COP watchdog reset
	int cop_reset = WDT_Was_Reset_By_COP();
#endif
	
	Init_Debug_Signals();
	Init_RGB_LEDs();
	Control_RGB_LEDs(0,0,1);
	
#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed watchdog before LCD init
#endif
	
	LCD_Init();
	
#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed watchdog after LCD init
#endif
	
	if (!LCD_Text_Init(1)) {
		/* Font bitmaps not found in memory.
		1. Ensure downloading this project doesn't erase all of flash memory. 
			Go to Target Options->Debug->(debugger) Settings->Flash Download ... Select "Erase Sectors"
			Save project and close.
		2. Open Overlay project, build it and program it into MCU. Close Overlay project.
	  3. Reopen this project, build and download.
		*/
		Fail_Flash(2);
	}
	
#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed watchdog after LCD text init
#endif
	
	LCD_Erase();
	LCD_Text_PrintStr_RC(0,0, "ECE 4/560 Project");
	
#if ENABLE_COP_WATCHDOG
	// Display if we recovered from a COP watchdog reset
	if (cop_reset) {
		LCD_Text_PrintStr_RC(1,0, "COP Reset Recovery!");
		Control_RGB_LEDs(1, 1, 0);  // Yellow LED to indicate recovery
		// Feed watchdog during the delay (break into smaller chunks)
		for (int i = 0; i < 15; i++) {
			Delay(100);
			WDT_Feed();
		}
		Control_RGB_LEDs(0, 0, 1);  // Back to blue
	}
#endif
	
	LCD_Text_PrintStr_RC(1,0, "Testing:");
	LCD_Text_PrintStr_RC(2,0, "Accel...");

#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed before I2C init
#endif
	
	i2c_init();											// init I2C peripheral

#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed before accelerometer init
#endif

//	test_acc_int();
	if (!init_mma()) {							// init accelerometer
		Fail_Flash(3);			// accel initialization failed
	}
	
#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed after accelerometer init
#endif

	LCD_Text_PrintStr_RC(2,9, "Done");
	
#if ENABLE_COP_WATCHDOG
	// Break Delay(250) into smaller chunks and feed watchdog
	for (int i = 0; i < 5; i++) {
		Delay(50);
		WDT_Feed();
	}
#else
	Delay(250);
#endif

	LCD_Erase();

#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed before Buck init
#endif

	Init_Buck_HBLED();
	
#if ENABLE_COP_WATCHDOG
	WDT_Feed();  // Feed before RTOS init
#endif
	
	osKernelInitialize();
	Fault_Init();
	Create_OS_Objects();
	osKernelStart();	
}
