#ifndef PROFILE_H
#define PROFILE_H

#include "system_MKL25Z4.h"

//// Configuration 

// Comment out PROFILER_LCD_SUPPORT definition if not using LCD
#define PROFILER_LCD_SUPPORT 

// Comment out PROFILER_SERIAL_SUPPORT if not using printf/serial port for profile output
// #define PROFILER_SERIAL_SUPPORT

#define PROFILER_PIT_CH (0)
#define PROFILER_SAMPLE_FREQ_HZ (999)  // pick to avoid synchronization

//// End of configuration

/* Exception Behavior and Stacks:
RTOS: 
	HW pushes context onto PSP (PC offset is +24 bytes)
	SW (handler) builds stack frame on MSP 
	SW: Call to Process_Profile_Sample builds stack frame on MSP 
	=> Return address is is at PSP + 24
	
No RTOS:
	HW pushes context onto MSP (PC offset is +24 bytes)
	SW: IRQ handler builds stack frame on MSP (+8 bytes)
	SW: Call to Process_Profile_Sample builds stack frame on MSP (+12 bytes)
	=> Return address is is at MSP + 44
	
*/ 

#ifdef USING_RTOS 
	#include <cmsis_os2.h>
#endif

#ifdef USING_AC5 // register variables declared in profile.c
	#define PROFILER_PSP (_psp)
	#define PROFILER_MSP (_msp)
#else // USING_AC6 // intrinsics
	#define PROFILER_PSP (__arm_rsr("PSP"))
	#define PROFILER_MSP (__arm_rsr("MSP"))
#endif

#define HW_RET_ADX_OFFSET (24) // always 24, ISA-defined
// #define IRQ_FRAME_SIZE (8)    // may change based on handler code and compiler settings
#define IRQ_FRAME_SIZE (16)    // Microlib
#define PPS_FRAME_SIZE (20)  // may change based on handler code and compiler settings

#ifdef USING_RTOS // PC is on PSP, not MSP
	#define FRAME_SIZE 	(0)
	#define RA_SP 			(PROFILER_PSP)
#else // Using MSP, so stack frames are also on the MSP stack 
	#define FRAME_SIZE 	(IRQ_FRAME_SIZE + PPS_FRAME_SIZE)
	#define RA_SP 			(PROFILER_MSP)
#endif	

#define SAMPLE_FREQ_HZ_TO_TICKS(freq) ((SystemCoreClock/(2*freq))-1)

extern volatile unsigned int profiling_enabled;

extern void Init_Profiling(void);

extern void Disable_Profiling(void);
extern void Enable_Profiling(void);
extern int Profiling_Is_Enabled(void);

#ifdef USING_RTOS
extern void Profiler_Select_Thread(osThreadId_t * th);
// Call to profile only specified thread. Otherwise, will profile all threads.
#endif

// Need to call this function from timer IRQHandler (e.g. PIT_IRQHandler)
extern void Process_Profile_Sample(void);

extern void Sort_Profile_Regions(void);

#ifdef PROFILER_LCD_SUPPORT 
extern void Display_Profile(void);
#endif

#ifdef PROFILER_SERIAL_SUPPORT
void Serial_Print_Sorted_Profile(void);
#endif

#endif
