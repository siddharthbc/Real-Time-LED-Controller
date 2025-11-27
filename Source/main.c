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
	Init_Debug_Signals();
	Init_RGB_LEDs();
	Control_RGB_LEDs(0,0,1);			
	
	LCD_Init();
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
	
	LCD_Erase();
	LCD_Text_PrintStr_RC(0,0, "ECE 4/560 Project");
	LCD_Text_PrintStr_RC(1,0, "Testing:");
	LCD_Text_PrintStr_RC(2,0, "Accel...");

	
	i2c_init();											// init I2C peripheral
//	test_acc_int();
	if (!init_mma()) {							// init accelerometer
		Fail_Flash(3);			// accel initialization failed
	}
	LCD_Text_PrintStr_RC(2,9, "Done");
	Delay(250);
	LCD_Erase();

	Init_Buck_HBLED();
	osKernelInitialize();
	Fault_Init();
	Create_OS_Objects();
	osKernelStart();	
}
