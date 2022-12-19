#ifndef __SOLAR_MPPT_H__
#define __SOLAR_MPPT_H__

#include "stdbool.h"

typedef enum
{
	MPPT_STATE_SCAN = 0,
	MPPT_STATE_HOLD,
	MPPT_STATE_IDLE,
} mppt_state_t;

#define SOLAR_POWER_LIMIT			1000000	// Power limit for solar charging mode switch. Unit in uW.

#define ADC_BAT_TH 6400  // SIMBAMCU-36 SIMBAMCU-26 MVtoADC (6400UL, BUB_ADC_INPUT_IMPEDANCE_RATIO, 270UL) - diode drop 270K 150K resitor divider
#define ADC_BAT_OK 7400  // SIMBAMCU-36 SIMBAMCU-26 MVtoADC (7400UL, BUB_ADC_INPUT_IMPEDANCE_RATIO, 270UL) - diode drop 270K 150K resitor divider
#define ADC_MAIN_TH 6000 // SIMBAMCU-36 SIMBAMCU-26 MVtoADC (6000UL, MAIN_ADC_INPUT_IMPEDANCE_RATIO, 887UL) - diode drop 88.7K 10K resitor divider
#define ADC_SOLAR_TH (3000)

double mppt_output_p_get(void);
void mppt_process_po(void);
void mppt_process_nml(void);
void mppt_test(void);
bool mppt_is_running(void);
void mppt_power_check_for_low_power_en(void);
void mppt_process_nml_deinit(void);
#endif	// #ifndef __SOLAR_MPPT_H__
