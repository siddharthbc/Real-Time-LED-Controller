#ifndef CONTROL_H
#define CONTROL_H

#include "FX.h"
#include "config.h"
#include "UI.h"

// Flash parameters
#define FLASH_PERIOD_MS (800)
#define FLASH_CURRENT_MA (75) 
#define FLASH_DURATION_MS (10)

// Switching parameters
#define PWM_HBLED_CHANNEL (4)
#define PWM_PERIOD (750) // 
/* 48 MHz input clock. 
	PWM frequency = 48 MHz/(PWM_PERIOD*2)
	PWM period = PWM_PERIOD/24 MHz
	Timer is in count-up/down mode. 
	800: 30 kHz, 33.33 us
	750: 32 kHz, 31.25 us
	700: 34.286 kHz, 29.16 us
	650: 36.9 kHz, 27.08 us
	600: 40 kHz, 25 us
	500: 48 kHz
	250: 96 kHz
	*/
#define LIM_DUTY_CYCLE (PWM_PERIOD-1)

// Control approach configuration
#define USE_ASYNC_SAMPLING 				0
#define USE_SYNC_NO_FREQ_DIV 			0
#define USE_SYNC_SW_CTL_FREQ_DIV 	1
#define USE_SYNC_HW_CTL_FREQ_DIV 	0

#define SW_CTL_FREQ_DIV_FACTOR (3) // Software division in ISR
#define HW_CTL_FREQ_DIV_CODE (0) // Not used, not supported

#define CTL_PERIOD (PWM_PERIOD*SW_CTL_FREQ_DIV_FACTOR)

#if USE_ASYNC_SAMPLING
#define 	USE_TPM0_INTERRUPT 0
#define 	USE_ADC_HW_TRIGGER 0
#define 	USE_ADC_INTERRUPT 1
#endif

#if USE_SYNC_NO_FREQ_DIV
#define 	USE_TPM0_INTERRUPT 0
#define 	USE_ADC_HW_TRIGGER 1
#define 	USE_ADC_INTERRUPT 1
#endif

#if USE_SYNC_SW_CTL_FREQ_DIV
#define 	USE_TPM0_INTERRUPT 1
#define 	USE_ADC_HW_TRIGGER 0
#define 	USE_ADC_INTERRUPT 1
#endif

#if USE_SYNC_HW_CTL_FREQ_DIV
#define 	USE_TPM0_INTERRUPT 0
#define 	USE_ADC_HW_TRIGGER 1
#define 	USE_ADC_INTERRUPT 1
#endif

// Control Parameters
// default control mode: OpenLoop, BangBang, Incremental, PID, PID_FX
#define DEF_CONTROL_MODE (PID_FX)

// Incremental controller: change amount
#define INC_STEP (0.1*CTL_PERIOD)

// Proportional Gain, scaled by 2^8
#define PGAIN_8 (1.0*CTL_PERIOD) // (0x0028)

// PID (floating-point) gains. Guaranteed to be non-optimal. 
#define P_GAIN_FL (0.006*CTL_PERIOD)
#define I_GAIN_FL (0.000*CTL_PERIOD)
#define D_GAIN_FL (0.000*CTL_PERIOD)

// PID_FX (fixed-point) gains. Guaranteed to be non-optimal. 
#define P_GAIN_FX (87.5*CTL_PERIOD)	
#define I_GAIN_FX (0.625*CTL_PERIOD) 
#define D_GAIN_FX (0.0*CTL_PERIOD)

// Data type definitions
typedef struct {
	float dState; // Last position input
	float iState; // Integrator state
	float iMax, iMin; // Maximum and minimum allowable integrator state
	float pGain, // proportional gain
				iGain, // integral gain
				dGain; // derivative gain
} SPid;

typedef struct {
	FX16_16 dState; // Last position input
	FX16_16 iState; // Integrator state
	FX16_16 iMax, iMin; // Maximum and minimum allowable integrator state
	FX16_16 pGain, // proportional gain
				iGain, // integral gain
				dGain; // derivative gain
} SPidFX;

typedef enum {OpenLoop, BangBang, Incremental, Proportional, PID, PID_FX, MODE_COUNT} CTL_MODE_E;

// Scope state machine states for ISR-Thread synchronization
// Armed:     Waiting for trigger (setpoint crosses threshold)
// Triggered: Actively filling buffers with samples
// Full:      Buffers are full, waiting for TDW to start plotting
// Plotting:  TDW is plotting data, ISR must not write to buffers
typedef enum {Armed, Triggered, Full, Plotting} SCOPE_STATE_E;

//=============================================================
// RTOS Event Flags for Scope Synchronization (Approach 2)
// Used when SCOPE_SYNC_WITH_RTOS == 1
//=============================================================
#if SCOPE_SYNC_WITH_RTOS
#include <cmsis_os2.h>

// Event flag bit definitions
#define SCOPE_FLAG_BUFFER_FULL    (1U << 0)  // ISR sets when buffer is full
#define SCOPE_FLAG_PLOT_COMPLETE  (1U << 1)  // Thread sets when plotting is done

// Event flags object (defined in control.c)
extern osEventFlagsId_t scope_event_flags;
#endif

// Functions
void Init_Buck_HBLED(void);
void Update_Set_Current(void);

// Handler functions (callbacks)
void Control_OnOff_Handler (UI_FIELD_T * fld, int v);
void Control_IntNonNegative_Handler (UI_FIELD_T * fld, int v);
void Control_DutyCycle_Handler(UI_FIELD_T * fld, int v);

// Shared global variables
extern volatile int g_set_current_mA; // Default starting LED current
extern volatile uint16_t g_set_current_sample;
extern volatile int g_peak_set_current_mA;
extern volatile int g_flash_duration;
extern volatile int g_flash_period; 

extern volatile int g_measured_current_mA;
extern volatile int g_duty_cycle;  // global to give debugger access

#define SAM_BUF_SIZE   (960)
#define SCOPE_TRIGGER_THRESHOLD_mA  (1)

extern volatile uint16_t g_set_sample[SAM_BUF_SIZE];
extern volatile uint16_t g_meas_sample[SAM_BUF_SIZE];
extern volatile SCOPE_STATE_E g_scope_state;

extern volatile int g_enable_control;
extern volatile CTL_MODE_E control_mode;
extern volatile int g_enable_flash;
extern volatile int error;

extern SPidFX plantPID_FX;
extern SPid plantPID;

// Hardware configuration
#define ADC_SENSE_CHANNEL (8)
#define ADC_SENSE_MUXSEL (0)


#define R_SENSE (2.2f)
#define R_SENSE_MO ((int) (R_SENSE*1000))

#define V_REF (3.3f)
#define V_REF_MV ((int) (V_REF*1000))

#define ADC_FULL_SCALE (0x10000)
#define MA_SCALING_FACTOR (1000)


#ifndef DAC_POS
	#define DAC_POS 30
#endif

#define DAC_RESOLUTION 4096

#define MA_TO_DAC_CODE(i) ((i)*(2.2f*DAC_RESOLUTION/V_REF_MV))
// #define MA_TO_DAC_CODE(i) (i*2.2*DAC_RESOLUTION/V_REF_MV) // Introduces timing delay and interesting bug!

void Control_HBLED(void);

#endif // #ifndef CONTROL_H
