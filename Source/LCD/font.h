#ifndef FONT_H
#define FONT_H
#include <stdint.h>

#define USE_TEXT_BITMAP_RUNS 1

// Font controls
#define FORCE_MONOSPACE (1)
#define CHAR_TRACKING (1) // additional horizontal padding after a character
#define FONTS_IN_APPLICATION (0) // Put fonts in application (1) or overlay memory (0)

// Font type definitions
typedef struct {
	uint8_t FontID;
	uint8_t Orientation;
	uint16_t FirstChar;
	uint16_t LastChar;
	uint8_t Height;
	uint8_t Reserved;
} FONT_HEADER_T;

typedef struct {
	uint32_t Width:8; // pixels
	uint32_t Offset:24; // Offset from start of table
} GLYPH_INDEX_T;

// External font data
extern const uint8_t Lucida_Console8x13[];
extern const uint8_t Lucida_Console12x19[];
extern const uint8_t Lucida_Console20x31[];

#if FONTS_IN_APPLICATION
#else
	#define P_LUCIDA_CONSOLE8x13   ((const uint8_t *) 0x00008000)
	#define P_LUCIDA_CONSOLE12x19  ((const uint8_t *) 0x00008800)
	#define P_LUCIDA_CONSOLE20x31  ((const uint8_t *) 0x00009800)
#endif

// Definitions and Conversions
#define CHAR_WIDTH G_LCD_char_width 
#define CHAR_HEIGHT G_LCD_char_height 

#define ROW_TO_Y(r) ((r)*G_LCD_char_height)
#define COL_TO_X(c) ((c)*(G_LCD_char_width+CHAR_TRACKING))

#define LCD_MAX_COLS (LCD_WIDTH/(G_LCD_char_width+CHAR_TRACKING))
#define LCD_MAX_ROWS (LCD_HEIGHT/G_LCD_char_height)

#define NEWLINE(p) {p->X = 0; p->Y += CHAR_HEIGHT;}

#endif // #ifndef FONT_H
