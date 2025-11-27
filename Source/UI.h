#ifndef UI_H
#define UI_H
#include "LCD.h"

// Definitions
#define UI_LABEL_LEN (16)
#define UI_UNITS_LEN (4)

#define SPLIT_LCD_UPDATE 1

// Structures
typedef struct sUI_FIELD_T UI_FIELD_T;
typedef struct sUI_FIELD_T {
	char Label[UI_LABEL_LEN]; 
	char Units[UI_UNITS_LEN];
	char Buffer[2*UI_LABEL_LEN]; // Holds latest field text: Label Val Units
	volatile int * Val;
	volatile char * ValT; // Unused. Future expansion for text values.
	PT_T RC; // Starting row (Y) and column (X) of field
	COLOR_T * ColorFG, * ColorBG;
	char Updated, Selected, ReadOnly, Volatile;
	void (*Handler)(UI_FIELD_T * fld, int v); // Handler function to change value based on slider pos v
} UI_FIELD_T ;

typedef struct  {
	int Val; // Is 0 when touched at horizontal middle 
	PT_T UL, LR;
	PT_T BarUL, BarLR;
	COLOR_T * ColorFG, * ColorBG, * ColorBorder;
} UI_SLIDER_T;

#define UI_NUM_FIELDS (sizeof(Fields)/sizeof(UI_FIELD_T))
#define UI_SLIDER (100)

#define UI_SLIDER_HEIGHT 		(30)
#define UI_SLIDER_WIDTH 		(LCD_WIDTH)
#define UI_SLIDER_BAR_WIDTH (8)

#define NUM_CURR_PIXELS    	LCD_WIDTH
#define SAMPLES_PER_PIXEL  	(SAM_BUF_SIZE/NUM_CURR_PIXELS)
#define PRE_TRIG_SAMPLES    256   /* start the display this many samples before start of trigger */

#define PARTIAL_SCOPE_ERASE 0

extern volatile int g_scope_height;
#define INIT_SCOPE_HEIGHT  (128)
// #define CVT_RATIO ((0.75*V_REF_MV*MA_SCALING_FACTOR)/(ADC_FULL_SCALE*R_SENSE))
#define CVT_RATIO ((0.125*V_REF_MV*MA_SCALING_FACTOR)/(ADC_FULL_SCALE*R_SENSE))
#define DISP_SCALE (0xFFFF/g_scope_height)
#define SCALE_SCOPE(val) (g_scope_height - ((g_scope_height*(val))/0x4000))

#define SCALE_ADC_CODE_TO_SCOPE(sample) ((sample)*CVT_RATIO)
#define CLIP_SCOPE(val)   (val<0? 0 : (val > INIT_SCOPE_HEIGHT-1? INIT_SCOPE_HEIGHT-1: val))

// Function prototypes
#if SPLIT_LCD_UPDATE
void UI_Draw_Waveforms(void);
void UI_Update_Controls(int first_time);
#else
void UI_Draw_Screen(int first_time);
#endif


int UI_Identify_Control(PT_T * p);
void UI_Process_Touch(PT_T * p);

void UI_Update_Field_Values (UI_FIELD_T * f, int num);
void UI_Draw_Fields(UI_FIELD_T * f, int num);

extern volatile int g_holdoff;

#endif // UI_H
