// Maximum Power Point Tracking (MPPT) implementation

#include "solar_mppt.h"
#include "mp2762a_drv.h"
#include "user.h"

#define MPPT_DEBUG  0//1
#if MPPT_DEBUG
#define mppt_printf(...) pf_log_raw(atel_log_ctl.platform_en, ##__VA_ARGS__)
#else
#define mppt_printf(...) 
#endif

#define MPPT_INPUT_CUR_CALI_EN	1//0	// Enable/diable MPPT input current calibration. 0, diable; NOT 0, enable

static bool mppt_state = false;
static int32_t mppt_input_v_cur = 0;	// Current input voltage. Unit in mV
static double mppt_input_i_cur = 0.0;	// Current input current. Unit in mA
static double mppt_power_cur = 0.0;		// Current power. Unit in 10^(-6) W
static double mppt_power_prev = 0.0;	// Previous power. Unit in 10^(-6) W
static int32_t mppt_input_v_setting_cur = 0;	// Current input voltage setting. Unit in mV
static int32_t mppt_input_v_setting_prev = 0;	// Previous input voltage setting. Unit in mV
static const int32_t mppt_input_v_setting_min = 8500;	// Input voltage setting min limt. Unit in mV
static const int32_t mppt_input_v_setting_max = 13500;	// Input voltage setting max limt. Unit in mV
static const int32_t mppt_v_delta = 100;					// Input voltage setting variation. Unit in mV
static const double mppt_power_delta_limit = 36450;	// Power delta limit. Unit in 10^(-6) W

// static mppt_state_t mppt_status = MPPT_STATE_SCAN;
static mppt_state_t mppt_status = MPPT_STATE_IDLE;
static const int32_t mppt_input_v_settinng_limit_df = 8500;	// Input Voltage Setting default limit. Unit in mV
static const int32_t mppt_input_v_settinng_limit_l = 11000;//8500;	// Input Voltage Setting lower limt. Unit in mV
static const int32_t mppt_input_v_settinng_limit_u = 13500;	// Input Voltage Setting upper limt. Unit in mV
static double mppt_max_power = 0.0;
static int32_t mppt_input_v_setting_mark = mppt_input_v_settinng_limit_l;
static bool mppt_state_scan_1st_time = true;
static const int32_t mppt_v_step = 100;		// Input voltage setting variation. Unit in mV
static int32_t mppt_hold_cnt = 0;
static const int32_t mppt_hold_limit = 900;		// 15*60 = 900. 15 min.
static const double mppt_p_ratio_limit = 0.1;	// 10 percent

static int32_t mppt_condition_change_cnt = 0;   //the condition change cnt
static int32_t mppt_condition_change_limited_cnt = 30;   // check once time in 10 second. total: 30*10 = 5 mins.
static int32_t input_v_delta = 200; // unit mV.
extern bool flag_only_solar_check;
static bool set_mp2762a_input_v_limit = false;
// Check conditions to enter MPPT process
// Return value: true, OK; false, conditions are not met
static bool mppt_enter_conditon_check(void)
{
	bool case1 = ((PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain)   < ADC_MAIN_TH) &&
				  (PF_ADC_RAW_TO_VSIN_MV(monet_data.AdcSolar) >= ADC_SOLAR_TH));	// There is only Solar power only
				 // (PF_ADC_RAW_TO_VSIN_MV(monet_data.AdcSolar) >= ADC_MAIN_TH));	// 
	bool case2 = is_charger_power_on();
	bool case3 = (solar_chg_mode_get() == SOLAR_CHG_MODE1);
	
	if (case1 == true && case2 == true && case3 == true)
		return true;
	return false;
}

// Get input voltage value. Unit in mV
static int32_t mppt_input_v_get(void)
{
	return mp2762a_input_vol_get();
}

#if MPPT_INPUT_CUR_CALI_EN
// Calibrate Input Current of charger chip.
// JIRA NALAMCU-65: Input Current read from charger chip is not accurate in MPPT algorithm
// Param. cur_raw: current to be calibrated. Unit in mA
// Return value: calibrated current value. Unit in mA
static double mppt_input_cur_calibrate(double cur_raw)
{
	double cur = 0.0;
	
	if (cur_raw <= 50.0)
		cur = cur_raw;
	else if (cur_raw < 70.0)
		cur = cur_raw - 10;
	else if (cur_raw < 100.0)
		cur = cur_raw - 20;
	else if (cur_raw < 200.0)
		cur = cur_raw - 30;
	else
		cur = cur_raw - 60;
	return cur;
}
#endif

// Get input current value. Unit in mA
static double mppt_input_i_get(void)
{
	double cur = 0.0;
	cur = mp2762a_input_cur_get();
#if MPPT_INPUT_CUR_CALI_EN
	cur = mppt_input_cur_calibrate(cur);
#endif
	return cur;
}

// Get output power value. Unit in uW
double mppt_output_p_get(void)
{
	int32_t input_v = 0;		// Unit in mV
	double input_i = 0.0;	// Unit in mA
	double power = 0.0;		// Unit in uW
	
	if(mppt_status == MPPT_STATE_SCAN)
	{
		input_v = 0;
		input_i = 0.0;
		power = mppt_max_power;
	}
	else
	{
		input_v = mppt_input_v_get();
		input_i = mppt_input_i_get();
		power = 1.0 * input_v * input_i;
	}
	pf_log_raw(atel_log_ctl.platform_en, "mppt_output_p_get::mppt_status=%d,input_v=%d,input_i=%d,power=%d.\r",mppt_status,(int32_t)input_v,(int32_t)input_i,(int32_t)power);
	return power;
}

// Set input voltage setting. Unit in mV
static void mppt_input_v_setting_set(int32_t setting)
{
	mp2762a_input_vol_limit_set(setting);
}

// Get input voltage setting. Unit in mV
static int32_t mppt_input_v_setting_get(void)
{
	return mp2762a_input_vol_limit_get();
}

/*static*/ bool mppt_is_running(void)
{
	return mppt_state;
}

// Set MPPT process state
// Param. setting: if true, set MPPT is running; if false, set MPPT is stopped
static void mppt_state_set(bool setting)
{
	mppt_state = setting;
}

static void mppt_init(void)
{
	mppt_input_v_cur = 0;
	mppt_input_i_cur = 0.0;
	mppt_power_cur = 0.0;
	mppt_power_prev = 0.0;
	mppt_input_v_setting_cur = mppt_input_v_setting_min;
	mppt_input_v_setting_prev = mppt_input_v_setting_min;
	mppt_input_v_setting_cur = mppt_input_v_setting_cur + mppt_v_delta;
	mppt_input_v_setting_set(mppt_input_v_setting_cur);
	mppt_state_set(true);
}

// MPPT process Based on Perturbation and Observation (P&O) algrithm
// This function is designed to be called in a periodic function, e.g. one second timer handle.
void mppt_process_po(void)
{
	double power_delta = 0.0;
	
	if (mppt_enter_conditon_check() == true)
	{
		if (mppt_is_running() != true)
		{
			mppt_init();
			mppt_printf("MPPT init\r\n");
			return;
		}
		mppt_input_v_cur = mppt_input_v_get();
		mppt_input_i_cur = mppt_input_i_get();
		mppt_power_cur = 1.0 * mppt_input_v_cur * mppt_input_i_cur;
		power_delta = fabs(mppt_power_cur - mppt_power_prev);
		mppt_printf("Pcur %.3f, Pprev %.3f, Pdelta %.3f, Pdelta_limit %.3f\r\n", mppt_power_cur, mppt_power_prev, power_delta, mppt_power_delta_limit);
		if (power_delta < mppt_power_delta_limit)
		{
			// Keep unchange
			mppt_printf("Set unchange\r\n");
		}
		else if (mppt_power_cur > mppt_power_prev)
		{
			if (mppt_input_v_setting_cur > mppt_input_v_setting_prev)
			{
				mppt_input_v_setting_prev = mppt_input_v_setting_cur;
				mppt_input_v_setting_cur += mppt_v_delta;
				if (mppt_input_v_setting_cur > mppt_input_v_setting_max)
					mppt_input_v_setting_cur = mppt_input_v_setting_max;
				mppt_input_v_setting_set(mppt_input_v_setting_cur);
				mppt_power_prev = mppt_power_cur;
				mppt_printf("Pcur > Pprev, Vcur > Vprev\r\n");
			}
			else
			{
				mppt_input_v_setting_prev = mppt_input_v_setting_cur;
				mppt_input_v_setting_cur -= mppt_v_delta;
				if (mppt_input_v_setting_cur < mppt_input_v_setting_min)
					mppt_input_v_setting_cur = mppt_input_v_setting_min;
				mppt_input_v_setting_set(mppt_input_v_setting_cur);
				mppt_power_prev = mppt_power_cur;
				mppt_printf("Pcur > Pprev, Vcur <= Vprev\r\n");
			}
		}
		else
		{
			if (mppt_input_v_setting_cur > mppt_input_v_setting_prev)
			{
				mppt_input_v_setting_prev = mppt_input_v_setting_cur;
				mppt_input_v_setting_cur -= mppt_v_delta;
				if (mppt_input_v_setting_cur < mppt_input_v_setting_min)
					mppt_input_v_setting_cur = mppt_input_v_setting_min;
				mppt_input_v_setting_set(mppt_input_v_setting_cur);
				mppt_power_prev = mppt_power_cur;
				mppt_printf("Pcur <= Pprev, Vcur > Vprev\r\n");
			}
			else
			{
				mppt_input_v_setting_prev = mppt_input_v_setting_cur;
				mppt_input_v_setting_cur += mppt_v_delta;
				if (mppt_input_v_setting_cur > mppt_input_v_setting_max)
					mppt_input_v_setting_cur = mppt_input_v_setting_max;
				mppt_input_v_setting_set(mppt_input_v_setting_cur);
				mppt_power_prev = mppt_power_cur;
				mppt_printf("Pcur <= Pprev, Vcur <= Vprev\r\n");
			}
		}
	}
	else
	{
		if (mppt_is_running() == true)
		{
			mppt_state_set(false);
			mppt_printf("MPPT deinit\r\n");
		}
	}
	mppt_printf("state %u, v %u mV, i %.2f mA, Pcur %.3f uW, Pprev %.3f uW, v_cfg_cur %u mV, v_cfg_prev %u mV\r\n", 
					mppt_is_running(), mppt_input_v_cur, mppt_input_i_cur, 
					mppt_power_cur, mppt_power_prev, 
					mppt_input_v_setting_cur, mppt_input_v_setting_prev);
}

// Not used
// static bool solar_pw_low_power_flag = false;

// // Set backdoor function for MPPT algorithm. Called in low power mode (solar power checking)
// void mppt_power_check_for_low_power_en(void)
// {
// 	solar_pw_low_power_flag = true;
// }

// static bool mppt_power_check_for_low_power_state(void)
// {
// 	return solar_pw_low_power_flag;
// }

static void solar_power_check(void)
{
	int32_t input_v = 0;	// Unit in mV
	double input_i = 0.0;	// Unit in mA
	double power = 0.0;		// Unit in uW
	
	input_v = mppt_input_v_get();
	input_i = mppt_input_i_get();
	power = 1.0 * input_v * input_i;
	
	pf_log_raw(atel_log_ctl.platform_en, "solar_power_check() %d mV, %u mA, %u uW\r", (input_v + input_v_delta), (uint32_t)input_i, (uint32_t)power);
	if (power < SOLAR_POWER_LIMIT)
	{
		// if (mppt_power_check_for_low_power_state() == false)
		{
			// mppt_printf("solar_power_check() Power < 1 W, set to Mode 2 and enable timer for it\r");
            pf_log_raw(atel_log_ctl.platform_en, "solar_power_check() Power < 1 W\r");
            // if ((input_v < VOL_LIMIT_S_CHG_MODE_SEL) ||
            //     (monet_data.charge_mode2_disable == 0))
            // {
            //     solar_chg_mode_select(FUNC_JUMP_POINT_0);
            // }
		
				if ((input_v + input_v_delta) < VOL_LIMIT_S_CHG_MODE_SEL)  // if input_V < 11V,  
				{
                    mppt_condition_change_cnt = 0;
					// if (monet_data.charge_mode2_disable != 0)
					// {
					// 	//mppt_process_nml_deinit(); 
					// 	solar_chg_mode_select(FUNC_JUMP_POINT_4);
        			// 	pf_log_raw(atel_log_ctl.platform_en,">>Input_v < 11V && Vbat > 8.2V choose to FUNC_JUMP_POINT_4  \r");
					// }
                    // else
					{
						solar_chg_mode_select(FUNC_JUMP_POINT_4);//for <1W && bat<8.2v && input_v < 11v, choose mode 2 
        				pf_log_raw(atel_log_ctl.platform_en,">>Mode1 change to off status \r");
					}                   
				}
				else
				{
					if (monet_data.charge_mode2_disable != 0)
					{
                        mppt_condition_change_cnt++;
						// solar_chg_mode_select(FUNC_JUMP_POINT_1);
						pf_log_raw(atel_log_ctl.platform_en,">>Bat high 8.2V, still in mode 1  condition_cnt(%d)\r",mppt_condition_change_cnt);
                        if (mppt_condition_change_cnt == mppt_condition_change_limited_cnt)
                        {
                           mppt_process_nml_deinit();
                           mppt_condition_change_cnt = 0;
						//    mppt_hold_cnt = 0; 				// Avoid scan at same time error.
                        } 
					}
					else
					{
                        mppt_condition_change_cnt = 0;
						solar_chg_mode_select(FUNC_JUMP_POINT_3); //for <1W && bat<8.2v, choose mode 2 
						pf_log_raw(atel_log_ctl.platform_en,">>Mode1 change to mode2 \r");	
					}
				}
		}
		// else
		// 	solar_chg_stage_switch_3_to_2();
	}
	else  //p > 1w
	{
		// if (monet_data.charge_mode1_disable) //if critical
		// {
		// 	solar_chg_mode_select(FUNC_JUMP_POINT_5);
		// }
	}
}

// MPPT process. Normal algrithm.
// Scan input voltage setting in Vstart ~ Vend, and select the V with max power.
// Hold 15 minutes, then check the power again. If it gets 10% change, re-scan.
// This function is designed to be called in a 1 second period function
void mppt_process_nml(void)
{
	int32_t input_v = 0;	// Unit in mV
	double input_i = 0.0;	// Unit in mA
	double power = 0.0;		// Unit in uW
	int32_t input_v_setting = 0;	// Unit in mV
	double power_delta = 0.0;

	uint32_t input_lowsolar_vol = 0;
	double input_lowsolar_cur = 0.0;
	double input_lowsolar_p = 0.0;

	//0819 due to support low power to solar mode2 charging
	
	if (mppt_enter_conditon_check() == true)
	{
		if (set_mp2762a_input_v_limit == false )
		{
			mp2762a_input_vol_limit_set(MP2762A_INPUT_VOL_LIMIT_SOL);
			set_mp2762a_input_v_limit = true;
			pf_log_raw(atel_log_ctl.platform_en, "set input v limit \r");
			return;
		}
		
		if (mppt_status == MPPT_STATE_IDLE)
		{
			input_lowsolar_vol = mp2762a_input_vol_get();
			input_lowsolar_cur = mp2762a_input_cur_get();
			//input_lowsolar_cur = mppt_input_i_get();
			input_lowsolar_p = input_lowsolar_vol * input_lowsolar_cur;
			mppt_printf("MPPT,solar V: %d mV, I: %.2f mA, P: %.2f uW \r\n", input_lowsolar_vol, input_lowsolar_cur, input_lowsolar_p);
			pf_log_raw(atel_log_ctl.platform_en, "MPPT,solar V: %u mV, I: %u mA, P: %u uW \r", input_lowsolar_vol, (uint32_t)input_lowsolar_cur, (uint32_t)input_lowsolar_p);
		}

		if ((input_lowsolar_p > SOLAR_POWER_LIMIT) ||
            (monet_data.charge_mode2_disable == 1) ||
			(mppt_status == MPPT_STATE_SCAN)      ||
			(mppt_status == MPPT_STATE_HOLD) )
		{

			if(flag_only_solar_check == true)
			{
				return;
			}
			monet_data.charge_status = CHARGE_STATUS_SOLAR_MODE_1_PROCESS;
			pf_log_raw(atel_log_ctl.platform_en, 
						"mppt_process_nml mppt runnig(%d)  (%d).\r", 
						mppt_status,mppt_hold_cnt);

			if (mppt_is_running() != true)
			{
				mppt_printf("MPPT init\r\n");
				mppt_state_set(true);
				mppt_status = MPPT_STATE_SCAN;
				mppt_state_scan_1st_time = true;
			}
			
			if (mppt_status == MPPT_STATE_SCAN)
			{
				if (mppt_state_scan_1st_time == true)
				{
					mppt_printf("MPPT status SCAN\r\n");
					mppt_state_scan_1st_time = false;
					mppt_max_power = 0.0;
					mppt_input_v_setting_mark = mppt_input_v_settinng_limit_l;
					mppt_input_v_setting_set(mppt_input_v_settinng_limit_l);	// Scan starts at lower limit
					return;
				}
				input_v = mppt_input_v_get();
				input_i = mppt_input_i_get();
				power = 1.0 * input_v * input_i;
				input_v_setting = mppt_input_v_setting_get();
				mppt_printf("MPPT, V: %d mV, I: %.2f mA, P: %.2f uW, Vsetting: %d mV\r\n", input_v, input_i, power, input_v_setting);
				if (power > mppt_max_power)
				{
					mppt_max_power = power;
					mppt_input_v_setting_mark = input_v_setting;
				}
				input_v_setting += mppt_v_step;
				if (input_v_setting > mppt_input_v_settinng_limit_u)	// Status SCAN ends
				{
					mppt_input_v_setting_set(mppt_input_v_setting_mark);
					mppt_status = MPPT_STATE_HOLD;
					mppt_state_scan_1st_time = true;
					
					pf_log_raw(atel_log_ctl.platform_en, 
								"MPPT, Pmax: "NRF_LOG_FLOAT_MARKER" uW, Vsetting: %d mV\r", 
								NRF_LOG_FLOAT(mppt_max_power), 
								mppt_input_v_setting_mark);
					pf_log_raw(atel_log_ctl.platform_en, "MPPT status is set to HOLD\r");
				}
				else
				{
					mppt_input_v_setting_set(input_v_setting);
					pf_log_raw(atel_log_ctl.platform_en, 
								"MPPT status SCAN InputSet(%d Mv)\r", 
								input_v_setting);
				}
			}
			else if (mppt_status == MPPT_STATE_HOLD)
			{
				mppt_printf("MPPT status HOLD\r\n");
				mppt_hold_cnt++;
				if (mppt_hold_cnt % 10 == 0)	// Check the solar power every 10 seconds
					solar_power_check();
				if (mppt_hold_cnt >= mppt_hold_limit)
				{
					mppt_hold_cnt = 0;
					input_v = mppt_input_v_get();
					input_i = mppt_input_i_get();
					power = 1.0 * input_v * input_i;
					input_v_setting = mppt_input_v_setting_get();
					mppt_printf("MPPT, V: %d mV, I: %.2f mA, P: %.2f uW, Vsetting: %d mV\r\n", input_v, input_i, power, input_v_setting);
					power_delta = fabs(mppt_max_power - power);
					if ((mppt_max_power == 0.0 && (power > mppt_max_power)) ||	// When divisor is zero
						((mppt_max_power != 0.0) && ((power_delta / mppt_max_power) > mppt_p_ratio_limit)))	// Status HOLD ends
					{
						mppt_status = MPPT_STATE_SCAN;	// Switch to Scan status
						mppt_printf("P %.2f, power_delta %.2f, Pmax %.2f\r\n", power, power_delta, mppt_max_power);
						mppt_printf("MPPT status is set to SCAN\r\n");
					}
				} // if (mppt_hold_cnt > mppt_hold_limit)
			} // else if (mppt_status == MPPT_STATE_HOLD)
		} // if (mppt_enter_conditon_check() == true)		
		else
		{
			if(flag_only_solar_check == false)
			{
				setChargerOff();
            	solar_chg_mode_set(SOLAR_CHG_MODE2);
            	timer_solar_chg_mode_restart();
            	monet_data.charge_status = CHARGE_STATUS_SOLAR_MODE_2;
            	mppt_printf("mppt_process_nml(), set to Mode 2\r");
				pf_log_raw(atel_log_ctl.platform_en,"mppt_process_nml(), set to Mode 2\r");
				mppt_max_power = 0.0;	// Clear this value to avoid misuse in other place
				//mppt_input_v_setting_set(mppt_input_v_settinng_limit_df);	// Recover input voltage to default value. Otherwise there is risk that Main/AUX power cannot charge battery.
				mppt_hold_cnt = 0;
				set_mp2762a_input_v_limit = false;
				mppt_state_set(false);
				mppt_status = MPPT_STATE_IDLE;
			}

		}		
	}
	else
	{
        pf_log_raw(atel_log_ctl.platform_en, "mppt_process_nml mppt not runnig.\r");

		if (mppt_is_running() == true)
		{
			mppt_printf("MPPT deinit\r\n");
			mppt_state_set(false);
			mppt_max_power = 0.0;	// Clear this value to avoid misuse in other place
			mppt_input_v_setting_set(mppt_input_v_settinng_limit_df);	// Recover input voltage to default value. Otherwise there is risk that Main/AUX power cannot charge battery.
			mppt_hold_cnt = 0;
			set_mp2762a_input_v_limit = false;
			mppt_status = MPPT_STATE_IDLE;
		}
	}
}

// De-init MPPT nml algorithm.
// Called when mppt_process_nml() cannot deinit itself.
void mppt_process_nml_deinit(void)
{
	if (mppt_is_running() == true)
	{
		mppt_printf("MPPT deinit\r\n");
		CRITICAL_REGION_ENTER();
		mppt_state_set(false);
		mppt_max_power = 0.0;	// Clear this value to avoid misuse in other place
		CRITICAL_REGION_EXIT();
		mppt_input_v_setting_set(mppt_input_v_settinng_limit_df);	// Recover input voltage to default value. Otherwise there is risk that Main/AUX power cannot charge battery.
		CRITICAL_REGION_ENTER();
		mppt_hold_cnt = 0;
		set_mp2762a_input_v_limit = false;
		mppt_status = MPPT_STATE_IDLE;
		CRITICAL_REGION_EXIT();
	}
}

// Scan input voltage setting from 8.5 V to 13.5 V and get respective info.
void mppt_test(void)
{
	int32_t input_v = 0;	// Unit in mV
	double input_i = 0.0;	// Unit in mA
	double power = 0.0;		// Unit in uW
	int32_t input_v_setting = 0;	// Unit in mV
	
	input_v = mppt_input_v_get();
	input_i = mppt_input_i_get();
	power = 1.0 * input_v * input_i;
	power = power;	// To avoid compiler warning
	input_v_setting = mppt_input_v_setting_get();
	mppt_printf("Input V: %d mV, input I: %.2f mA, power: %.2f uW, input V setting: %d mV\r\n", input_v, input_i, power, input_v_setting);
	input_v_setting += 100;	// 100 mV
	if (input_v_setting > 13500)
		input_v_setting = 8500;
	mppt_input_v_setting_set(input_v_setting);
}

