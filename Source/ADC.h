#ifndef ADC_H
#define ADC_H
#include <cmsis_os2.h>
#include "config.h"

/* Cannot start a low-priority conversion within the time required
 * for conversion + dequeue req + enqueue response + ISR latency.
 * measured TPM count for this at 0x1DB TPM counts, and add some headroom.
 * This should ensure that the conversion completes and results retrieved
 * BEFORE the HBLED conversion starts.
 */
#define TPM_WINDOW  100 //

typedef struct {
	uint8_t Channel, MuxSel;
	osMessageQueueId_t ResponseQueue;
} ADC_Request_t;

typedef struct {
	uint8_t Channel, MuxSel;
	uint16_t Sample;
} ADC_Response_t;

extern osMessageQueueId_t ADC_RequestQueue;
extern osMessageQueueId_t ADC_ResultQueue;

void Init_ADC(void);
uint16_t request_conversion(uint8_t channel, uint8_t muxsel);    // handles enqueuing the req and waiting for the response.
void ADC_Update_MuxSel(uint32_t);

#endif
