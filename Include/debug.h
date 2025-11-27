#ifndef DEBUG_H
#define DEBUG_H
#include <stdint.h>
#include "MKL25Z4.h"

#define MASK(x) (1UL << (x))

// Set to 1 to also use the four SPI as debug signals
#define DBG_USE_SPI_SIGNALS 1
// Set to 1 to enable testing debug signal in initialization
#define DEBUG_INIT_TEST 0 

#define DBG_NUM_SIGNALS (8 + 4*DBG_USE_SPI_SIGNALS + 1) // Last is NULL

// Define low-level names for debug signals. 
// These are used as indices into the debug_GPIO array, which holds
// each signal's port and bit assignment.
#define DBG_0 0
#define DBG_1 1 	
#define DBG_2 2	  
#define DBG_3 3		
#define DBG_4 4
#define DBG_5 5
#define DBG_6 6
#define DBG_7 7
#if DBG_USE_SPI_SIGNALS // use four more?
  #define DBG_8 8
  #define DBG_9 9
  #define DBG_10 10
  #define DBG_11 11
#endif

// Define meaningful names for user debug signals here
#define DBG_TPM_ISR_POS					DBG_0
#define DBG_ADC_ISR_POS  				DBG_1
#define DBG_LOPRI_ADC_POS  			DBG_2
#define DBG_TUSP_POS  					DBG_3
#define DBG_CONTROLLER_POS			DBG_4
#define DBG_PENDING_WVFM_POS 		DBG_5
#define DBG_T_DRAW_WVFMS_POS		DBG_6
#define DBG_T_DRAW_UI_CTLS_POS	DBG_7
#define DBG_BLOCKING_LCD_POS		DBG_8
#define DBG_LCD_COMM_POS				DBG_9
#define DBG_FAULT_POS  					DBG_10
#define DBG_IDLE_LOOP						DBG_11

#define DBG_TREADACC_POS				DBG_NULL
#define DBG_PORTA_IRQ						DBG_NULL
#define DBG_TREADTS_POS					DBG_NULL
#define DBG_DMA_ISR_POS					DBG_NULL
#define DBG_TSNDMGR_POS					DBG_NULL
#define DBG_TREFILLSB_POS				DBG_NULL


#define DBG_NULL (DBG_NUM_SIGNALS-1) // mapped in debug.c to a non-GPIO bit on a used port, so accesses have no effect

// Debug output control macros
#define DEBUG_START(x)	{debug_GPIO[x].FGPIO->PSOR = MASK(debug_GPIO[x].Bit);}
#define DEBUG_STOP(x)		{debug_GPIO[x].FGPIO->PCOR = MASK(debug_GPIO[x].Bit);}
#define DEBUG_TOGGLE(x)	{debug_GPIO[x].FGPIO->PTOR = MASK(debug_GPIO[x].Bit);}

// Interface functions and data structures
// Type definition for describing debug output signal
typedef struct {
	uint32_t ID;
	uint32_t Bit; // -> Mask later?
	FGPIO_MemMapPtr FGPIO;
	PORT_MemMapPtr Port;
} debug_GPIO_struct;


void Init_Debug_Signals(void);
extern debug_GPIO_struct debug_GPIO[DBG_NUM_SIGNALS];

#endif // DEBUG_H
