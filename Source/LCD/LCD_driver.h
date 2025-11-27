#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <stdint.h>

#define CTLR_ILI9341  (1)
#define CTLR_ST7789   (2)
#define CTLR_T6963  	(3)

// #define LCD_CONTROLLER (CTLR_ILI9341) 
#define LCD_CONTROLLER (CTLR_ST7789) 
// #define LCD_CONTROLLER (CTLR_T6963) 

#define USE_LARGE_FONT (1) // 1 for original 12x19 font, 0 for smaller 8x13 font
#define USE_TOUCHSCREEN (1) // 1 to enable code for reading touchscreen, 0 to disable 

#define SHIELD_VERSION (14)

// Backlight
#define LCD_BL_TPM (TPM1)
#define LCD_BL_TPM_CHANNEL (0)
#define LCD_BL_TPM_FREQ (40000)
#define LCD_BL_PERIOD ((SystemCoreClock)/(2*(LCD_BL_TPM_FREQ)))

// Touchscreen Hardware Interface
#if (SHIELD_VERSION == 14)
#define LCD_TS_YD_ADC_CHAN (6)
#define LCD_TS_YD_ADC_MUXSEL (1)

#define LCD_TS_XR_ADC_CHAN (7)
#define LCD_TS_XR_ADC_MUXSEL (0)

#define LCD_TS_YD_PORT (PORTD)
#define LCD_TS_XL_PORT (PORTA)
#define LCD_TS_YU_PORT (PORTA)
#define LCD_TS_XR_PORT (PORTE)

#define LCD_TS_YD_PT (PTD)
#define LCD_TS_XL_PT (PTA)
#define LCD_TS_YU_PT (PTA)
#define LCD_TS_XR_PT (PTE)

#define LCD_TS_YD_BIT (5)
#define LCD_TS_XL_BIT (17)
#define LCD_TS_YU_BIT (16)
#define LCD_TS_XR_BIT (23)

#define LCD_TS_CONFIG_DIG_TOUCH 0
#define LCD_TS_CONFIG_ANA_X 1
#define LCD_TS_CONFIG_ANA_Y 2

#else

#error "Must specify SHIELD_VERSION 14 in LCD_driver.h"
#endif


// Touchscreen Configuration
#define TS_DELAY (1)
#define TS_CALIB_SAMPLES (60)

/**************************************************************/
#if 0
#define	GPIO_ResetBit(pos)	{FPTC->PCOR = MASK(pos); __nop(); __nop(); __nop(); __nop();}
#define	GPIO_SetBit(pos) 		{FPTC->PSOR = MASK(pos); __nop(); __nop(); __nop(); __nop();}
#define GPIO_Write(cmd) 		{FPTC->PDOR &= ~LCD_DATA_MASK; \
														__nop(); __nop(); __nop(); __nop(); \
														FPTC->PDOR |= (cmd & 0xff) << LCD_DB8_POS; \
														__nop(); __nop(); __nop(); __nop();}
#else 
#define	GPIO_ResetBit(pos)	{FPTC->PCOR = MASK(pos); }
#define	GPIO_SetBit(pos) 		{FPTC->PSOR = MASK(pos); }
#define GPIO_Write(cmd) 		{FPTC->PDOR &= ~LCD_DATA_MASK; \
														FPTC->PDOR |= (cmd & 0xff) << LCD_DB8_POS; }
#endif
/**************************************************************/

#define LCD_CTRL_INIT_SEQ_END 0
#define LCD_CTRL_INIT_SEQ_CMD 1
#define LCD_CTRL_INIT_SEQ_DAT 2

typedef struct {
	uint8_t Type;  // 0: end, 1: command, 2: data
	uint8_t Value;
} LCD_CTLR_INIT_SEQ_T; // sequence of commands and data for initializing LCD controller

#endif
