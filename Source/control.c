#include <MKL25Z4.h>
#include <stdio.h>
#include <stdint.h>

#include "GPIO_defs.h"
#include "debug.h"
#include "config.h"
#include "control.h"

#include "timers.h"
#include "delay.h"
#include "LEDs.h"
#include "UI.h"
#include "FX.h"

#if SCOPE_SYNC_WITH_RTOS
#include <cmsis_os2.h>
// Event flags for ISR-Thread synchronization
osEventFlagsId_t scope_event_flags;
#endif

volatile int g_duty_cycle = 5;  // global to give debugger access

volatile int g_enable_flash = 1;
volatile int g_peak_set_current_mA = FLASH_CURRENT_MA; // Peak flash current
volatile int g_flash_duration = FLASH_DURATION_MS;
volatile int g_flash_period = FLASH_PERIOD_MS; 

volatile int g_enable_control = 1;
volatile int g_set_current_mA = 0; 				// Default starting LED current, mA
volatile int g_set_current_code = 0; 				// Default starting LED current, equivalent ADC code

volatile int g_measured_current_mA = 0;
volatile int error= 0;

int32_t pGain_8 = PGAIN_8; // proportional gain numerator scaled by 2^8

volatile SCOPE_STATE_E g_scope_state = Armed; 
volatile __ALIGNED(256) uint16_t g_set_sample[SAM_BUF_SIZE];
volatile __ALIGNED(256) uint16_t g_meas_sample[SAM_BUF_SIZE];

SPid plantPID = {0, // dState
	0, // iState
	LIM_DUTY_CYCLE, // iMax
	-LIM_DUTY_CYCLE, // iMin
	P_GAIN_FL, // pGain
	I_GAIN_FL, // iGain
	D_GAIN_FL  // dGain
};

SPidFX plantPID_FX = {FL_TO_FX(0), // dState
	FL_TO_FX(0), // iState
	FL_TO_FX(LIM_DUTY_CYCLE), // iMax
	FL_TO_FX(-LIM_DUTY_CYCLE), // iMin
	P_GAIN_FX, // pGain
	I_GAIN_FX, // iGain
	D_GAIN_FX  // dGain
};

float UpdatePID(SPid * pid, float error, float position){
	float pTerm, dTerm, iTerm;

	// calculate the proportional term
	pTerm = pid->pGain * error;
	// calculate the integral state with appropriate limiting
	pid->iState += error;
	if (pid->iState > pid->iMax) 
		pid->iState = pid->iMax;
	else if (pid->iState < pid->iMin) 
		pid->iState = pid->iMin;
	iTerm = pid->iGain * pid->iState; // calculate the integral term
	dTerm = pid->dGain * (position - pid->dState);
	pid->dState = position;

	return pTerm + iTerm - dTerm;
}

FX16_16 UpdatePID_FX(SPidFX * pid, FX16_16 error_FX, FX16_16 position_FX){
	FX16_16 pTerm, dTerm, iTerm, diff, ret_val;

	// calculate the proportional term
	pTerm = Multiply_FX(pid->pGain, error_FX);

	// calculate the integral state with appropriate limiting
	pid->iState = Add_FX(pid->iState, error_FX);
	if (pid->iState > pid->iMax) 
		pid->iState = pid->iMax;
	else if (pid->iState < pid->iMin) 
		pid->iState = pid->iMin;
	
	iTerm = Multiply_FX(pid->iGain, pid->iState); // calculate the integral term
	diff = Subtract_FX(position_FX, pid->dState);
	dTerm = Multiply_FX(pid->dGain, diff);
	pid->dState = position_FX;

	ret_val = Add_FX(pTerm, iTerm);
	ret_val = Subtract_FX(ret_val, dTerm);
	return ret_val;
}

void Control_HBLED(void) {
	uint16_t res;
	FX16_16 change_FX, error_FX;
	static int sample_idx = 0;
	static int prev_set_current_mA = 0;
	static int threshold_mA = SCOPE_TRIGGER_THRESHOLD_mA;
	
	DEBUG_START(DBG_CONTROLLER_POS);
	
#if USE_ADC_INTERRUPT
	// already completed conversion, so don't wait
#else
	while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK))
		; // wait until end of conversion
#endif
	res = ADC0->R[0];
	g_measured_current_mA = (res*1500)>>16; // Extra Credit: Explain why this doesn't work: V_REF_MV*MA_SCALING_FACTOR)/(ADC_FULL_SCALE*R_SENSE)

	//=============================================================
	// SCOPE SYNCHRONIZATION: ISR Side
	// Approach selected by SCOPE_SYNC_WITH_RTOS in config.h
	//=============================================================
	
#if SCOPE_SYNC_WITH_RTOS
	//=============================================================
	// APPROACH 2: RTOS Event Flags
	// ISR uses state machine but signals thread via event flags
	//=============================================================
	switch (g_scope_state) {
		case Armed:
			// STATE: Armed - Waiting for trigger condition
			// Check for trigger: setpoint crosses threshold (low-to-high)
			if ((prev_set_current_mA < threshold_mA) && (g_set_current_mA >= threshold_mA)) {
				// Trigger condition met! Start filling buffers
				sample_idx = 0;
				g_scope_state = Triggered;
				// Start sampling immediately
				g_meas_sample[sample_idx] = res;
				g_set_sample[sample_idx] = g_set_current_code;
				sample_idx++;
			}
			// In Armed state, do NOT fill buffers
			break;
			
		case Triggered:
			// STATE: Triggered - Actively filling buffers
			g_meas_sample[sample_idx] = res;
			g_set_sample[sample_idx] = g_set_current_code;
			sample_idx++;
			
			// Check if buffer is full
			if (sample_idx >= SAM_BUF_SIZE) {
				sample_idx = 0;
				g_scope_state = Full;
				// RTOS: Signal thread that buffer is full
				osEventFlagsSet(scope_event_flags, SCOPE_FLAG_BUFFER_FULL);
			}
			break;
			
		case Full:
			// STATE: Full - Waiting for thread to start plotting
			// Do NOT write to buffers
			break;
			
		case Plotting:
			// STATE: Plotting - Thread is reading buffers
			// Do NOT write to buffers
			break;
			
		default:
			g_scope_state = Armed;
			sample_idx = 0;
			break;
	}
	
#else
	//=============================================================
	// APPROACH 1: State Machine (Polling, no RTOS mechanisms)
	// ISR and thread communicate via shared g_scope_state variable
	//=============================================================
	switch (g_scope_state) {
		case Armed:
			// STATE: Armed - Waiting for trigger condition
			// Check for trigger: setpoint crosses threshold (low-to-high)
			if ((prev_set_current_mA < threshold_mA) && (g_set_current_mA >= threshold_mA)) {
				// Trigger condition met! Start filling buffers
				sample_idx = 0;
				g_scope_state = Triggered;
				// Start sampling immediately
				g_meas_sample[sample_idx] = res;
				g_set_sample[sample_idx] = g_set_current_code;
				sample_idx++;
			}
			// In Armed state, do NOT fill buffers
			break;
			
		case Triggered:
			// STATE: Triggered - Actively filling buffers
			g_meas_sample[sample_idx] = res;
			g_set_sample[sample_idx] = g_set_current_code;
			sample_idx++;
			
			// Check if buffer is full
			if (sample_idx >= SAM_BUF_SIZE) {
				sample_idx = 0;
				g_scope_state = Full;  // Thread will poll and detect this
			}
			break;
			
		case Full:
			// STATE: Full - Buffers full, waiting for TDW to start plotting
			// Do NOT write to buffers - TDW polls state and will transition to Plotting
			break;
			
		case Plotting:
			// STATE: Plotting - TDW is reading buffers, do not write
			// Do NOT write to buffers - TDW will set state back to Armed when done
			break;
			
		default:
			// Unknown state - reset to Armed
			g_scope_state = Armed;
			sample_idx = 0;
			break;
	}
#endif
	//=============================================================
	
	prev_set_current_mA = g_set_current_mA;
	
	if (g_enable_control) {
		switch (control_mode) {
			case OpenLoop:
					// don't do anything!
				break;
			case BangBang:
				if (g_measured_current_mA < g_set_current_mA)
					g_duty_cycle = LIM_DUTY_CYCLE;
				else
					g_duty_cycle = 0;
				break;
			case Incremental:
				if (g_measured_current_mA < g_set_current_mA)
					g_duty_cycle += INC_STEP;
				else
					g_duty_cycle -= INC_STEP;
				break;
			case Proportional:
				g_duty_cycle += (pGain_8*(g_set_current_mA - g_measured_current_mA))/256; //  - 1;
			break;
			case PID:
				g_duty_cycle += UpdatePID(&plantPID, g_set_current_mA - g_measured_current_mA, g_measured_current_mA);
				break;
			case PID_FX:
				error_FX = INT_TO_FX(g_set_current_mA - g_measured_current_mA);
				change_FX = UpdatePID_FX(&plantPID_FX, error_FX, INT_TO_FX(g_measured_current_mA));
				g_duty_cycle += FX_TO_INT(change_FX);
			break;
			default:
				break;
		}
	
		// Update PWM controller with duty cycle
		if (g_duty_cycle < 0)
			g_duty_cycle = 0;
		else if (g_duty_cycle > LIM_DUTY_CYCLE)
			g_duty_cycle = LIM_DUTY_CYCLE;
		PWM_Set_Value(TPM0, PWM_HBLED_CHANNEL, g_duty_cycle);
	} // if g_enable_control
	
	// Increment sample index and wrap at SAM_BUF_SIZE
	DEBUG_STOP(DBG_CONTROLLER_POS);
}



void Set_DAC(unsigned int code) {
	// Force 16-bit write to DAC
	uint16_t * dac0dat = (uint16_t *)&(DAC0->DAT[0].DATL);
	*dac0dat = (uint16_t) code;
}

void Set_DAC_mA(unsigned int current) {
	unsigned int code = MA_TO_DAC_CODE(current);
	// Force 16-bit write to DAC
	uint16_t * dac0dat = (uint16_t *)&(DAC0->DAT[0].DATL);
	*dac0dat = (uint16_t) code;
}

void Init_DAC_HBLED(void) {
  // Enable clock to DAC and Port E
	SIM->SCGC6 |= SIM_SCGC6_DAC0_MASK;
	SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
	
	// Select analog for pin
	PORTE->PCR[DAC_POS] &= ~PORT_PCR_MUX_MASK;
	PORTE->PCR[DAC_POS] |= PORT_PCR_MUX(0);	
		
	// Disable buffer mode
	DAC0->C1 = 0;
	DAC0->C2 = 0;
	
	// Enable DAC, select VDDA as reference voltage
	DAC0->C0 = DAC_C0_DACEN_MASK | DAC_C0_DACRFS_MASK;
	Set_DAC(0);
}

void Init_ADC_HBLED(void) {
	// Configure ADC to read Ch 8 (FPTB 0)
	SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK; 
	ADC0->CFG1 = 0x0C; // 16 bit
	//	ADC0->CFG2 = ADC_CFG2_ADLSTS(3);
	ADC0->SC2 = ADC_SC2_REFSEL(0);

#if USE_ADC_HW_TRIGGER
	// Enable hardware triggering of ADC
	ADC0->SC2 |= ADC_SC2_ADTRG(1);
	// Select triggering by TPM0 Overflow
	SIM->SOPT7 = SIM_SOPT7_ADC0TRGSEL(8) | SIM_SOPT7_ADC0ALTTRGEN_MASK;
	// Select input channel 
	ADC0->SC1[0] &= ~ADC_SC1_ADCH_MASK;
	ADC0->SC1[0] |= ADC_SC1_ADCH(ADC_SENSE_CHANNEL);
#endif // USE_ADC_HW_TRIGGER

#if USE_ADC_INTERRUPT 

	// delay enabling the interrupt until the server thread is running.
#if !USE_ADC_SERVER
	// enable ADC interrupt
	ADC0->SC1[0] |= ADC_SC1_AIEN(1);
#endif
	// Configure NVIC for ADC interrupt
	NVIC_SetPriority(ADC0_IRQn, 2); 
	NVIC_ClearPendingIRQ(ADC0_IRQn); 
	NVIC_EnableIRQ(ADC0_IRQn);	
#endif // USE_ADC_INTERRUPT
}

#if 1
void Update_Set_Current(void) {
	// Does rectangular flash
	static volatile int delay = 0;
	
	if (delay == 0)					// Just for initialization from global
		delay = g_flash_period;
	
	if (g_enable_flash){
		delay--;
		if (delay == g_flash_duration) { // turn on LED now
			g_set_current_mA = g_peak_set_current_mA;
			Set_DAC_mA(g_set_current_mA);
			g_set_current_code = (g_set_current_mA<<16)/1500; 
		} else if (delay == 0) {
			delay = g_flash_period;
			g_set_current_mA = 0;	// turn off LED now
			g_set_current_code = 0; 
			Set_DAC_mA(g_set_current_mA);
		}
	}
}
#else
void Update_Set_Current(void) {
	// Ramp curent up and down 
	static volatile int t = 0;
	int phase;
	int t_ramp;
	
	if (g_enable_flash){
		t_ramp = g_flash_duration/2;
		t++;
		phase = (2*t)/g_flash_duration;
		switch (phase) {
			case 0:
				g_set_current_mA = (t*g_peak_set_current_mA)/t_ramp;
				break;
			case 1:
				g_set_current_mA = ((2*t_ramp - t)*g_peak_set_current_mA)/t_ramp;
				break;
			default:
				g_set_current_mA = 0;
				break;
		}
		Set_DAC_mA(g_set_current_mA);
		if (t > = g_flash_period)
			t = 0;
	}
}
#endif

void Init_Buck_HBLED(void) {
	Init_DAC_HBLED();
	Init_ADC_HBLED();
	
	// Configure driver for buck converter
	// Set up PTE31 to use for SMPS with TPM0 Ch 4
	SIM->SCGC5 |= SIM_SCGC5_PORTE_MASK;
	PORTE->PCR[31]  &= PORT_PCR_MUX(7);
	PORTE->PCR[31]  |= PORT_PCR_MUX(3);
	PWM_Init(TPM0, PWM_HBLED_CHANNEL, PWM_PERIOD, g_duty_cycle, 0, 0);
}

// Handler functions (callbacks)
// Default handlers
void Control_OnOff_Handler (UI_FIELD_T * fld, int v) {
	if (fld->Val != NULL) {
		if (v > 0) {
			*fld->Val = 1;
		} else {
			*fld->Val = 0;
		}
	}
}

void Control_IntNonNegative_Handler (UI_FIELD_T * fld, int v) {
	int n;
	if (fld->Val != NULL) {
		n = *fld->Val + v/16;
		if (n < 0) {
			n = 0;
		}
		*fld->Val = n;
	}
}

void Control_DutyCycle_Handler(UI_FIELD_T * fld, int v) {
	int dc;
	if (fld->Val != NULL) {
		dc = g_duty_cycle + v/16;
		if (dc < 0)
			dc = 0;
		else if (dc > LIM_DUTY_CYCLE)
			dc = LIM_DUTY_CYCLE;
		*(fld->Val) = dc;
		PWM_Set_Value(TPM0, PWM_HBLED_CHANNEL, g_duty_cycle);
	}
}
