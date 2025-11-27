#ifndef THREADS_H
#define THREADS_H

#include <cmsis_os2.h>
#include "LCD_driver.h"
#include <MKL25Z4.h>

#define THREAD_READ_TS_PERIOD_TICKS (100)  
#define THREAD_DRAW_WAVEFORM_PERIOD_TICKS (200) 
#define THREAD_DRAW_UI_CONTROLS_PERIOD_TICKS (210) 
#define THREAD_UPDATE_SETPOINT_PERIOD_TICKS (1)
#define THREAD_READ_ACCELEROMETER_PERIOD_TICKS (50) 

#define PERIODIC_READ_ACCEL (1)

// Custom stack sizes for larger threads
#define READ_ACCEL_STK_SZ 768

void Init_Debug_Signals(void);

void Create_OS_Objects(void);
 
extern osThreadId_t t_Read_TS, t_Read_Accelerometer, t_US;
extern osMutexId_t LCD_mutex;

#endif // THREADS_H

