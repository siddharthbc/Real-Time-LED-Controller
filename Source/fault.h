#ifndef FAULT_H
#define FAULT_H

#include <stdint.h>
#include <stdio.h>

#include <cmsis_os2.h>
#include "debug.h"
#include <MKL25Z4.h>

#include "LCD.h"
#include "LCD_driver.h"
#include "ST7789.h"
#include "T6963.h"
#include "font.h"

#include "GPIO_defs.h"
#include "timers.h"
#include "config.h"
#include "ADC.h"

#define EXTENDED_FAULTS (1)

#define FAULT_PERIOD (2000)

#define FAULT_MSG_LCD_ROW (14)

// Initialization function
void Fault_Init(void);

#endif // FAULT_H
