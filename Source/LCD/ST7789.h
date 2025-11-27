#ifndef ST7789_H
#define ST7789_H

#include "LCD_driver.h"

#if (((LCD_CONTROLLER)==(CTLR_ILI9341)) || ((LCD_CONTROLLER)==(CTLR_ST7789)))

extern const LCD_CTLR_INIT_SEQ_T Init_Seq_ST7789[];
extern const LCD_CTLR_INIT_SEQ_T Init_Seq_ILI9341[];

#define LCD_WIDTH (240)
#define LCD_HEIGHT (320)

#define LCD_CENTER_X (LCD_WIDTH/2)
#define LCD_CENTER_Y (LCD_HEIGHT/2)

#define BITS_PER_PIXEL				(24)

// TFT LCD Hardware Interface
// signals -- all on port C
#define LCD_DB8_POS (3)
#define LCD_DB15_POS (10)
#define LCD_D_NC_POS (12)
#define LCD_NWR_POS (13)
#define LCD_NRD_POS (16)
#define LCD_NRST_POS (17)
#define LCD_DATA_MASK (((unsigned )0x0ff) << LCD_DB8_POS)

// ST7789 Commands
#define ST7789_CMD_NORON (0x13)
#define ST7789_CMD_PTLON (0x12)
#define ST7789_CMD_DISPOFF (0x28)
#define ST7789_CMD_DISPON (0x29)
#define ST7789_CMD_SLPIN (0x10)
#define ST7789_CMD_SLPOUT (0x11)
#define ST7789_CMD_IDMOFF (0x38)
#define ST7789_CMD_IDMON (0x39)
#define ST7789_CMD_WRDISBV (0x51)

#endif // LCD Controller

#endif // ST7789_H

