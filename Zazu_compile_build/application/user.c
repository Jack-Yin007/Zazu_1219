/*
 * user.c
 *
 *  Created on: Dec 12, 2015
 *      Author: J
 */

#include "user.h"
#include "mp2762a_drv.h"
#include "charger_process.h"
#include "solar_mppt.h"
#include "rtcclock.h"
#include "nrf_drv_clock.h"
#include <inttypes.h>

/* Define Slave Address  ---------------------------------------------------*/
typedef enum {RESET = 0, SET = !RESET} BitStatus;

static uint16_t gcount = 0;
static uint32_t count1sec = 0;

static bool eventIReadyFlag = false;
static bool exit_from_lower_power_mode = false;

gpio_conf monet_conf;
gpio_data monet_gpio = {0};
monet_struct monet_data = {0};
debug_tlv_info_t debug_tlv_info = {0};
power_state_t  power_state = {0};

atel_log_ctl_t atel_log_ctl = 
{
    .normal_en = 1,
    .core_en = 1,
    .io_protocol_en = 1,
    .irq_handler_en = 1,
    .platform_en = 1,
    .warning_en = 1,
    .error_en = 1,
};

device_reset_info_t reset_info = {0};

device_shipping_mode_t device_shipping_mode = {0};

uint8_t ring_buffer_from_mcu[RING_BUFFER_SIZE_FROM_MCU] = {0};


const char *charge_status_string[] = 
{
    "SOLAR_MODE_1_PROCESS",
    "SOLAR_MODE_1_FULL",
    "SOLAR_MODE_2",
    "SOLAR_MODE_2",
    "SOLAR_LOWPOWER_OFF",
    "EXTERNAL_POWER_PROCESS",
    "EXTERNAL_POWER_FULL",
    "NO_SOURCES",
    "NO_SOURCES_MODE_2"

};

#define GPIO_GET(index) (bool)((monet_gpio.gpiolevel >>index)&0x1)
#define GPIO_SET(index, s)  monet_gpio.gpiolevel = (s>0) ? ((1<<index)|monet_gpio.gpiolevel): (monet_gpio.gpiolevel & (~(1<<index)))

void BleConnectionHeartBeat(uint32_t second);
void monet_Scommand(void);
void monet_xAAcommand_nala_exception(uint8_t* pParam, uint8_t Length);
void monet_xDDcommand(uint8_t* pParam, uint8_t Length);

/******************************************************************************/
/* User Functions                                                             */
/******************************************************************************/
void disableEventIReadyFlag(void) {
    eventIReadyFlag = false;
}

void systemreset(uint8_t flag)
{
    // NVIC_SystemReset();
    while (1);
}

void charger_chip_detect(void)
{
    uint32_t i = 0;

    for (i = 0; i < 10; i++)
    {
        monet_data.CANExistChargerNoExist = (charger_exist_probe() == 1) ? 0 : 1;
        if (monet_data.CANExistChargerNoExist == 0)
        {
            break;
        }
    }

    if (ZAZU_HARDWARE_VERSION == ZAZU_HARDWARE_NOCAN_WITHCHARGER)
    {
        monet_data.CANExistChargerNoExist = 0;
        monet_data.ChargerRestartValue = 0;       // Here keep in sync with nala.
    }

    pf_log_raw(atel_log_ctl.core_en, 
               "Zazu Started: Charger chip noexist(%d).\r", 
               monet_data.CANExistChargerNoExist);
}

// This function contians charger chip detection
void power_on_condition_check(uint8_t alreay_init)
{
    uint16_t adc_bat = 0;
    uint16_t adc_vin = 0;

    charger_chip_detect();

    monet_data.mux_can = 1;
    if (exit_from_lower_power_mode == true && reset_info.reset_from_dfu == 1)
    {
        uint8_t a = 100, b = 0, c = 0, d = 0, e = 0;

        while (a--)
        {
            if (pf_gpio_read(GPIO_CAN_UART_SWITCH_KEY) == 0)
            {
                b++;
            }

            if (pf_gpio_read(GPIO_POWER_KEY) == 0)
            {
                c++;
            }

            atel_adc_conv_force();
            adc_bat = (uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup);
            adc_vin = (uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain);
             
            if (adc_bat < BUB_THRESHOLD_CHANGE_TEMP)
            {
                monet_data.temp_set = CHG_TEMP_LIMIT5;
            }
            else
            {
                monet_data.temp_set = CHG_TEMP_LIMIT4;
            }


            if (adc_bat >= monet_data.batCriticalThresh)
            {
                d++;
            }

            if (adc_vin >= MAIN_ADC_MAIN_VALID)
            {
                e++;
            }

            nrf_delay_ms(30);
        }

        if (b >= 20)
        {
            if (monet_data.CANExistChargerNoExist)
            {
                //monet_data.mux_can = 0;
            }
        }

        if (c >= 60)
        {
            reset_info.button_power_on = 1;
        }

        if (d >= 60)
        {
            monet_data.batPowerValid = 1;
        }

        if (e >= 60)
        {
            monet_data.mainPowerValid = 1;
        }

        pf_log_raw(atel_log_ctl.core_en, 
                   "Zazu Started: S(%d) P(%d) B(%d) M(%d).\r", 
                   b, c, d, e);
        
        //pf_gpio_write(GPIO_MCU_C, monet_data.mux_can);

        // #if !DEVICE_POWER_KEY_SUPPORT
        if ((monet_data.batPowerValid == 0) && 
            (monet_data.mainPowerValid == 0))
        {
            monet_data.batPowerSave = 1;
        }
        else
        {
            monet_data.batPowerSave = 0;
        }

        pf_log_raw(atel_log_ctl.error_en,
                   "Power on fail: MainValid(%d) BatValid(%d) KeyValid(%d).\r",
                   monet_data.mainPowerValid,
                   monet_data.batPowerValid,
                   reset_info.button_power_on);

    }
   
    if (reset_info.reset_from_dfu == 0 ) 
    {
        uint8_t i = 100, j = 0, k = 0, l = 0, m = 0;

        while (i--)
        {
            if (pf_gpio_read(GPIO_CAN_UART_SWITCH_KEY) == 0)
            {
                j++;
            }

            if (pf_gpio_read(GPIO_POWER_KEY) == 0)
            {
                k++;
            }

            atel_adc_conv_force();
            adc_bat = (uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup);
            adc_vin = (uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain);

            if (adc_bat < BUB_THRESHOLD_CHANGE_TEMP)
            {
                monet_data.temp_set = CHG_TEMP_LIMIT5;
            }
            else
            {
                monet_data.temp_set = CHG_TEMP_LIMIT4;
            }


            if (adc_bat >= monet_data.batCriticalThresh)
            {
                l++;
            }

            if (adc_vin >= MAIN_ADC_MAIN_VALID)
            {
                m++;
            }

            nrf_delay_ms(30);
        }

        if (j >= 20)
        {
            if (monet_data.CANExistChargerNoExist)
            {
                monet_data.mux_can = 0;
            }
        }

        if (k >= 60)
        {
            reset_info.button_power_on = 1;
        }

        if (l >= 60)
        {
            monet_data.batPowerValid = 1;
        }

        if (m >= 60)
        {
            monet_data.mainPowerValid = 1;
        }

        pf_log_raw(atel_log_ctl.core_en, 
                   "Zazu Started: S(%d) P(%d) B(%d) M(%d).\r", 
                   j, k, l, m);
        
        pf_gpio_write(GPIO_MCU_C, monet_data.mux_can);

        #if !DEVICE_POWER_KEY_SUPPORT
        if ((monet_data.batPowerValid == 0) && 
            (monet_data.mainPowerValid == 0))
        {
            monet_data.batPowerSave = 1;
        }
        else
        {
            monet_data.batPowerSave = 0;
        }
        pf_log_raw(atel_log_ctl.core_en,
                   "Zazu Started: power save mode(%d).\r",
                   monet_data.batPowerSave);
        #endif /* DEVICE_POWER_KEY_SUPPORT */

        if (monet_data.mainPowerValid == 1)
        {
            if (!alreay_init)
            {
                pf_device_power_normal_indicate();
                pf_log_raw(atel_log_ctl.core_en, "Zazu Started: Main power valid.\r");
                return;
            }
            // pf_device_power_normal_indicate();
            // pf_log_raw(atel_log_ctl.core_en, "Zazu Started: Main power valid.\r");
            // return;
        }

        if ((monet_data.batPowerValid == 1)
            #if DEVICE_POWER_KEY_SUPPORT
             && (reset_info.button_power_on == 1)
            #endif /* DEVICE_POWER_KEY_SUPPORT */
           )
        {
            if (!alreay_init)
            {
                pf_device_power_normal_indicate();
                pf_log_raw(atel_log_ctl.core_en, "Zazu Started: Bat and Key valid.\r");
                return;
            }
            // pf_device_power_normal_indicate();
            // pf_log_raw(atel_log_ctl.core_en, "Zazu Started: Bat and Key valid.\r");
            // return;
        }

        pf_log_raw(atel_log_ctl.error_en,
                   "Power on fail: MainValid(%d) BatValid(%d) KeyValid(%d).\r",
                   monet_data.mainPowerValid,
                   monet_data.batPowerValid,
                   reset_info.button_power_on);
        #if DEVICE_POWER_KEY_SUPPORT
        pf_gpio_write(GPIO_V_DEV_EN, 0);
        pf_gpio_write(GPIO_MCU_D, 1);
        pf_gpio_write(GPIO_MCU_E, 0);
        nrf_delay_ms(1000);
        nrf_gpio_pin_clear(P011_VSYS_EN);
        nrf_gpio_cfg_output(P011_VSYS_EN);
        while (1);
        #endif /* DEVICE_POWER_KEY_SUPPORT */
    }
    else
    {
        pf_gpio_write(GPIO_MCU_C, monet_data.mux_can);
        if (!alreay_init)
        {
            pf_device_power_normal_indicate();
        }
    }
}

void InitApp(uint8_t alreay_init)
{
    if (!alreay_init)
    {
        memset(&monet_data, 0, sizeof(monet_data));
    }

    monet_data.firstPowerUp = 1;
    monet_data.restart_indicate = 1;
    monet_data.batPowerCritical = 0;
    monet_data.ble_disconnnet_delayTimeMs = 0;
    /* Setup analog functionality and port direction */
    io_tx_queue_init();

    init_config();
    gpio_init();

    if (!alreay_init)
    {
        monet_data.ldr_enable_status = 0;
        monet_data.led_flash_enable = 0;
    }
 

    monet_data.AdcBackup = 0;
    monet_data.MainState = MAIN_UNKNOWN;
    monet_data.debounceBatValidMs = BUB_VALID_DEBOUNCE_MS;

    monet_data.iorxframe.state = IO_WAIT_FOR_DOLLAR;

    monet_data.boot_major    =  (DFU_FLAG_REGISTER2 >>   0) & 0xff;
    monet_data.boot_minor    =  (DFU_FLAG_REGISTER2 >>   8) & 0xff;
    monet_data.boot_revision =  (DFU_FLAG_REGISTER2 >>  16) & 0xff;
    monet_data.boot_build    =  (DFU_FLAG_REGISTER2 >>  24) & 0xff;

    if (alreay_init == 0)
    {
        monet_data.batCriticalThresh = BUB_CRITICAL_THRESHOLD;  // Threshold MV
        monet_data.batLowThresh = BUB_LOW_THRESHOLD;   
    }

    #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
    ble_connection_channel_init();
    #endif /* BLE_FUNCTION_ONOFF */

    if (!alreay_init)
    {
        pf_adc_ready_timer_init();
        pf_gpio_pattern_timer_init();
    }

    power_on_condition_check(alreay_init);

    // Init solar charger chip procedure
    if (monet_data.CANExistChargerNoExist == 0)
    {
        uint16_t adc_bat = 0, i = 0;

        if (reset_info.reset_from_dfu == 1)
        {
            for (i = 5; i > 0; i--)
            {
                atel_adc_conv_force();
            }
        }

        adc_bat = (uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup);
        if (adc_bat <= CHARGING_MODE2_DISABLE_LOW_THRESHOLD)
        {
            monet_data.charge_mode2_disable = 0;
        }
        else if (adc_bat >= CHARGING_MODE2_DISABLE_HIGH_THRESHOLD)
        {
            monet_data.charge_mode2_disable = 1;
        }
        if (adc_bat <= monet_data.batCriticalThresh)
        {
            monet_data.charge_mode1_disable = 1;
        }
        else if (adc_bat >= monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_DELTA)
        {
            monet_data.charge_mode1_disable = 0;
        }

        // if (monet_data.charge_mode1_disable)
        // {
        //     solar_chg_mode_set(SOLAR_CHG_MODE2);
        // }
        if (!alreay_init)
        {
            pf_timer_solar_chg_mode_init();
        }

        solar_chg_mode_set(SOLAR_CHG_MODE1);
        nrf_delay_ms(1000);

        setChargerOff();	// Charger should be disable before ADC, or the solar ADC value would not be correct

        atel_adc_conv_force();
        atel_adc_conv_force();

        solar_chg_mode_select(FUNC_JUMP_POINT_0);
    }

    if (!alreay_init)
    {
        device_button_pattern_init();
    }
    

    monet_data.sysTickUnit = TIME_UNIT_IN_SLEEP_NORMAL;
    pf_systick_start(monet_data.sysTickUnit);

    if (monet_data.batPowerSave == 0)
    {
        turnOnCamera();
        ble_send_timer_start();
    }

    #if SPI_CAN_CONTROLLER_EN
    if (monet_data.CANExistChargerNoExist == 1)
    {
        pf_spi_init();
        can_controller_init();
    }
    #endif /* SPI_CAN_CONTROLLER_EN */

    // Move to main()
    // pf_wdt_init();

    if (!alreay_init)
    {
        monet_gpio.Intstatus |= MASK_FOR_BIT(INT_POWERUP); // Added power up flag to watch for Power up resets
    }
    
}

void io_tx_queue_init(void)
{
    ring_buffer_init(&monet_data.txQueueU1, ring_buffer_from_mcu, RING_BUFFER_SIZE_FROM_MCU);
}

void init_config(void)
{
    setdefaultConfig(pf_gpio_read(GPIO_MCU_BAT_EN));

    // If not first start up, keep the value of monet_gpio.Intstatus.
    memset(&monet_gpio.counter, 0, sizeof(monet_gpio) - ((uint32_t)(&monet_gpio.counter) - (uint32_t)(&monet_gpio.Intstatus)));
}

void setdefaultConfig(uint8_t bat_en)
{
    uint32_t i;
    uint8_t status = PIN_STATUS(1,0,1,0);

    for(i=0; i< NUM_OF_GPIO_PINS; i++)
    {
        monet_conf.gConf[i].status = status;
        monet_conf.gConf[i].Reload = 0;
    }

    #ifdef ATEL_GPIO_ASSOC
    #undef ATEL_GPIO_ASSOC
    #endif /* ATEL_GPIO_ASSOC */

    #define ATEL_GPIO_ASSOC(enum,x,y,z,s) do {monet_conf.gConf[enum].status = (uint8_t)PIN_STATUS(x, y, z, s);} while(0);
    
    ATEL_ALL_GPIOS
    #undef ATEL_GPIO_ASSOC

    monet_conf.gConf[GPIO_V_DEV_EN].Reload = 0;

    monet_conf.IntPin = GPIO_TO_INDEX(GPIO_NONE);
    monet_conf.WD.Pin = GPIO_TO_INDEX(GPIO_NONE);

    monet_conf.WD.Reload = WATCHDOG_DEFAULT_RELOAD_VALUE; //in second
    monet_gpio.WDtimer = monet_conf.WD.Reload;
    monet_gpio.WDflag = 0;
}

void gpio_init(void)
{
    uint8_t i;

    for(i = 0; i< NUM_OF_GPIO; i++)
    {
        if ((i != GPIO_V_DEV_EN) &&
            (i != GPIO_DEV_EN) &&
            #if DEVICE_POWER_KEY_SUPPORT
            (i != GPIO_SYS_PWR_EN) &&
            #endif /* DEVICE_POWER_KEY_SUPPORT */
            (i != GPIO_MCU_DEV_IO_EN) &&
            (i != GPIO_MCU_SLEEP_DEV) &&
            (i != GPIO_MCU_SLEEP_DEV1) &&
            (i != GPIO_CHRG_SLEEP_EN) &&
            (i != GPIO_SOLAR_CHARGE_SWITCH))
        {
            if ((monet_data.SleepState == SLEEP_HIBERNATE) || (monet_data.cameraPowerAbnormal))
            {
                if (i == GPIO_MCU_C)
                {
                    continue;
                }
            }

            configGPIO(i, monet_conf.gConf[i].status);

            monet_gpio.counter[i] = monet_conf.gConf[i].Reload;
            if((monet_conf.gConf[i].status & GPIO_DIRECTION) == DIRECTION_OUT) {
                SetGPIOOutput(i, (bool)((monet_conf.gConf[i].status & GPIO_SET_HIGH)>0));
            } else {
                GPIO_SET(i, pf_gpio_read(i));
            }
        }
    }
}

void configGPIO(int index, uint8_t status)
{
    atel_gpio_cfg_t gpio_cfg;

    if (index >= GPIO_TO_INDEX(GPIO_NONE))
    {
        pf_log_raw(atel_log_ctl.error_en, "configGPIO Index Out of Range.\r");
        return;
    }

    if((status & GPIO_DIRECTION) == DIRECTION_OUT)
    {
        if(status & GPIO_MODE)
        {
            gpio_cfg.func = ATEL_GPIO_FUNC_OUT;
            gpio_cfg.pull = ATEL_GPIO_NOPULL;
        } else {
            gpio_cfg.func = ATEL_GPIO_FUNC_OD;
            gpio_cfg.pull = ATEL_GPIO_PULLUP;
        }

        pf_gpio_cfg(index, gpio_cfg);
    }
    else
    {
        if(status & GPIO_MODE)
        {
            gpio_cfg.func = ATEL_GPIO_FUNC_INT;
            gpio_cfg.sense = ATEL_GPIO_SENSE_TOGGLE;
            gpio_cfg.pull = ATEL_GPIO_NOPULL;

            pf_gpio_cfg(index, gpio_cfg);
        }
        else
        {
            gpio_cfg.func = ATEL_GPIO_FUNC_IN;
            gpio_cfg.pull = ATEL_GPIO_NOPULL;
            pf_gpio_cfg(index, gpio_cfg);
        }
    }
}

void SetGPIOOutput(uint8_t index, bool Active)
{
    if (index >= GPIO_TO_INDEX(GPIO_NONE))
    {
        pf_log_raw(atel_log_ctl.error_en, "SetGPIOOutput Index Out of Range.\r");
        return;
    }

    // IRQ_OFF
    if (((monet_conf.gConf[index].status & GPIO_OUTPUT_HIGH) && Active) ||
        (!(monet_conf.gConf[index].status & GPIO_OUTPUT_HIGH) && !Active)) {
        pf_gpio_write(index, 1);
    } else {
        pf_gpio_write(index, 0);
    }

    GPIO_SET(index, pf_gpio_read(index));

    if(monet_conf.gConf[index].Reload) {
        if(Active) {
            monet_gpio.counter[index] = monet_conf.gConf[index].Reload;
        } else {
            monet_gpio.counter[index] = 0;
        }
    }
    // IRQ_ON
}

#define PAIRING_SUCCESS_DELAY_CNT         (4)
#define PAIRING_BOTTON_DETECT_DEBOUNCE    (4)
static void led_state_recovery_from_botton(void)
{
    pf_log_raw(atel_log_ctl.core_en, "led_state_recovery_from_botton (%d:%d %d).\r",power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button,
                                                                                    power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button,
                                                                                    reset_info.ble_peripheral_paired);
    //static uint8_t pairing_success_num = PAIRING_SUCCESS_DELAY_CNT;
    static uint8_t pairing_botton_detect_debounce = PAIRING_BOTTON_DETECT_DEBOUNCE;

    if ((power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button == 1) || (power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button == 1) || 
        (power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button == 1) || (power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button == 1))
    {
        if (!power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button && !power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button)
        {
            if (pairing_botton_detect_debounce)   //due to the device entering pairing mode by pressing the botton at least
            {                                     // 15s, here should debounce timer to detect the botten press
                pairing_botton_detect_debounce--;
                if (!pairing_botton_detect_debounce)
                { 
                    if (!power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button || !power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button) 
                    {
                        pairing_botton_detect_debounce = PAIRING_BOTTON_DETECT_DEBOUNCE;
                        if (power_state.status == EXTERN_POWER_CHARGING_FULL)
                        {
                            power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume = 1;
                        }
                        else
                        {
                            power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume = 1;
                        }
                        power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button = 0;
                        power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button = 0;
                        pf_log_raw(atel_log_ctl.core_en, "led_state_recovery_from_botton (%d:%d).\r",power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button,
                                                                                                     power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume); 
                        pf_log_raw(atel_log_ctl.core_en, "led_state_recovery_from_botton (%d:%d).\r",power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button,
                                                                                                     power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume);       
                    }
                }
            }
        }   
 
        else  //due to when device  pair fail,the led will work 5 mins. 
        {     //but the charger led priority indicate should lower the botton led.
            if (reset_info.ble_peripheral_paired)
            {
                // if (pairing_success_num)
                // {
                //     pairing_success_num--; // give more 10 second to let the led work complete
                //     if (!pairing_success_num)  //Warning: 10s delay deal with in function extern_charging_led_condition_check()
                //     {
                       // pairing_success_num = PAIRING_SUCCESS_DELAY_CNT;
                        if (power_state.status == EXTERN_POWER_CHARGING_FULL)
                        {
                            power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume = 1;
                        }
                        else
                        {
                            power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume = 1;
                        }
                        power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button = 0;
                        power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button = 0;  // only this way clear this flag.
                        power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button = 0;
                        power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button = 0;
                        pf_log_raw(atel_log_ctl.core_en, "led_state_recovery_from_pairing_botton (%d:%d).\r",power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button,
                                                                                                             power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume); 
                        pf_log_raw(atel_log_ctl.core_en, "led_state_recovery_from_pairing_botton (%d:%d).\r",power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button,
                                                                                                             power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume);
                   //  }
                // }
            }
            else
            {
                if (!monet_data.in_pairing_mode)
                {
                    if (power_state.status == EXTERN_POWER_CHARGING_FULL)
                    {
                        power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume = 1;
                    }
                    else
                    {
                        power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume = 1;
                    }
                    power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button = 0;
                    power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button = 0;  // only this way clear this flag.
                    power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button = 0;
                    power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button = 0;
                    pf_log_raw(atel_log_ctl.core_en, "led_state_recovery_from_Pairing_timeout_botton (%d:%d).\r",power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button,
                                                                                                                 power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume); 
                    pf_log_raw(atel_log_ctl.core_en, "led_state_recovery_from_Pairing_timeout_botton (%d:%d).\r",power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button,
                                                                                                                 power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume);
                   //  }
                }
            }
        }
    }
}

void turnOnCamera(void)
{
	clock_hfclk_request(); //ZAZUMCU-149::Need to manually switch to the external 
    pf_uart_init();
    pf_camera_pwr_init();
    if ((monet_data.SleepState == SLEEP_HIBERNATE) || (monet_data.cameraPowerAbnormal))
    {
        init_config();
        gpio_init();
        monet_data.SleepState = SLEEP_OFF;
        monet_data.SleepStateChange = 1;
    }

    monet_data.cameraPowerOnDelay = CAMERA_POWER_ON_DELAY_MS;
    if (monet_data.upgrading_mode_enter)
    {
        monet_data.upgrading_mode_enter = 0;
        monet_data.cameraPowerOnDelay = 0;
    }
    monet_data.cameraPowerOffDelay  = 0;
    monet_data.SleepAlarm       = 0; // Disable the sleep timer
    MCU_TurnOn_Camera();
    MCU_Wakeup_Camera();

    monet_Scommand();
}

void turnOffCameraDelay(uint32_t delay_ms)
{
    if (monet_data.cameraPowerOn == 1)
    {
        monet_data.cameraPowerOnDelay   = delay_ms ? delay_ms : CAMERA_POWER_ON_DELAY_MS;
        monet_data.cameraPowerOffDelay  = 0;
        monet_data.SleepAlarm           = 0; // Disable the sleep timer
    }
}

void turnOffCamera(void)
{
    monet_data.cameraPowerOnDelay = CAMERA_POWER_ON_DELAY_MS;
    monet_data.cameraPowerOffDelay = CAMERA_POWER_OFF_DELAY_MS;
    disableEventIReadyFlag();
}

void MCU_TurnOn_Camera(void)
{
    if (monet_data.cameraPowerOn == 0)
    {
        monet_data.cameraTurnOnTimes++;
    }
    
    pf_camera_pwr_ctrl(true);
    if (monet_data.firstPowerUp) {
        monet_data.firstPowerUp = 0;
    }

    monet_data.cameraPowerOn = 1;
    //monet_data.cameraTurnOnTimes++; //Record the turn on tiems
}

void MCU_TurnOff_Camera(void)
{
    if (monet_data.cameraPowerOn)
    {
        monet_data.cameraTurnOffTimes++;
    }
    monet_data.cameraofftime = count1sec;
    monet_data.cameraPowerOn = 0;

    if (monet_data.cameraPowerAbnormal == 0)
    {
        monet_data.SleepState = SLEEP_HIBERNATE;
        monet_data.SleepStateChange = 1;
    }

    monet_data.cameraPowerOnDelay = 0;
    monet_data.cameraPowerOffDelay  = 0;

    monet_data.led_flash_enable = 0;

    if (monet_data.uartTXDEnabled != 0)
    {
        // while (atel_io_queue_process()); // Sendout all data in queue.
        pf_uart_deinit();
    }
    clock_hfclk_release(); //ZAZUMCU-149::Need to manually switch to the external 32 MHz crystal

    pf_camera_pwr_ctrl(false);

    MCU_Sleep_Camera_App();

    pf_cfg_before_hibernation();
 
    // monet_data.cameraTurnOffTimes++;
}

void MCU_Wakeup_Camera(void)
{
    pf_gpio_write(GPIO_MCU_WAKE_DEV, 1);
    monet_gpio.Intstatus |= MASK_FOR_BIT(INT_WAKEUP);
}

void MCU_Sleep_Camera(void)
{
    pf_gpio_write(GPIO_MCU_WAKE_DEV, 0);
    monet_data.cameraofftime = count1sec;
}

void MCU_Wakeup_Camera_App(void)
{
    if ((monet_data.uartTXDEnabled == 0) && (monet_data.uartToBeInit == 0))
    {
        monet_data.uartToBeInit = 1;
        monet_data.uartTickCount = 0;
    }
}

void MCU_Sleep_Camera_App(void)
{
    configGPIO(GPIO_MCU_SLEEP_DEV, monet_conf.gConf[GPIO_MCU_SLEEP_DEV].status);
    configGPIO(GPIO_MCU_SLEEP_DEV1, monet_conf.gConf[GPIO_MCU_SLEEP_DEV1].status);

    monet_data.appActive = 0;
}

int8_t atel_io_queue_process(void)
{
    int8_t ret = 0;
    static uint8_t txq_resend = 0;
    static uint8_t txq_resend_value = 0;
    uint32_t again_count = 0;
    uint8_t tmp = 0;

    if ((monet_data.mcuDataHandleMs) && (monet_data.cameraPowerOn == 0))
    {
        ble_send_timer_start();
        
        if (is_ble_recv_queue_empty() == 0)
        {
            uint16_t recv_len = 0;
            uint8_t *p_data = NULL;
            monet_data.data_recv_channel = 0xffff;

            ble_recv_data_pop(&p_data, &recv_len, &monet_data.data_recv_channel);
            pf_log_raw(atel_log_ctl.core_en, "<<Receive nala 0xDD command2 \r");

            if (recv_len)
            {
            //     if (p_data[0] == 0xAA)
            //     {
            //         uint8_t buffer[2] = {0x04, 0x40};
            //         // BuildFrame(p_data[0], p_data + 1, recv_len - 1);

            //         // Handle 0xAA Command by MCU itself
            //         pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process 0xAA MS(%d)\r",
            //                    monet_data.mcuDataHandleMs);
            //         // Nala default TX power is 8dBm
            //         monet_data.ble_tx_power = 8;
            //         pf_ble_peer_tx_rssi_get(&monet_data.ble_rx_power);
            //         if (monet_data.ble_rx_power != 127)
            //         {
            //             monet_xAAcommand_nala_exception(buffer, 2);
            //         }
            //         monet_xAAcommand(p_data + 1, recv_len - 1);
            //         //ble_recv_data_delete_one();
            //     }
            //     if (p_data[0] == 0xDD)
            //     {
            //         pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process 0xDD MS(%d)\r",
            //                    monet_data.mcuDataHandleMs);
            //         monet_xDDcommand(p_data + 1, recv_len - 1);
            //         //ble_recv_data_delete_one();
            //     }
            // }
            // if (p_data != NULL)
            // {
            //     ble_recv_data_delete_one();
                if (p_data[0] == 0xAA)
                {
                    uint8_t buffer[2] = {0x04, 0x40};
                    // BuildFrame(p_data[0], p_data + 1, recv_len - 1);

                    // Handle 0xAA Command by MCU itself
                    pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process 0xAA MS(%d)\r",
                               monet_data.mcuDataHandleMs);
                    // Nala default TX power is 8dBm
                    monet_data.ble_tx_power = 8;
                    pf_ble_peer_tx_rssi_get(&monet_data.ble_rx_power);
                    if (monet_data.ble_rx_power != 127)
                    {
                        monet_xAAcommand_nala_exception(buffer, 2);
                    }
                    monet_xAAcommand(p_data + 1, recv_len - 1);
                    ble_recv_data_delete_one();
                }
                else
                {
                    if (p_data[0] == 0xDD)
                    {
                        pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process 0xDD MS(%d)\r",
                        monet_data.mcuDataHandleMs);
                        monet_xDDcommand(p_data + 1, recv_len - 1);
                        ble_recv_data_delete_one();
                    }
                    else
                    {
                        if ((monet_data.batPowerCritical == 1) && (monet_data.mainPowerValid == 0))  // only no main and critical release the queue room.
                        {
                            if (p_data != NULL)
                            {
                              //  ble_recv_data_delete_one();  // due to critical we should enter lowe power mode, don't need
                            }
                        }
                    }
                }      
            }
        }

        if (monet_data.mcuDataHandleMs <= 1000)
        {
            CRITICAL_REGION_ENTER();
            monet_data.mcuDataHandleMs = 0;
            CRITICAL_REGION_EXIT();

            if ((monet_data.cameraPowerOn == 0) && (monet_data.cameraPowerOnPending == 0))
            {
                ble_send_timer_stop();
                pf_ble_peer_tx_rssi_stop();
            }
        }
    }

RXQ_AGAIN:
    if (pf_uart_rx_one(&tmp) == 0)
    {
        GetRxCommandEsp(tmp);
        again_count++;
        if (again_count <= (MAX_COMMAND))
        {
            goto RXQ_AGAIN;
        }
    }

    again_count = 0;

    if (monet_data.uartTXDEnabled == 0)
    {
        return ret;
    }

TXQ_AGAIN:
    if ((ring_buffer_is_empty(&monet_data.txQueueU1) == 0) && monet_data.cameraPowerOn && monet_data.appActive)
    {
        uint8_t txdata = 0;
        uint32_t force_send_count = 0;

        camera_poweroff_delay_refresh();

        FORCE_SEND_AGAIN:
        if (txq_resend)
        {
            if (pf_uart_tx_one(txq_resend_value) == 0)
            {
                // pf_log_raw(atel_log_ctl.core_en, "atel_io_rtx(0x%x)\r", txq_resend_value);
                txq_resend = 0;
                txq_resend_value = 0;

                again_count++;
                if (again_count <= (MAX_COMMAND))
                {
                    goto TXQ_AGAIN;
                }
            }
        }
        else
        {
            ring_buffer_out(&monet_data.txQueueU1, &txdata);
            if (pf_uart_tx_one(txdata) == 0)
            {
                // pf_log_raw(atel_log_ctl.core_en, "atel_io_tx(0x%x)\r", txdata);
                again_count++;
                if (again_count <= (MAX_COMMAND))
                {
                    goto TXQ_AGAIN;
                }
            }
            else
            {
                txq_resend = 1;
                txq_resend_value = txdata;
            }
        }

        if (txq_resend)
        {
            force_send_count++;
            if (force_send_count < 3)
            {
                nrf_delay_ms(1);
                goto FORCE_SEND_AGAIN;
            }
        }

        ret = 1;
    }

    if (txq_resend)
    {
        pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process txq_resend(%d)\r", txq_resend);
        pf_uart_deinit();
        nrf_delay_ms(5);
        pf_uart_init();
    }

POP_AGAIN:
    if (monet_data.cameraPowerOn && monet_data.appActive)
    {
        if (ring_buffer_left_get(&monet_data.txQueueU1) > (2 * (BLE_DATA_RECV_BUFFER_CELL_LEN + 8)))
        {
            if (is_ble_recv_queue_empty() == 0)
            {
                uint16_t recv_len = 0;
                uint8_t *p_data = NULL;
                monet_data.data_recv_channel = 0xffff;

                ble_recv_data_pop(&p_data, &recv_len, &monet_data.data_recv_channel);
              //  pf_log_raw(atel_log_ctl.core_en, "<<Receive nala 0xDD command3  %x %d %d \r",p_data[0],p_data[1],p_data[2]);

                // pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process recv_len(%d)\r", recv_len);
                // printf_hex_and_char(p_data, recv_len);
                if (recv_len)
                {
                    if (p_data[0] == 0xDD)
                    {
                        pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process 0xDD\r");
                        monet_xDDcommand(p_data + 1, recv_len - 1);
                    }
                    else
                    {
                        if (p_data[0] == 0xAA)
                        {
                            // BuildFrame(p_data[0], p_data + 1, recv_len - 1);

                            uint8_t buffer[2] = {0x04, 0x40};
                            // Nala default TX power is 8dBm
                            monet_data.ble_tx_power = 8;
                            pf_ble_peer_tx_rssi_get(&monet_data.ble_rx_power);
                            if (monet_data.ble_rx_power != 127)
                            {
                                monet_xAAcommand_nala_exception(buffer, 2);
                            }

                            // Handle 0xAA Command by MCU itself
                            pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process 0xAA\r");
                            monet_xAAcommand(p_data + 1, recv_len - 1);
                        }
                        else
                        {
                            BuildFrame(IO_CMD_CHAR_BLE_RECV, p_data, recv_len);
                        }
                    }
                }

                if (p_data != NULL)
                {
                    ble_recv_data_delete_one();
                }

                goto POP_AGAIN;
            }
        }
    }

    #if SPI_CAN_CONTROLLER_EN
    if ((monet_data.cameraPowerOn && 
         monet_data.appActive && 
        (monet_data.CANExistChargerNoExist == 1) && 
        (monet_data.bleShouldAdvertise == 0)))
    {
        if (ring_buffer_left_get(&monet_data.txQueueU1) > (2 * (BLE_DATA_RECV_BUFFER_CELL_LEN + 8)))
        {
            can_rx_pool_queue_process();
        }
    }
    #endif /* SPI_CAN_CONTROLLER_EN */

    return ret;
}

// Data format:
// Length(byte):  1  1   1   LEN       1
// Content:      '$' LEN CMD data_body 0x0D
void GetRxCommand(uint8_t RXByte)
{
    static uint8_t parmCount = 0;

    switch(monet_data.iorxframe.state) {
    case IO_WAIT_FOR_DOLLAR:
        if(RXByte == '$') {
            monet_data.appActive = 1;
            monet_data.iorxframe.state = IO_GET_FRAME_LENGTH;
            if (monet_data.restart_indicate == 1)
            {
                monet_data.restart_indicate = 0; //zazumcu - 115
            }
        }
        break;
    case IO_GET_FRAME_LENGTH:
        #if CMD_CHECKSUM
		RXByte--;
        #endif /* CMD_CHECKSUM */
        if (RXByte <= MAX_COMMAND) {
            monet_data.iorxframe.length = RXByte;
            monet_data.iorxframe.remaining = RXByte;
            monet_data.iorxframe.state = IO_GET_COMMAND;
        } else {
            monet_data.iorxframe.state = IO_WAIT_FOR_DOLLAR;
        }
        break;
    case IO_GET_COMMAND:
        monet_data.iorxframe.cmd = RXByte;
        monet_data.iorxframe.state = IO_GET_CARRIAGE_RETURN;
        monet_data.iorxframe.checksum = RXByte;
        parmCount = 0;
        break;
    case IO_GET_CARRIAGE_RETURN:
        monet_data.iorxframe.checksum += RXByte;
        if (monet_data.iorxframe.remaining == 0) {
            #if CMD_CHECKSUM
			if (monet_data.iorxframe.checksum == 0xFF) {
            #else
			if (RXByte == 0x0D) {
            #endif /* CMD_CHECKSUM */
                HandleRxCommand();
            }
            monet_data.iorxframe.state = IO_WAIT_FOR_DOLLAR;
        } else {
            monet_data.iorxframe.remaining--;
            monet_data.iorxframe.data[parmCount++] = RXByte;
        }
        break;
    default:
        monet_data.iorxframe.state = IO_WAIT_FOR_DOLLAR;
        break;
    }
}

void GetRxCommandEsp(uint8_t RXByte)
{
#if CMD_ESCAPE
    // Ensure protocol is resynced if un-escaped $ is revc'd
    if (RXByte == '$')
    {
        // Clean up old data
        memset(&monet_data.iorxframe, 0, sizeof(monet_data.iorxframe));
        // Setup for new message
        monet_data.iorxframe.state = IO_WAIT_FOR_DOLLAR;
        monet_data.iorxframe.escape = IO_GET_NORMAL;
    }

    if (monet_data.iorxframe.state == IO_WAIT_FOR_DOLLAR) {
        GetRxCommand(RXByte);
        monet_data.iorxframe.escape = IO_GET_NORMAL;
        return;
    }

    switch (monet_data.iorxframe.escape) {
    case IO_GET_NORMAL:
        if (RXByte == '!')
        {
            monet_data.iorxframe.escape = IO_GET_ESCAPE;
        }
        else {
            GetRxCommand(RXByte);
        }
        break;
    case IO_GET_ESCAPE:
        monet_data.iorxframe.escape = IO_GET_NORMAL;
        if (RXByte == '0')
            GetRxCommand('$');
        else if (RXByte == '1')
            GetRxCommand('!');
        else {
            // Reset the state
            monet_data.iorxframe.state = IO_WAIT_FOR_DOLLAR;
        }
        break;
    }
#else
    GetRxCommand(RXByte);
#endif /* CMD_ESCAPE */
}

void atel_timerTickHandler(uint32_t tickUnit_ms)
{
    uint8_t i, bDoReport=0;
    BitStatus  s;
    uint8_t count = 0;
    uint16_t adc_bat = monet_data.AdcBackup;
    uint16_t adc_bat_raw = monet_data.AdcBackupRaw;

    if (gTimer)
    {
        gTimer--;
    }
    else
    {
        return;
    }

    pf_wdt_kick();

    pf_log_raw(atel_log_ctl.normal_en,"BUB(0x%x:%dMv) BATRaw(0x%x:%dMv) Baten(%d)\r",   //zazumcu-180 : add some logs
                                    adc_bat,
                                    (uint16_t)PF_ADC_RAW_TO_BATT_MV(adc_bat),
                                    adc_bat_raw,
                                    (uint16_t)PF_ADC_RAW_TO_BATT_MV(adc_bat_raw),
                                    pf_gpio_read(GPIO_BAT_ADC_EN));

    for(i=0; i < NUM_OF_GPIO; i++) {
        if ((monet_conf.gConf[i].status & GPIO_DIRECTION) == DIRECTION_OUT) { //including watchdog timer
            if (monet_gpio.counter[i]) {
                monet_gpio.counter[i]--;
                if (monet_gpio.counter[i]==0) {
                    // WARNING: this may cause GPIO pin status change
                    pf_gpio_toggle(i);
                    GPIO_SET(i, pf_gpio_read(i));
                    pf_log_raw(atel_log_ctl.core_en, "pf_gpio_toggle(index: %d value: %d)\r", i, GPIO_GET(i));
                }
            }
        } else { // Input
            if ((monet_conf.gConf[i].status & GPIO_IT_MODE) &&
                (i < MAX_EXT_GPIOS)) { // No need to read the internal GPIO pins
                if (monet_gpio.counter[i] || !monet_conf.gConf[i].Reload) {
                    if (monet_gpio.counter[i]) {
                        monet_gpio.counter[i]--;
                    }
                    if (!monet_gpio.counter[i] || !monet_conf.gConf[i].Reload) {
                        s = (BitStatus)(pf_gpio_read(i) > 0);
                        if (GPIO_GET(i) != s) {
                            GPIO_SET(i,s);
                            switch(monet_conf.gConf[i].status & GPIO_IT_MODE) {
                            case GPIO_IT_LOW_TO_HIGH:
                                if(s) {
                                    bDoReport = 1;
                                }
                                break;
                            case GPIO_IT_HIGH_TO_LOW:
                                if(!s) {
                                    bDoReport = 1;
                                }
                                break;
                            case GPIO_IT_BOTH:
                                bDoReport = 1;
                                break;
                            default:
                                break;
                            }
                            if (bDoReport) {
                                monet_gpio.Intstatus |= 1<<i;
                                bDoReport = 0;
                            }
                        }
                    }
                } else {
                    s = (BitStatus)(pf_gpio_read(i) > 0);
                    if (GPIO_GET(i) != s) {
                        monet_gpio.counter[i] = monet_conf.gConf[i].Reload;
                    }
                }
            } else {
                GPIO_SET(i, pf_gpio_read(i));
            }
        } //End of Input
    } //End of for loop

    check_valid_power_state(tickUnit_ms);

    gcount += tickUnit_ms;
    while (gcount >= TIME_UINT_SECOND_TIMER_PERIOD) {
        if (count == 0)
        {
            atel_timer_s(gcount / 1000);
        }
        if (gcount >= TIME_UINT_SECOND_TIMER_PERIOD)
        {
            gcount -= TIME_UINT_SECOND_TIMER_PERIOD;
        }
        count++;
    }

    atel_adc_converion(tickUnit_ms, 0);

    if (monet_data.CANExistChargerNoExist == 0)
    {
        charger_process_periodically(tickUnit_ms);
        solar_chg_mode_check_proc();
    }

    // atel_uart_restore();

    ble_connection_status_monitor(tickUnit_ms);

    camera_poweroff_delay_detect(tickUnit_ms);

    camera_power_abnormal_detect(tickUnit_ms);

    device_bootloader_enter_dealy(tickUnit_ms);
}

// This function should be called after charger chip detection
void atel_adc_converion(uint32_t delta, uint8_t force)
{
    static uint32_t adc_tick_ms = 0;

    if (force == 0)
    {
        adc_tick_ms += delta;

        if (adc_tick_ms >= DEVICE_ADC_SAMPLE_PERIOD_MS)
        {
            adc_tick_ms -= DEVICE_ADC_SAMPLE_PERIOD_MS;
        }
        else
        {
            return;
        }

        if (monet_data.CANExistChargerNoExist == 0)
        {
            // if (monet_data.cameraPowerOn == 0)
            {
                configGPIO(GPIO_BAT_ADC_EN, (uint8_t)PIN_STATUS(0, 1, 1, 0));
            }

            if (pf_saadc_ready() == 0)
            {
                pf_adc_ready_timer_start();
            }
            pf_gpio_write(GPIO_BAT_ADC_EN, (ADC_BUB_MEASURE_ENABLED == 0) ? 0 : 1);
        }
        else
        {
            goto SAADC_SAMPLE;
        }
    }
    else if (force == 1)
    {
        if (pf_saadc_ready() != 0)
        {
SAADC_SAMPLE:
            pf_adc_init();
            pf_adc_start(); // Trigger ADC sample
            pf_adc_poll_finish();
            pf_adc_deinit();

            pf_saadc_ready_clear();
            pf_gpio_write(GPIO_BAT_ADC_EN, (ADC_BUB_MEASURE_ENABLED == 0) ? 1 : 0);
            // pf_log_raw(atel_log_ctl.normal_en, "GPIO_BAT_ADC_EN(%d)\r", pf_gpio_read(GPIO_BAT_ADC_EN));
        }
    }
    else
    {
        goto SAADC_SAMPLE;
    }
}

// This function should be called after charger chip detection
void atel_adc_conv_force(void)
{
    uint16_t count = 0;

    atel_adc_converion(DEVICE_ADC_SAMPLE_PERIOD_MS, 0);

    while (pf_saadc_ready() == 0)
    {
        pf_delay_ms(1);
        count++;
        if (count > SAADC_READY_TIMER_PERIOD_MS)
        {
            break;
        }
    }

    atel_adc_converion(DEVICE_ADC_SAMPLE_PERIOD_MS, 1);
}

// Power event handle
// Param. pwr_event: power event. See @Power_event_selection for value selection
void power_event_handle(uint32_t pwr_event)
{
    if (pwr_event == PWR_EVENT_MAIN_PWR_PLUG_IN)
    {
        if (monet_data.CANExistChargerNoExist == 0)
        {
            solar_chg_mode_select(FUNC_JUMP_POINT_0);
        }
    }
    else if (pwr_event == PWR_EVENT_MAIN_PWR_GONE)
    {
        /* Reserved */;
        //0212 22  wfy add
        // when remove 12v,the led should back to off which use to indicate the charger status.
        pf_device_extern_power_charging_exit();
    }
}

void atel_timer_s(uint32_t seconds)
{
    const uint16_t delta = seconds;
    static uint32_t real_time_second = 0;
    uint16_t adc_bat = monet_data.AdcBackup;
    uint16_t adc_bat_raw = monet_data.AdcBackupRaw;
    uint16_t adc_vin = monet_data.AdcMain;
    uint16_t adc_vsin = monet_data.AdcSolar;
    data_time_table_t time_table = {0};

    count1sec += seconds;
    real_time_second += seconds;

    pf_log_raw(atel_log_ctl.normal_en, "timer_s: %d ", count1sec);
    time_table = SecondsToTimeTable(real_time_second);
    pf_log_raw(atel_log_ctl.normal_en,
              "(%d/%d/%d %02d:%02d:%02d) ",
                time_table.year,
                time_table.month,
                time_table.day,
                time_table.hour,
                time_table.minute,
                time_table.second);
    pf_log_raw(atel_log_ctl.normal_en, "Int(0x%x) Slp(%d) ",
               monet_gpio.Intstatus,
               monet_data.SleepState);
    pf_log_raw(atel_log_ctl.normal_en, "CamPwr(%d:%d) Act(%d) DEV_EN(%d) HID(0x%x)\r", 
               monet_data.cameraPowerOn,
               monet_data.cameraPowerOff,
               monet_data.appActive,
               pf_gpio_read(GPIO_V_DEV_EN),
               monet_data.cameraHID);
    pf_log_raw(atel_log_ctl.normal_en, "OnDelay(%ds) OffDelay(%ds) WDReload(%d) WD(%d) Paired(%d) ",
               monet_data.cameraPowerOnDelay / 1000,
               monet_data.cameraPowerOffDelay / 1000,
               monet_conf.WD.Reload,
               monet_gpio.WDtimer,
               reset_info.ble_peripheral_paired);
    pf_log_raw(atel_log_ctl.normal_en, "ResetShip(%d) AdvUntilPair(%d)\r",
               reset_info.reset_from_shipping_mode,
               reset_info.reset_from_shipping_mode_donot_adv_untill_pair);
    // pf_log_raw(atel_log_ctl.normal_en, " Min(%d) Max(%d) Late(%d) Out(%d)\r", 
    //                                     monet_data.ble_conn_param.min_100us,
    //                                     monet_data.ble_conn_param.max_100us,
    //                                     monet_data.ble_conn_param.latency,
    //                                     monet_data.ble_conn_param.timeout_100us);
    pf_log_raw(atel_log_ctl.normal_en, "PeerMac(%x:%x:%x:%x:%x:%x) ", 
                                        monet_data.ble_info[0].mac_addr[0],
                                        monet_data.ble_info[0].mac_addr[1],
                                        monet_data.ble_info[0].mac_addr[2],
                                        monet_data.ble_info[0].mac_addr[3],
                                        monet_data.ble_info[0].mac_addr[4],
                                        monet_data.ble_info[0].mac_addr[5]);
    pf_log_raw(atel_log_ctl.normal_en, "DT(%ds) BLEC_Num(%d) SADV(%d:%d:%d) PairMode(%d)\r", 
                                        monet_data.bleDisconnectDelayTimeMs / 1000,
                                        ble_connected_channel_num_get(),
                                        monet_data.bleShouldAdvertise, 
                                        monet_data.to_advertising_result, 
                                        monet_data.is_advertising,
                                        monet_data.in_pairing_mode);
    pf_log_raw(atel_log_ctl.normal_en, "Dic(%ds) Conn(%d) Quenum(%d) \r",
                                        monet_data.ble_disconnnet_delayTimeMs /1000,
                                        get_ble_conn_handle(),
                                        get_tx_queue_number());
    pf_log_raw(atel_log_ctl.normal_en, "MUX_CAN(%d) CAN_EXIST(%d) ", 
                                        monet_data.mux_can, 
                                        monet_data.CANExistChargerNoExist);
    pf_log_raw(atel_log_ctl.normal_en, "Ship Src(0x%02x) P(%d) M(%d)\r",
                                        device_shipping_mode.wakesrc,
                                        device_shipping_mode.pending,
                                        device_shipping_mode.mode);
    pf_log_raw(atel_log_ctl.normal_en, "BUB(0x%x:%dMv) BATRaw(0x%x:%dMv) ", 
                                        adc_bat,
                                        (uint16_t)PF_ADC_RAW_TO_BATT_MV(adc_bat),
                                        adc_bat_raw,
                                        (uint16_t)PF_ADC_RAW_TO_BATT_MV(adc_bat_raw));
    pf_log_raw(atel_log_ctl.normal_en, "VIN(0x%x:%dMv) VSIN(0x%x:%dMv) ",
                                        adc_vin,
                                        (uint16_t)PF_ADC_RAW_TO_VIN_MV(adc_vin),
                                        adc_vsin,
                                        (uint16_t)PF_ADC_RAW_TO_VSIN_MV(adc_vsin));
    pf_log_raw(atel_log_ctl.normal_en, "BUB Valid(%d) Critical(%d) VIN Valid(%d) PowerSave(%d) ", 
                                        monet_data.batPowerValid, 
                                        monet_data.batPowerCritical, 
                                        monet_data.mainPowerValid, 
                                        monet_data.batPowerSave);
    pf_log_raw(atel_log_ctl.normal_en, "BatThresh Critical(%dMv) Low(%dMv) ModeDisable (%d) \r", 
                                        monet_data.batCriticalThresh,
                                        monet_data.batLowThresh,
                                        monet_data.charge_mode_disable);
    pf_log_raw(atel_log_ctl.normal_en, "ChargeStatus(%d:%s) ChargeMode(%d) ChargeOn(%d) Mode2Disable (%d) Mode1Disable (%d) \r",
                                        monet_data.charge_status,
                                        charge_status_string[monet_data.charge_status],
                                        solar_chg_mode_get(),
                                        is_charger_power_on(),
                                        monet_data.charge_mode2_disable,
                                        monet_data.charge_mode1_disable);
    pf_log_raw(atel_log_ctl.normal_en,"TempNTC(%d) dC, TempNordic(%d) dC, Temp(%d) dC TempSet(%d) dc\r\n", 
					                   monet_data.temp_ntc, 
                                       monet_data.temp_nordic, 
                                       monet_data.temp,
                                       monet_data.temp_set);

    // if (monet_data.ldr_enable_status == 1)
    // {
    //     if (pf_gpio_read(GPIO_LIGTH_RESISTOR_INT) == ATEL_GPIO_PIN_RESET)
    //     {
    //         pf_log_raw(atel_log_ctl.normal_en, "LDR_Dark ");
    //     }
    //     else if (pf_gpio_read(GPIO_LIGTH_RESISTOR_INT) == ATEL_GPIO_PIN_SET)
    //     {
    //         pf_log_raw(atel_log_ctl.normal_en, "LDR_Light ");
    //     }
    // }

    // Enable the default 25hr (by default) watchdog timer
    // if (reset_info.ble_peripheral_paired == 1)
    {
        if (monet_gpio.WDtimer > delta)
        {
            monet_gpio.WDtimer -= delta;
        }
        else
        {
            monet_gpio.WDtimer = 0;

            pf_log_raw(atel_log_ctl.core_en, "Watchdog Timeout.\r");
            MCU_TurnOff_Camera();
            systemreset(monet_gpio.WDflag);
            // Don't allow timer to be reset until modem is powered off
            monet_gpio.WDtimer = 180;
            monet_gpio.WDflag = 0;
        }
    }
    // else // Enable the default 25hr (by default) watchdog timer
    // {
    //     monet_gpio.WDtimer = monet_conf.WD.Reload;
    // }

    uint16_t AdcMain_mV = (uint16_t)PF_ADC_RAW_TO_VIN_MV(adc_vin);
    // We check if we have valid power
    if (AdcMain_mV >= MAIN_ADC_MAIN_VALID)
    {
        if (monet_data.MainState == MAIN_GONE)
        {
            pf_log_raw(atel_log_ctl.core_en, "main pwr restored.\r");
            power_event_handle(PWR_EVENT_MAIN_PWR_PLUG_IN);

#if DEVICE_BUTTON_FOR_FACTORY
            if (device_shipping_mode.mode == 0)
            {
                pf_device_button_for_factory_enter();
            }
#endif /* DEVICE_BUTTON_FOR_FACTORY */

        }
        monet_data.MainState = MAIN_NORMAL;
    }
    else if ((AdcMain_mV <= MAIN_ADC_MAIN_OFF) &&
             (monet_data.MainState != MAIN_GONE))
    {
        pf_log_raw(atel_log_ctl.core_en, "main pwr gone.\r");
        monet_data.MainState = MAIN_GONE;
        power_event_handle(PWR_EVENT_MAIN_PWR_GONE);
    }

    // BleConnectionHeartBeat(delta);

    if (monet_data.in_pairing_mode)
    {
        CRITICAL_REGION_ENTER();
        monet_data.in_pairing_mode_time += (seconds * 1000);
        CRITICAL_REGION_EXIT();
        pf_log_raw(atel_log_ctl.core_en, "device in pairing mode(time: %d ms)\r", monet_data.in_pairing_mode_time);
        if (monet_data.in_pairing_mode_time >= BLE_IN_PAIRING_MODE_TIMEOUT_MS)
        {
            pf_log_raw(atel_log_ctl.core_en, "pf_device_pairing_fail(timeout: %d ms)\r", monet_data.in_pairing_mode_time);

            pf_ble_adv_connection_reset();
            CRITICAL_REGION_ENTER();
            monet_data.to_advertising = 1;
            monet_data.in_pairing_mode = 0;
            monet_data.in_pairing_mode_time = 0;
            monet_data.pairing_success = 0;
            CRITICAL_REGION_EXIT();

            pf_device_pairing_fail();
        }
    }

    // This feature is for indicating upgrading mode with LED, not used for now
    // if (monet_data.in_upgrading_mode)
    // {
    //     monet_data.in_upgrading_mode_time += (seconds * 1000);
    //     pf_log_raw(atel_log_ctl.core_en, "device in upgrading mode(time: %d ms)\r", monet_data.in_upgrading_mode_time);
    //     if (monet_data.in_upgrading_mode_time >= CAMERA_UPGRADING_MODE_TIMEOUT_MS)
    //     {
    //         pf_device_upgrading_fail();
    //         pf_log_raw(atel_log_ctl.core_en, "pf_device_upgrading_fail(timeout: %d ms)\r", monet_data.in_upgrading_mode_time);
    //     }
    // }

    if (monet_data.to_advertising_recover)
    {
        pf_log_raw(atel_log_ctl.core_en, "to_advertising_recover(mode: %d)\r", monet_data.in_pairing_mode);

        monet_data.to_advertising_recover = 0;

        pf_ble_adv_connection_reset();

        monet_data.to_advertising = 1;
        // if (monet_data.to_advertising_without_white_list == 1)
        // {
        //     monet_data.to_advertising_without_white_list = 0;
        //     pf_ble_adv_start_without_whitelist();
        // }
        // else
        // {
        //     pf_ble_adv_start();
        // }

    }

    device_shipping_mode_check();
    device_shipping_auto_check();
    device_low_power_mode_check(1);
    if (exit_from_lower_power_mode == true)
    {
        InitApp(1);
    }
    device_sleeping_recovery_botton_check();
}

// in_out: in 1 out 0
void device_in_out_shipping_mode_indicate(uint8_t in_out)
{
    nrf_gpio_cfg_output(P022_BLE_ESP_EN);
    nrf_gpio_pin_clear(P022_BLE_ESP_EN);
    pf_delay_ms(50);
    nrf_gpio_cfg_output(P106_DCDC_3V3_EN);
    nrf_gpio_pin_clear(P106_DCDC_3V3_EN);
    nrf_gpio_cfg_output(P105_RED_LED1);
    nrf_gpio_pin_set(P105_RED_LED1);
    nrf_gpio_cfg_output(P023_GREEN_LED1);
    if (in_out == 0)
    {
        nrf_gpio_pin_set(P023_GREEN_LED1);
    }
    pf_delay_ms(3000);
    nrf_gpio_pin_clear(P105_RED_LED1);
    nrf_gpio_pin_clear(P023_GREEN_LED1);
}

void device_shipping_mode_check(void)
{
    static uint32_t systick_count_old = 0;

    if ((monet_data.cameraPowerOn == 1) || (device_shipping_mode.pending == 0))
    {
        return;
    }
    pf_log_raw(atel_log_ctl.core_en,"<<<Force enter shipping mode \r");

    if (monet_data.mainPowerValid == 1)
    {
        device_shipping_mode.wakesrc |= DEV_SHIP_WAKE(MAIN_POWER_VALID);
    }
    else
    {
        device_shipping_mode.wakesrc |= DEV_SHIP_WAKE(MAIN_POWER_INVALID);
    }

    device_shipping_mode.pending = 0;
    device_shipping_mode.mode = 1;
    monet_data.device_in_shipping_mode = 1;
 
    //led_state_recovery_from_botton();
        
    // TODO
    // Stop Adv
    // Stop all timers
    // Turn off charger
    // Deinit I2C, SPI, UART ...
    // Kick Watchdog
    // Call Idle
    // Change Systick Timer Period
    // Check ADC Value, do not enable BUB ADC EN
    // Print Debug Info

    pf_ble_adv_connection_reset();

    pf_systick_stop();
    timer_solar_chg_mode_stop();
    app_timer_stop_all();

    if (monet_data.CANExistChargerNoExist == 0)
    {
        setChargerOff();
        solar_chg_mode_set(SOLAR_CHG_MODE1); 
        setChargerOn1();
        setChargerOff();
        pf_log_raw(atel_log_ctl.core_en,"<<<Turn off charger with canExit \r");
    }

    if (monet_data.CANExistChargerNoExist == 1)
    {
        pf_spi_uninit();
    }

    pf_i2c_0_uninit();

    pf_uart_deinit();

    pf_wdt_kick();

    device_in_out_shipping_mode_indicate(1);

    pf_cfg_before_shipping();

    pf_systick_shippping_start();

    while (1)
    {
        pf_device_enter_idle_state();

        pf_wdt_kick();

        if (systick_count_old != device_shipping_mode.count)
        {
            systick_count_old = device_shipping_mode.count;

            atel_adc_converion(0, 2);

            pf_log_raw(atel_log_ctl.core_en,
                       "Shipping Mode: Count(%d) Src(0x%02x)\r",
                       device_shipping_mode.count,
                       device_shipping_mode.wakesrc);

            if ((uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain) > MAIN_ADC_MAIN_VALID)
            {
                if (DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_PREWAKE))
                {
                    device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_WAKE);
                }
                else if (DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_INVALID))
                {
                    device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_WAKE);
                }
            }
            else
            {
                device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_PREWAKE);
            }
        }

        device_button_pattern_process();

        if ((DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_WAKE)) || 
            (DEV_SHIP_WAKE_KEY_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(KEY_VALID)))
        {

            pf_log_raw(atel_log_ctl.core_en,
                       "Exit Shipping Mode: Count(%d) Src(0x%02x)\r",
                       device_shipping_mode.count,
                       device_shipping_mode.wakesrc);
            device_in_out_shipping_mode_indicate(0);
            monet_data.device_in_shipping_mode = 0;
            DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_SHIPPING_MODE;		
            DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
            //pf_log_raw(atel_log_ctl.core_en," 0x%08x Reset Camera Power Off\r",DFU_FLAG_REGISTER1);
			pf_sys_shutdown();
        }
    }
}


#define SHIPPING_ENTER_DELAY_CNT    (5)
void device_shipping_auto_check(void)
{
    static uint32_t systick_count_old = 0;
    static uint32_t shipping_mode_enter_debounce = SHIPPING_ENTER_DELAY_CNT;
    pf_log_raw(atel_log_ctl.core_en,"<<<ResetShipping(%d) : MainPower(%d) : NotAdvUntillPair(%d) : BlePaired(%d) : CameraPower(%d) :InPairing(%d) \r",
                                    reset_info.reset_from_shipping_mode,monet_data.mainPowerValid,reset_info.reset_from_shipping_mode_donot_adv_untill_pair,
                                    reset_info.ble_peripheral_paired,monet_data.cameraPowerOn,monet_data.in_pairing_mode);

    //No main power, Unpair,No pair, esp32 off,    //penging remain orginal command.                                          //when before reset from shipping mode, the reset_from_shipping_mode_donot_adv_untill_pair flag is 0,
    if (((reset_info.reset_from_shipping_mode == 1) && (reset_info.reset_from_shipping_mode_donot_adv_untill_pair == 0)) ||   // here need 2 flag to avoid some other case so the device enter shipping mode normally 
       (monet_data.mainPowerValid == 1) || (reset_info.ble_peripheral_paired == 1) || (monet_data.cameraPowerOn == 1) ||      // for factory,the AT+Sleep=2 do not enter
       (monet_data.in_pairing_mode == 1))                                                                   // zazumcu -110 in pairing do not enter shipping mde 
    {
        shipping_mode_enter_debounce = SHIPPING_ENTER_DELAY_CNT;
        return;
    }
    if (shipping_mode_enter_debounce) //make sure the device condition really fits enter shipping mode.
    {
        shipping_mode_enter_debounce--;
        pf_log_raw(atel_log_ctl.core_en,"<<<Auto enter shipping mode debounce (%d) \r",shipping_mode_enter_debounce);
        if (!shipping_mode_enter_debounce)
        {
            shipping_mode_enter_debounce = SHIPPING_ENTER_DELAY_CNT;
            pf_log_raw(atel_log_ctl.core_en,"<<<Auto enter shipping mode \r");
            // if ((monet_data.cameraPowerOn == 1) || (device_shipping_mode.pending == 0))
            // {
            //     return;
            // }
            // warnig:main power will invalid.
            if (monet_data.mainPowerValid == 1)  
            {
                device_shipping_mode.wakesrc |= DEV_SHIP_WAKE(MAIN_POWER_VALID);
            }
            else
            {
                device_shipping_mode.wakesrc |= DEV_SHIP_WAKE(MAIN_POWER_INVALID);
            }

            device_shipping_mode.pending = 0;
            device_shipping_mode.mode = 1;   
            monet_data.device_in_shipping_mode = 1;
         
            //led_state_recovery_from_botton();
            
            // TODO
            // Stop Adv
            // Stop all timers
            // Turn off charger
            // Deinit I2C, SPI, UART ...
            // Kick Watchdog
            // Call Idle
            // Change Systick Timer Period
            // Check ADC Value, do not enable BUB ADC EN
            // Print Debug Info

            pf_ble_adv_connection_reset();

            pf_systick_shipping_stop();
            pf_systick_stop();
            timer_solar_chg_mode_stop();
            app_timer_stop_all();

            if (monet_data.CANExistChargerNoExist == 0)
            {
                setChargerOff();
                solar_chg_mode_set(SOLAR_CHG_MODE1); 
                setChargerOn1();
                setChargerOff();
                pf_log_raw(atel_log_ctl.core_en,"<<<Turn off charger with canExit \r");
            }

            if (monet_data.CANExistChargerNoExist == 1)
            {
                pf_spi_uninit();
            }

            pf_i2c_0_uninit();

            pf_uart_deinit();

            pf_wdt_kick();

            device_in_out_shipping_mode_indicate(1);

            pf_cfg_before_shipping();

            pf_systick_shippping_start();

            while (1)
            {
                pf_device_enter_idle_state();

                pf_wdt_kick();

                if (systick_count_old != device_shipping_mode.count)
                {
                    systick_count_old = device_shipping_mode.count;

                    atel_adc_converion(0, 2);

                    pf_log_raw(atel_log_ctl.core_en,
                                "Shipping Mode: Count(%d) Src(0x%02x)\r",
                                device_shipping_mode.count,
                                device_shipping_mode.wakesrc);

                    if ((uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain) > MAIN_ADC_MAIN_VALID)
                    {
                        if (DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_PREWAKE))
                        {
                            device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_WAKE);
                        }
                        else if (DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_INVALID))
                        {
                            device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_WAKE);
                        }
                    }
                    else
                    {
                        device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_PREWAKE);
                    }
                }

                device_button_pattern_process();

                if ((DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_WAKE)) || 
                    (DEV_SHIP_WAKE_KEY_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(KEY_VALID)))
                {

                    pf_log_raw(atel_log_ctl.core_en,
                                "Exit Shipping Mode: Count(%d) Src(0x%02x)\r",
                                device_shipping_mode.count,
                                device_shipping_mode.wakesrc);
                    device_in_out_shipping_mode_indicate(0);
                    monet_data.device_in_shipping_mode = 0;
                    DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_SHIPPING_MODE;
                    DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
                    //pf_log_raw(atel_log_ctl.core_en," 0x%08x Reset Camera Power Off\r",DFU_FLAG_REGISTER1);
                    pf_sys_shutdown();
                }
            } 
        }
    }  
}

#define LOW_POWER_DELAY_CNT (2u)

void device_low_power_mode_check(uint8_t first_run)
{
    static uint32_t systick_cnt_old = 0;
    static uint32_t low_power_mode_enter_debounce = LOW_POWER_DELAY_CNT;
    static uint8_t  lower_mode_mode1_enable = 0;
    static uint8_t  lower_mode_mode1_enable1 = 0;
    static uint8_t  lower_mode_mode2_enable = 0;

    nrf_gpio_cfg_output(P003_ADC_EN);
    nrf_gpio_pin_set(P003_ADC_EN);
    nrf_delay_ms(30);
    atel_adc_converion(0, 2);
 
    if (((uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain) > MAIN_ADC_MAIN_VALID)  && (first_run == 0))
    {
        monet_data.mainPowerValid = 1;
    }
    else if(((uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain) < MAIN_ADC_MAIN_VALID) && first_run == 0)
    {
        monet_data.mainPowerValid = 0;
    }

    if ((uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackupRaw) < monet_data.batCriticalThresh && (first_run == 0) && (monet_data.mainPowerValid == 0))  // enter this mode
    {
        pf_log_raw(atel_log_ctl.core_en,"<<<Auto enter low power mode (%d) \r",PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackupRaw));
        monet_data.batPowerCritical = 1;
    }

    pf_log_raw(atel_log_ctl.core_en,"BatCritical: (%d) MainValid: (%d) Temp: (%d) BatCriThres (%d) BleHandler: (%d) CameraPending: (%d)\r",
                                    monet_data.batPowerCritical,monet_data.mainPowerValid,monet_data.temp_nordic,
                                    monet_data.batCriticalThresh,monet_data.mcuDataHandleMs,monet_data.cameraPowerOnPending);
    
    if ((monet_data.batPowerCritical == 0) || (monet_data.mainPowerValid == 1) || (monet_data.cameraPowerOnPending == 1) ||
        (monet_data.cameraPowerOn == 1) || ((monet_data.cameraPowerOn == 0) && (monet_data.mcuDataHandleMs != 0)))         // esp32 alive, main exist, no critical, pending, send queue data                                                         
    {
        low_power_mode_enter_debounce = LOW_POWER_DELAY_CNT;
        exit_from_lower_power_mode = false;
        return;
    }
    if (low_power_mode_enter_debounce || (first_run == 0)) //make sure the device condition really fits enter low power mode.
    {
        low_power_mode_enter_debounce--;
        exit_from_lower_power_mode = false;
        pf_log_raw(atel_log_ctl.core_en,"<<<Auto enter low power mode debounce (%d) \r",low_power_mode_enter_debounce);
       if (!low_power_mode_enter_debounce || (first_run == 0))
        {
            // low_power_mode_fisrt_run1 = false;
            low_power_mode_enter_debounce = LOW_POWER_DELAY_CNT;
            pf_log_raw(atel_log_ctl.core_en,"<<<Auto enter low power mode. Venter:(%d) \r",monet_data.batCriticalThresh);

            if (monet_data.mainPowerValid == 1)  
            {
                device_shipping_mode.wakesrc |= DEV_SHIP_WAKE(MAIN_POWER_VALID);
            }
            else
            {
                device_shipping_mode.wakesrc |= DEV_SHIP_WAKE(MAIN_POWER_INVALID);
            }

            // device_shipping_mode.pending = 0;
            // device_shipping_mode.mode = 1;   
            // monet_data.device_in_shipping_mode = 1;
         
            //led_state_recovery_from_botton();
            
            // TODO
            // Stop Adv
            // Stop all timers
            // Turn off charger
            // Deinit I2C, SPI, UART ...
            // Kick Watchdog
            // Call Idle
            // Change Systick Timer Period
            // Check ADC Value, do not enable BUB ADC EN
            // Print Debug Info

            monet_data.to_advertising = 0;
            monet_data.to_advertising_recover = 0;
            pf_ble_adv_connection_reset();

            pf_systick_shipping_stop();
            pf_systick_stop();
            timer_solar_chg_mode_stop();
            app_timer_stop_all();

            if (monet_data.CANExistChargerNoExist == 0)
            {
                // setChargerOff();
                // solar_chg_mode_set(SOLAR_CHG_MODE1); 
                // nrf_delay_ms(1000);
                // setChargerOff();
                pf_log_raw(atel_log_ctl.core_en,"<<<Turn off charger with canExit \r");
            }

            if (monet_data.CANExistChargerNoExist == 1)
            {
                pf_spi_uninit();
            }

            if (monet_data.cameraPowerOn)
            {
                pf_log_raw(atel_log_ctl.core_en,"<<<Turn off camera \r");
                ble_send_timer_stop();
                MCU_TurnOff_Camera();
                pf_ble_peer_tx_rssi_stop();
            }
            
            mppt_process_nml_deinit();

            pf_i2c_0_uninit();

            pf_uart_deinit();

            pf_wdt_kick();

            //device_in_out_shipping_mode_indicate(1);

            pf_cfg_before_shipping();
            pf_systick_shippping_start();
            
           while (1)
            {
               pf_device_enter_idle_state();

                pf_wdt_kick();

                if (systick_cnt_old != device_shipping_mode.count)  ///
                {

                    nrf_gpio_cfg_output(P003_ADC_EN);
                    nrf_gpio_pin_set(P003_ADC_EN);
                    nrf_delay_ms(10);
                    systick_cnt_old = device_shipping_mode.count;

                    atel_adc_converion(0, 2);
                    for (uint8_t i = 0; i < 30; i++)
                    {
                        pf_adc_poll_finish();
                    }

               
                    pf_log_raw(atel_log_ctl.core_en,
                                "Lower Power Mode: Count(%d) Src(0x%02x)\r",
                                device_shipping_mode.count,
                                device_shipping_mode.wakesrc);

                        if (((uint16_t)PF_ADC_RAW_TO_VSIN_MV(monet_data.AdcSolar) > VOL_LIMIT_S_CHG_MODE_SEL))
                        {
                            monet_data.temp_nordic = chip_temp_get();	// Read Nordic buildin temperature to make current software compatible with old board
                            if (((uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup)) < BUB_THRESHOLD_CHANGE_TEMP)
                            {
                                monet_data.temp_set = CHG_TEMP_LIMIT5;
                            }
                            else
                            {
                                monet_data.temp_set = CHG_TEMP_LIMIT4;
                            }

                            // {
                            //     monet_data.temp_set = CHG_TEMP_LIMIT5;
                            // }
                           
                            if ((monet_data.temp_nordic > monet_data.temp_set) || (monet_data.temp_nordic <  CHG_TEMP_LIMIT1))
                            {
                                if (lower_mode_mode1_enable == 0)
                                {
                                    solar_chg_mode_set(SOLAR_CHG_MODE1);
                                    setChargerOn1();
                                    setChargerOff();
                                    nrf_gpio_cfg_output(P021_CHRG_SLEEP_EN);
                                    nrf_gpio_pin_clear(P021_CHRG_SLEEP_EN);
                                    lower_mode_mode1_enable = 1;
                                    lower_mode_mode2_enable = 0;
                                    lower_mode_mode1_enable1 = 0;
                                    pf_log_raw(atel_log_ctl.core_en,"<<<set to mode1. \r");
                                }
                            }
                            else
                            {
                                if (lower_mode_mode2_enable == 0)
                                {
                                    solar_chg_mode_set(SOLAR_CHG_MODE2);
                                    //timer_solar_chg_mode_restart();
                                    monet_data.charge_status = CHARGE_STATUS_SOLAR_MODE_2;
                                    lower_mode_mode2_enable = 1;
                                    lower_mode_mode1_enable = 0;
                                    lower_mode_mode1_enable1 = 0;
                                    pf_log_raw(atel_log_ctl.core_en,"<<<set to mode2 \r");

                                }
                            }
                            pf_log_raw(atel_log_ctl.core_en,"<<<Read solar (%d) Temp (%d) \r",PF_ADC_RAW_TO_VIN_MV(monet_data.AdcSolar),monet_data.temp_nordic);
                        }
                        else
                        {
                            if (lower_mode_mode1_enable1 == 0)
                            {
                                solar_chg_mode_set(SOLAR_CHG_MODE1);
                                setChargerOn1();
                                setChargerOff();
                                nrf_gpio_cfg_output(P021_CHRG_SLEEP_EN);
                                nrf_gpio_pin_clear(P021_CHRG_SLEEP_EN);
                                pf_log_raw(atel_log_ctl.core_en,"<<<set to mode1 \r");
                                lower_mode_mode1_enable1 = 1;
                                lower_mode_mode2_enable = 0;
                                lower_mode_mode1_enable = 0;                                
                            }
                        }
               
                    if ((uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain) > MAIN_ADC_MAIN_VALID)   // main power do not stay in this state.
                    {
                        if (DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_PREWAKE))
                        {
                            device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_WAKE);
                        }
                        else if (DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_INVALID))
                        {
                            device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_WAKE);
                        }
                        pf_log_raw(atel_log_ctl.core_en,"MAIN POWER VALID \r");
                    }
                    else
                    {
                        device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_PREWAKE);
                        pf_log_raw(atel_log_ctl.core_en,"MAIN POWER INVALID \r");
                    }

                    pf_log_raw(atel_log_ctl.core_en,"Low Power Mode Bat:(%d) \r",PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup));
                    //if ((uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcBackup) > BATTERY_ADC_AVTIVE)  // exit this mode
                    if (((uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup) > (monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_FIRST_DELTA)) && (first_run == 0)) //
                    {
                        pf_log_raw(atel_log_ctl.core_en,"Exit Low Power Mode Bat:(%d)  Vexist(%d) \r",PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackupRaw),(monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_FIRST_DELTA));
                        device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_PREWAKE);
                        exit_from_lower_power_mode = true;
                        lower_mode_mode1_enable = 0;
                        lower_mode_mode2_enable = 0;
                        lower_mode_mode1_enable1 = 0;
                        //pf_sys_shutdown();
                        return;
                    }
                    else
                    {
                        if (((uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup) > (monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_DELTA))) //
                        {
                            pf_log_raw(atel_log_ctl.core_en,"1Exit Low Power Mode Bat:(%d)  Vexist(%d) \r",PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackupRaw),(monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_DELTA));
                            device_shipping_mode.wakesrc = DEV_SHIP_WAKE(MAIN_POWER_PREWAKE);
                            exit_from_lower_power_mode = true;
                            lower_mode_mode1_enable = 0;
                            lower_mode_mode2_enable = 0;
                            lower_mode_mode1_enable1 = 0;
                            //pf_sys_shutdown();
                            return;
                        }
                    }
                }

                //device_button_pattern_process();

                // if ((DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_WAKE)) || 
                //     (DEV_SHIP_WAKE_KEY_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(KEY_VALID)))
                if ((DEV_SHIP_WAKE_POWER_SRC_GET(device_shipping_mode.wakesrc) == DEV_SHIP_WAKE(MAIN_POWER_WAKE)))
                {

                    pf_log_raw(atel_log_ctl.core_en,
                                "2Exit Lower Power Mode, Main(%d) Count(%d) Src(0x%02x) \r",
                                (uint16_t)PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain),
                                device_shipping_mode.count,
                                device_shipping_mode.wakesrc);
                    exit_from_lower_power_mode = true;
                    lower_mode_mode1_enable = 0;
                    lower_mode_mode2_enable = 0;
                    lower_mode_mode1_enable1 = 0;

                    return;
                }
            } 
        }
    }  
}





#define SLEEP_RECOVERY_BOTTON_DEBOUNCE  (1)
//0212 22  wfy add
void device_sleeping_recovery_botton_check(void)
{
    static uint8_t sleep_debounce_time = SLEEP_RECOVERY_BOTTON_DEBOUNCE;
    pf_log_raw(atel_log_ctl.core_en,"device_sleeping_recovery_botton_check appActive(%d) \r",monet_data.appActive);
                                
    if (monet_data.appActive != 1 || power_state.status == EXTERN_POWER_CHARGING_FULL)  // device is sleep. note:sleep not power on.
    {
        if (sleep_debounce_time)
        {
            sleep_debounce_time--;
            if (sleep_debounce_time == 0)
            {
                sleep_debounce_time = SLEEP_RECOVERY_BOTTON_DEBOUNCE;
                led_state_recovery_from_botton();
            }
        }
        
    }
}


void clock_hfclk_release(void)  //ZAZUMCU-149
{
    uint16_t count = 0;
    NRF_LOG_RAW_INFO("clock_hfclk_release hfclk_is_running: %d\r", nrf_drv_clock_hfclk_is_running());
    NRF_LOG_FLUSH();

    if(nrf_drv_clock_hfclk_is_running())
    {
    	nrf_drv_clock_hfclk_release();
        while (nrf_drv_clock_hfclk_is_running())
        {
            pf_delay_ms(1);
            count++;
            if (count > 1000)
            {
                return;
            }
        }
    }
}

void clock_hfclk_request(void) //ZAZUMCU-149::Need to manually switch to the external 32 MHz crystal
{
    uint16_t count = 0;	
    if(!nrf_drv_clock_hfclk_is_running())
    {
	    NRF_LOG_RAW_INFO("clock_hfclk_request hfclk_is_running: %d\r", nrf_drv_clock_hfclk_is_running());
    	NRF_LOG_FLUSH();
        nrf_drv_clock_hfclk_request(NULL);
        while (!nrf_drv_clock_hfclk_is_running())
        {
            pf_delay_ms(1);
            count++;
            if (count > 1000)
            {
                return;
            }
        }
    }
}


void check_valid_power_state(uint32_t ms)
{
    uint16_t adc_bat = monet_data.AdcBackup;
    uint16_t adc_vin = monet_data.AdcMain;
    uint16_t adc_vsin = monet_data.AdcSolar;

    adc_bat = (uint16_t)PF_ADC_RAW_TO_BATT_MV(adc_bat);
    adc_vin = (uint16_t)PF_ADC_RAW_TO_VIN_MV(adc_vin);
    adc_vsin = (uint16_t)PF_ADC_RAW_TO_VSIN_MV(adc_vsin);

    if (adc_bat <= CHARGING_MODE2_DISABLE_LOW_THRESHOLD)
    {
        monet_data.charge_mode2_disable = 0;
    }
    else if (adc_bat >= CHARGING_MODE2_DISABLE_HIGH_THRESHOLD)
    {
        monet_data.charge_mode2_disable = 1;
    }
    // if (adc_bat <= monet_data.batCriticalThresh)
    // {
    //     monet_data.charge_mode1_disable = 1;
    // }
    // else if (adc_bat >= monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_DELTA)
    // {
    //     monet_data.charge_mode1_disable = 0;
    // }

    if (adc_bat < monet_data.batCriticalThresh)
    {
        if (monet_data.debounceBatCriticalMs > ms)
        {
            monet_data.debounceBatCriticalMs -= ms;
        }
        else
        {
            monet_data.debounceBatCriticalMs = 0;
            if (monet_data.batPowerCritical == 0)
            {
                monet_data.batPowerCritical = 1;
                monet_data.charge_mode1_disable = 1;
                pf_log_raw(atel_log_ctl.normal_en, "Critical Bat(%d Mv) Th(%d Mv) Deb(%d Ms)\r",
                           adc_bat,
                           monet_data.batCriticalThresh,
                           BUB_CRITICAL_DEBOUNCE_MS);
            }
        }
    }
    else
    {
        monet_data.debounceBatCriticalMs = BUB_CRITICAL_DEBOUNCE_MS;
        //monet_data.batPowerCritical = 0;
    }



    if (adc_bat > monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_DELTA)
    {
        if (monet_data.debounceBatActiveMs > ms)
        {
            monet_data.debounceBatActiveMs -= ms;
        }
        else
        {
            monet_data.debounceBatActiveMs = 0;
            if (monet_data.batPowerCritical == 1)
            {
                monet_data.batPowerCritical = 0;
                monet_data.charge_mode1_disable = 0;
                pf_log_raw(atel_log_ctl.normal_en, "Active Bat(%d Mv) Th(%d Mv) Deb(%d Ms)\r",
                           adc_bat,
                           monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_DELTA,
                           BUB_CRITICAL_DEBOUNCE_MS);
            }
        }
    }
    else
    {
        monet_data.debounceBatActiveMs = BUB_CRITICAL_DEBOUNCE_MS;
        //monet_data.batPowerActive = 0;
    }


    //if (adc_bat >= monet_data.batLowThresh)
    if (adc_bat >= monet_data.batCriticalThresh + BUB_CRITICAL_THRESHOLD_DELTA) //0712
    {
        if (monet_data.debounceBatValidMs > ms)
        {
            monet_data.debounceBatValidMs -= ms;
        }
        else
        {
            monet_data.debounceBatValidMs = 0;
            if (monet_data.batPowerValid == 0)
            {
                monet_data.batPowerValid = 1;
                pf_log_raw(atel_log_ctl.normal_en, "ValidPower Bat(%d Mv) Th(%d Mv) Deb(%d Ms)\r",
                           adc_bat,
                           monet_data.batLowThresh,
                           BUB_VALID_DEBOUNCE_MS);
            }
        }
    }
    else
    {
        monet_data.debounceBatValidMs = BUB_VALID_DEBOUNCE_MS;
        monet_data.batPowerValid = 0;
    }

    if (adc_vin >= MAIN_ADC_MAIN_VALID)
    {
        if (monet_data.debounceMainValidMs > ms)
        {
            monet_data.debounceMainValidMs -= ms;
        }
        else
        {
            monet_data.debounceMainValidMs = 0;
            if (monet_data.mainPowerValid == 0)
            {
                monet_data.mainPowerValid = 1;
                pf_log_raw(atel_log_ctl.normal_en, "ValidPower Main(%d Mv) Th(%d Mv) Deb(%d Ms)\r",
                           adc_vin,
                           MAIN_ADC_MAIN_VALID,
                           MAIN_VALID_DEBOUNCE_MS);
            }
        }
    }
    else
    {
        monet_data.debounceMainValidMs = MAIN_VALID_DEBOUNCE_MS;
        monet_data.mainPowerValid = 0;
    }

    if (((monet_data.batPowerCritical == 1) || 
         (monet_data.batPowerSave == 1)) &&
        (monet_data.mainPowerValid == 0))
    {
        monet_data.batPowerSave = 1;
        if (monet_data.in_pairing_mode == 0)
        {
            // Keep Advertising even battery critical.
            // monet_data.to_advertising = 0;
            // monet_data.to_advertising_recover = 0;
            // pf_ble_adv_connection_reset();
            // monet_data.to_advertising = 0;            //zazu-126 when battery crtical should disconnect then stop the advertise
            // monet_data.to_advertising_recover = 0;    //zazu-154 remove stop adv at enter lowpower mode.
            // pf_ble_adv_connection_reset();
        }
    }

    if (monet_data.batPowerSave == 1)
    {
        if ((monet_data.batPowerValid == 1) || 
            (monet_data.mainPowerValid == 1))
        {
            monet_data.batPowerSave = 0;
            pf_device_power_normal_indicate();
            pf_ble_adv_connection_reset();
            monet_data.to_advertising = 1;
        }
    }
    else
    {
        if (exit_from_lower_power_mode == true)
        {
            exit_from_lower_power_mode = false;
            {
                if ((monet_data.batPowerValid == 1) || 
                    (monet_data.mainPowerValid == 1))
                {
                    monet_data.batPowerSave = 0;
                    //pf_device_power_normal_indicate();
                    pf_log_raw(atel_log_ctl.core_en, "adv start lower power \r");
                    pf_ble_adv_connection_reset();
                    monet_data.to_advertising = 1;
                }
            }
        }
    }
}

// typedef enum {
//  eTypeReadRequest  = 1, 
//  eTypeWriteRequest,
//  eTypeDataPackBody,
//  eTypeAckPackData,
//  eTypeErrPackData,
//  eTypeOACK,
//  eTypeCapture      = 0x0064,  // expand from here
//  eTypeHeatBeat     = 0x0065,  // Heart beat
//  eTypeMax,
// } PackTypeT;

#if ((BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON) && BLE_CONNECTION_SUPPORT_HEARTBEAT)
void BleConnectionHeartBeat(uint32_t second)
{
    static uint32_t heart_beat_count = 0;
    static uint32_t last_quotient = 0;
    uint8_t buf[32] = {0};
    uint32_t tmp = 0;
    uint32_t ret = 0;

    heart_beat_count += second;

    tmp = heart_beat_count / MILISECOND_TO_SECONDS(CAMERA_HEARTBEAT_INTERVAL_MS);
    if (last_quotient != tmp)
    {
        last_quotient = tmp;

        buf[0] = 0x0065 & 0xff;
        buf[1] = (0x0065 >> 8) & 0xff;
        buf[2] = (count1sec >> 0) & 0xff;
        buf[3] = (count1sec >> 8) & 0xff;
        buf[4] = (count1sec >> 16) & 0xff;
        buf[5] = (count1sec >> 24) & 0xff;

        memcpy(buf + 6, monet_data.ble_mac_addr, BLE_MAC_ADDRESS_LEN);

        buf[12] = (monet_data.ble_conn_param.min_100us >> 0) & 0xff;
        buf[13] = (monet_data.ble_conn_param.min_100us >> 8) & 0xff;
        buf[14] = (monet_data.ble_conn_param.max_100us >> 0) & 0xff;
        buf[15] = (monet_data.ble_conn_param.max_100us >> 8) & 0xff;
        buf[16] = (monet_data.ble_conn_param.latency >> 0) & 0xff;
        buf[17] = (monet_data.ble_conn_param.latency >> 8) & 0xff;
        buf[18] = (monet_data.ble_conn_param.timeout_100us >> 0) & 0xff;
        buf[19] = (monet_data.ble_conn_param.timeout_100us >> 8) & 0xff;
        buf[20] = (monet_data.ble_conn_param.timeout_100us >> 16) & 0xff;
        buf[21] = (monet_data.ble_conn_param.timeout_100us >> 24) & 0xff;
        buf[22] = monet_data.cameraPowerOn;
        buf[23] = monet_data.appActive;

        ret = ble_aus_data_send_periheral(buf, 24, 0);
        
        pf_log_raw(atel_log_ctl.core_en, ">>>BleConnectionHeartBeat(%d) Err(%d).\r", count1sec, ret);
    }
}
#endif /* BLE_FUNCTION_ONOFF && BLE_CONNECTION_SUPPORT_HEARTBEAT */

void BatteryPowerHoldEn(FunCtrl Status)
{
    // Active high
    if (Status == HOLD) {
        pf_gpio_write(GPIO_MCU_BAT_EN, 1);
    } else if (Status == RELEASE) {
        pf_gpio_write(GPIO_MCU_BAT_EN, 0);
    }
}

void atel_uart_restore(void)
{
    if ((monet_data.uartToBeInit == 1) && (monet_data.uartTXDEnabled == 0)) {
        monet_data.uartTickCount++;
        if (monet_data.uartTickCount <= 0) {
            pf_gpio_write(GPIO_MCU_SLEEP_DEV, 1);
        }
        else if (monet_data.uartTickCount <= 0) {
            pf_gpio_write(GPIO_MCU_SLEEP_DEV, 0);
        }
        else {
            // TODO: disbale uart tx rx gpio function
            pf_uart_init();
            monet_data.uartToBeInit = 0;
            monet_data.uartTickCount = 0;
            monet_data.SleepState = SLEEP_OFF;
        }
    }
}

void ldr_int_process(void)
{
}

void ble_connection_status_refresh(void)
{
    monet_data.bleDisconnectDelayTimeMs = BLE_DISCONNECT_DELAY_MAX_MS;
}

void ble_connection_status_monitor(uint32_t delta)
{
    if (monet_data.bleConnectionStatus == 1)
    {
        if (monet_data.bleDisconnectDelayTimeMs >= delta)
        {
            monet_data.bleDisconnectDelayTimeMs -= delta;
        }
        else
        {
            monet_data.bleDisconnectDelayTimeMs = 0;
        }
        
        if (monet_data.bleDisconnectDelayTimeMs == 0)
        {
            pf_log_raw(atel_log_ctl.core_en, "ble_connection_status_monitor Connection Reset\r");
            pf_ble_adv_connection_reset();
            monet_data.to_advertising = 1;
        }
    }
}

void camera_poweroff_delay_refresh(void)
{
    if (monet_data.cameraPowerOffDelay != 0)
    {
        monet_data.cameraPowerOffDelay = CAMERA_POWER_OFF_DELAY_MS;
    }

    if (monet_data.cameraPowerOnDelay != 0)
    {
        if (monet_data.cameraPowerOnDelay < CAMERA_POWER_ON_DELAY_MS)
        {
            monet_data.cameraPowerOnDelay = CAMERA_POWER_ON_DELAY_MS;
        }
    }
}

void camera_poweroff_delay_detect(uint32_t delta)
{
    if (monet_data.cameraPowerOn == 1)
    {
        monet_data.cameraPowerOnTimeMs += delta;
        monet_data.cameraPowerOffTimeMs = 0;
    }
    else
    {
        monet_data.cameraPowerOnTimeMs = 0;
        monet_data.cameraPowerOffTimeMs += delta;
    }

    if (monet_data.cameraPowerOffDelay) {
       if (monet_data.cameraPowerOffDelay > delta) {
           monet_data.cameraPowerOffDelay -= delta;
       }
       else {
           monet_data.cameraPowerOffDelay = 0;
           pf_log_raw(atel_log_ctl.core_en, "Off into hibernate sleep: %d\r", monet_data.uartTXDEnabled);
           ble_send_timer_stop();
           MCU_TurnOff_Camera();
           pf_ble_peer_tx_rssi_stop();
       }
   }

   if ((monet_data.cameraPowerOnDelay) &&
       (monet_data.cameraPowerOn == 1) &&
       (monet_data.appActive == 1)) {
       if (monet_data.cameraPowerOnDelay > delta) {
           monet_data.cameraPowerOnDelay -= delta;
       } else {
           monet_data.cameraPowerOnDelay = 0;
           pf_log_raw(atel_log_ctl.core_en, "On into hibernate sleep: %d\r", monet_data.uartTXDEnabled);
           ble_send_timer_stop();
           MCU_TurnOff_Camera();
           pf_ble_peer_tx_rssi_stop();
       }
   }
}

void camera_power_abnormal_detect(uint32_t delta)
{
    static uint32_t debounce_ms = CAMERA_FIRST_POWER_OFF_DELAY_MS;

    if ((monet_data.cameraPowerOn == 1) && (monet_data.appActive == 1))   //zazumacu-107
    {
        debounce_ms = CAMERA_POWER_UP_ABNORMAL_DEBOUNCE;
        monet_data.cameraPowerAbnormal = 0;
        monet_data.cameraPowerAbnormalRetry = 0;
    }

    if (debounce_ms > CAMERA_POWER_UP_ABNORMAL_DEBOUNCE)
    {
        if (monet_data.cameraPowerOnDelay < CAMERA_POWER_ON_DELAY_MS)
        {
            monet_data.cameraPowerOnDelay = CAMERA_POWER_ON_DELAY_MS;
        }
    }

    // if ((monet_data.cameraPowerOn == 1) && (monet_data.appActive == 1))
    // {
    //     debounce_ms = CAMERA_POWER_UP_ABNORMAL_DEBOUNCE;
    //     monet_data.cameraPowerAbnormal = 0;
    //     monet_data.cameraPowerAbnormalRetry = 0;
    // }

    if ((monet_data.cameraPowerOn == 1) &&
        (monet_data.appActive == 0) &&
        (monet_data.cameraPowerAbnormal == 0))
    {
        if (debounce_ms > delta)
        {
            debounce_ms -= delta;
        }
        else
        {
            debounce_ms = 0;
        }

        if (debounce_ms == 0)
        {
            monet_data.cameraPowerAbnormalRetry++;
            pf_log_raw(atel_log_ctl.core_en, "Power up abnormal turn off(%d).\r", monet_data.cameraPowerAbnormalRetry);

            if (monet_data.cameraPowerAbnormalRetry <= CAMERA_POWER_UP_ABNORMAL_RETRY_MAX)
            {
                monet_data.cameraPowerAbnormal = 1;
            }
            else
            {
                pf_log_raw(atel_log_ctl.core_en, "Power up abnormal pf_sys_shutdown.\r");
                pf_delay_ms(100);
                pf_sys_shutdown();
            }
            ble_send_timer_stop();
            MCU_TurnOff_Camera();
            pf_ble_peer_tx_rssi_stop();

            monet_data.camera_commun_fail_times++;
            debounce_ms = CAMERA_POWER_UP_ABNORMAL_DEBOUNCE;
        }
    }
    else
    {
        if (debounce_ms > CAMERA_POWER_UP_ABNORMAL_DEBOUNCE)
        {
            return;
        }
        debounce_ms = CAMERA_POWER_UP_ABNORMAL_DEBOUNCE;
    }
}

void device_bootloader_enter_dealy(uint32_t delta)
{
    if ((monet_data.cameraBootEnterDelay) &&
        (monet_data.cameraPowerOn == 1))
    {
       if (monet_data.cameraBootEnterDelay > delta)
       {
           monet_data.cameraBootEnterDelay -= delta;
       }
       else
       {
           monet_data.cameraBootEnterDelay = 0;
           pf_log_raw(atel_log_ctl.core_en, ">>>Enter device bootloader.\r");
           pf_bootloader_enter();
           pf_log_raw(atel_log_ctl.core_en, ">>>Enter device err.\r");
       }
   }
}

void device_button_pattern_init(void)
{
    pf_button_pattern_timer_init();
}

int32_t device_button_state_get(void)
{
    return pf_gpio_read(GPIO_POWER_KEY);
}

void device_button_pattern_process(void)
{
    static device_button_state_t button_state = DEVICE_BUTTON_STATE_IDLE;
    static uint8_t button_release = 0;
    static uint32_t button_press_count = 0;
    static uint32_t count = 0;
    static uint32_t debounce_not_idle = 0;
    static uint32_t debounce_sys_time = 0;
    static uint32_t sample_time = 0;

    #if DEVICE_POWER_KEY_SUPPORT
    static uint8_t power_off_indicate = 0;
    #endif /* DEVICE_POWER_KEY_SUPPORT */

    #if DEVICE_BUTTON_FOR_FACTORY
    static uint8_t power_on_camera = 0;
    #endif /* DEVICE_BUTTON_FOR_FACTORY */

    static uint8_t unpairing_indicate = 0;

    switch(button_state)
    {
        case DEVICE_BUTTON_STATE_IDLE:
        if (device_button_state_get() != DEVICE_BUTTON_IDLE_STATE)
        {
            if (button_release == 0)
            {
                button_press_count++;
                if ((button_press_count % 10000) == 0)
                {
                    pf_log_raw(atel_log_ctl.core_en, "device_button_pattern_process button release required.\r");
                }
                return;
            }
            pf_log_raw(atel_log_ctl.core_en, "device_button_pattern_process idle to debounce.\r");
            pf_button_pattern_timer_start();
            button_state = DEVICE_BUTTON_STATE_DEBOUCE;
            count = 0;
            debounce_not_idle = 0;
            debounce_sys_time = 0;
            sample_time = 0;

            #if DEVICE_POWER_KEY_SUPPORT
            power_off_indicate = 0;
            #endif /* DEVICE_POWER_KEY_SUPPORT */

            #if DEVICE_BUTTON_FOR_FACTORY
            power_on_camera = 0;
            #endif /* DEVICE_BUTTON_FOR_FACTORY */

            unpairing_indicate = 0;

            if (power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].charging_process == 1) // when extern power charging if button process  0209 22 add
            {
                power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button = 1;
            }
            if (power_state.led_state[EXTERN_POWER_CHARGING_FULL].charging_process == 1)  // when extern power charging full if button process  0531 22 add
            {
                //power_state.led_state[EXTERN_POWER_CHARGING_FULL].charging_process = 0 ;
                power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button = 1;
            }
        }
        else
        {
            button_release = 1;
            if (button_press_count)
            {
                pf_log_raw(atel_log_ctl.core_en, "device_button_pattern_process button press count(%d).\r",
                                                  button_press_count);
                button_press_count = 0;
            }
            pf_button_pattern_timer_stop();
        }
        break;

        case DEVICE_BUTTON_STATE_DEBOUCE:
        {
            if (debounce_sys_time != pf_button_pattern_timer_get_ms())
            {
                sample_time += (pf_button_pattern_timer_get_ms() - debounce_sys_time);
                debounce_sys_time = pf_button_pattern_timer_get_ms();
            }
            else
            {
                break;
            }

            if (device_button_state_get() != DEVICE_BUTTON_IDLE_STATE)
            {
                count++;
            }

            if (sample_time >= DEVICE_BUTTON_CHECK_INTERVAL_MS)
            {
                if (count >= (DEVICE_BUTTON_CHECK_INTERVAL_MS / BUTTON_PATTERN_TIMER_PERIOD_MS / 2 + 1))
                {
                    debounce_not_idle++;
                    
                    
                    if (debounce_not_idle >= DEVICE_BUTTON_ACTION_THRESHOLD_PAIRING_BONDING)
                    {
                        button_state = DEVICE_BUTTON_STATE_PROCESS;
                        button_release = 0;
                        pf_log_raw(atel_log_ctl.core_en, "device_button_pattern_process to pair.\r");
                    }
                    else if (debounce_not_idle >= DEVICE_BUTTON_ACTION_THRESHOLD_UNPAIRING)
                    {
                        if (unpairing_indicate == 0)
                        {
                            unpairing_indicate = 1;
                            pf_ble_adv_connection_reset();
                            pf_device_unpairing_mode_enter();
                            pf_ble_white_list_reset();
                        }
                    }
                    else if (debounce_not_idle >= DEVICE_BUTTON_ACTION_THRESHOLD_POWER_ON_OFF)
                    {
                        #if DEVICE_POWER_KEY_SUPPORT
                        if (power_off_indicate == 0)
                        {
                            power_off_indicate = 1;
                            pf_device_power_off_mode_init();
                        }
                        #endif /* DEVICE_POWER_KEY_SUPPORT */

                        #if DEVICE_BUTTON_FOR_FACTORY
                        if (device_shipping_mode.mode == 1)
                        {
                            device_shipping_mode.wakesrc = DEV_SHIP_WAKE(KEY_VALID);
                        }

                        if (power_on_camera == 0)
                        {
                            power_on_camera = 1;
                            pf_device_button_for_factory_init();
                        }
                        #endif /* DEVICE_BUTTON_FOR_FACTORY */
                    }
                }
                else
                {
                    if (debounce_not_idle < DEVICE_BUTTON_ACTION_THRESHOLD_POWER_ON_OFF)
                    {
                        button_state = DEVICE_BUTTON_STATE_IDLE;
                        pf_button_pattern_timer_stop();
                        pf_log_raw(atel_log_ctl.core_en, "device_button_pattern_process to idle.\r");
                    }
                    else
                    {
                        button_state = DEVICE_BUTTON_STATE_PROCESS;
                        pf_log_raw(atel_log_ctl.core_en, "device_button_pattern_process to process.\r");
                    }
                }

                sample_time = 0;
                count = 0;
            }
        }
        break;

        case DEVICE_BUTTON_STATE_PROCESS:
        if (debounce_not_idle >= DEVICE_BUTTON_ACTION_THRESHOLD_PAIRING_BONDING)
        {
            if (power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].charging_process == 1) // when extern power charging if button process  0214 22 add
            {
                power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button = 1;
            }
            if (power_state.led_state[EXTERN_POWER_CHARGING_FULL].charging_process == 1)  // when extern power charging full if button process  0531 22 add
            {
                // power_state.led_state[EXTERN_POWER_CHARGING_FULL].charging_process = 0 ;
                power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button = 1;
            }
            pf_ble_adv_connection_reset();
            pf_device_pairing_mode_enter();
            CRITICAL_REGION_ENTER();
            reset_info.reset_from_shipping_mode_donot_adv_untill_pair = 0;
            monet_data.to_advertising = 1;
            monet_data.in_pairing_mode = 1;
            monet_data.in_pairing_mode_time = 0;
            monet_data.pairing_success = 0;
            CRITICAL_REGION_EXIT();
        }
        // else if (debounce_not_idle >= DEVICE_BUTTON_ACTION_THRESHOLD_TURNON_CAMERA)
        // {
        //     pf_device_upgrading_mode_enter();
        // }
        else if (debounce_not_idle >= DEVICE_BUTTON_ACTION_THRESHOLD_UNPAIRING)
        {
            pf_device_unpairing_mode_exit();
        }
        else if (debounce_not_idle >= DEVICE_BUTTON_ACTION_THRESHOLD_POWER_ON_OFF)
        {
            #if DEVICE_POWER_KEY_SUPPORT
            pf_device_power_off_mode_enter();
            #endif /* DEVICE_POWER_KEY_SUPPORT */

            #if DEVICE_BUTTON_FOR_FACTORY
            if (device_shipping_mode.mode == 0)
            {
                pf_device_button_for_factory_enter();
            }
            #endif /* DEVICE_BUTTON_FOR_FACTORY */
        }
        button_state = DEVICE_BUTTON_STATE_IDLE;
        pf_button_pattern_timer_stop();
        break;

        default:
        button_state = DEVICE_BUTTON_STATE_IDLE;
        pf_button_pattern_timer_stop();
        break;
    }
}

void CheckInterrupt(void)
{
    #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
    if (monet_data.bleConnectionEvent)
    {
        uint8_t buffer[2] = {0x04, 0x40};
        ble_connection_status_inform(monet_data.bleConnectionEvent - 1, 1);
        monet_data.bleConnectionEvent = 0;
        monet_data.bleConnectionStatus = 1;

        // Nala default TX power is 8dBm
        monet_data.ble_tx_power = 8;
        pf_ble_peer_tx_rssi_get(&monet_data.ble_rx_power);
        if (monet_data.ble_rx_power != 127)
        {
            monet_xAAcommand_nala_exception(buffer, 2);
        }

        // Enable the default 25hr (by default) watchdog timer
        // if (reset_info.ble_peripheral_paired == 1)
        {
            monet_gpio.WDtimer = monet_conf.WD.Reload;
        }
    }

    if (monet_data.bleDisconnectEvent)
    {
        ble_connection_status_inform(monet_data.bleDisconnectEvent - 1, 0);
        monet_data.bleDisconnectEvent = 0;
        monet_data.bleConnectionStatus = 0;
    }
    #endif /* BLE_FUNCTION_ONOFF */

    // When battery critical do not power on Camera
    if ((monet_data.batPowerCritical == 1) &&
        (monet_data.mainPowerValid == 0))
    {
        return;
    }

    if (((monet_data.RecvDataEvent != 0) && (monet_data.cameraPowerOff == 0)) ||
        (monet_data.cameraPowerAbnormal != 0) ||
        (monet_data.cameraPowerOnPending != 0) ||
        monet_data.upgrading_mode_enter)
    {
        if ((monet_data.RecvDataEvent) && (monet_data.cameraPowerOn == 0))
        {
            monet_data.cameraPowerOnPending = 1;
        }

        if ((monet_data.cameraPowerOn == 0) &&
            (monet_data.cameraPowerOffTimeMs >= CAMERA_POWER_ON_PENDING_MS))
        {
            turnOnCamera();
            ble_inform_camera_mac();
            //monet_requestAdc(1);
            monet_requestAdc(0);    //ZAZUMCU-104. 0212 22 corrected.  
          
            pf_log_raw(atel_log_ctl.core_en, ">>>turnOnCamera E(%d) A(%d) Ot(%d).\r",
                       monet_data.RecvDataEvent,
                       monet_data.cameraPowerAbnormal,
                       monet_data.cameraPowerOffTimeMs);
            monet_data.cameraPowerAbnormal = 0;
            monet_data.cameraPowerOnPending = 0;
        }
        ble_send_timer_start();
        monet_data.RecvDataEvent = 0;
    }

    if (monet_data.cameraPowerOff == 1)
    {
        monet_data.RecvDataEvent = 0;
        if (monet_data.cameraPowerOffTimeMs >= CAMERA_POWER_UP_MINIMUM_INTERVAL_MS)
        {
            monet_data.cameraPowerOff = 0;
        }
    }
}


/* Frame Structure: <0x7E><0x7E>$<L><C><P><CKSM><CR><LF>
 * <0x7E><0x7E> - Preamble
 * $ - 0x24
 * L - Length of <P>, 1 byte
 * C - Command, 1 byte
 * P - Parameters, <L> bytes
 * <CKSM> - Checksum, 1 byte
 * <CR> - 0x0D
 * <LF> - 0x0A
 *
 * nSize - the size of parameters
 */
void BuildFrame(uint8_t cmd, uint8_t * pParameters, uint8_t nSize)
{
    uint8_t  i;
    uint8_t sum;
    ring_buffer_t *pQueue;

    pQueue = &monet_data.txQueueU1;

    #if BLE_DATA_CHANNEL_SUPPORT
    if (IO_CMD_CHAR_BLE_RECV == cmd)
    {
        nSize += 1;
    }
    #endif /* BLE_DATA_CHANNEL_SUPPORT */

    if (ring_buffer_left_get(pQueue) < (nSize + 8)) {
        // No room
        return;
    }
    CRITICAL_REGION_ENTER();
    // IRQ_OFF;
    ring_buffer_in(pQueue, 0x7E);
    ring_buffer_in(pQueue, 0x7E);
    ring_buffer_in(pQueue, '$');

    pf_print_mdm_uart_tx_init();

    pf_print_mdm_uart_tx('$');
#if USE_CHECKSUM
    #if CMD_ESCAPE
    ring_buffer_in_escape(pQueue, nSize + 1);
    #else
    ring_buffer_in(pQueue, nSize + 1);
    #endif /* CMD_ESCAPE */

    pf_print_mdm_uart_tx(nSize + 1);
#else
    #if CMD_ESCAPE
    ring_buffer_in_escape(pQueue, nSize);
    #else
    ring_buffer_in(pQueue, nSize);
    #endif /* CMD_ESCAPE */

    pf_print_mdm_uart_tx(nSize);
#endif /* USE_CHECKSUM */
    #if CMD_ESCAPE
    ring_buffer_in_escape(pQueue, cmd);
    #else
    ring_buffer_in(pQueue, cmd);
    #endif /* CMD_ESCAPE */

    pf_print_mdm_uart_tx(cmd);

    sum = cmd;

    #if BLE_DATA_CHANNEL_SUPPORT
    if (IO_CMD_CHAR_BLE_RECV == cmd)
    {
    #if CMD_ESCAPE
        ring_buffer_in_escape(pQueue, (uint8_t)monet_data.data_recv_channel);
    #else
        ring_buffer_in(pQueue, (uint8_t)monet_data.data_recv_channel);
    #endif /* CMD_ESCAPE */

        pf_print_mdm_uart_tx((uint8_t)monet_data.data_recv_channel);

    #if USE_CHECKSUM
        sum += (uint8_t)monet_data.data_recv_channel;
    #endif /* USE_CHECKSUM */
        nSize -= 1;
    }
    #endif /* BLE_DATA_CHANNEL_SUPPORT */

    for(i=0; i < nSize; i++) {
        #if CMD_ESCAPE
        ring_buffer_in_escape(pQueue, pParameters[i]);
        #else
        ring_buffer_in(pQueue, pParameters[i]);
        #endif /* CMD_ESCAPE */

        pf_print_mdm_uart_tx(pParameters[i]);

        sum += pParameters[i];
    }

#if USE_CHECKSUM
    #if CMD_ESCAPE
    ring_buffer_in_escape(pQueue, sum ^ 0xFF);
    #else
    ring_buffer_in(pQueue, sum ^ 0xFF);
    #endif /* CMD_ESCAPE */
    
    pf_print_mdm_uart_tx(sum ^ 0xFF);
#endif /* USE_CHECKSUM */

    ring_buffer_in(pQueue, 0x0D);
    ring_buffer_in(pQueue, 0x0A); // For easier readability when capturing wire logs

    pf_print_mdm_uart_tx(0x0D);
    // IRQ_ON;
    CRITICAL_REGION_EXIT();
    pf_print_mdm_uart_tx_flush();
    // IRQ_ON;
}

#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
uint16_t ble_channel_get_from_mac(uint8_t *p_addr)
{
    uint16_t channel = 0xffff;
    uint16_t i = 0;

    if (p_addr == NULL)
    {
        return 0xffff;
    }

    for (i = 0; i < BLE_CHANNEL_NUM_MAX; i++)
    {
        if ((monet_data.ble_info[i].connect_status <= BLE_CONNECTION_STATUS_MAC_SET) &&
            (monet_data.ble_info[i].connect_status != BLE_CONNECTION_STATUS_NOTVALID))
        {
            uint8_t *p_tmp = monet_data.ble_info[i].mac_addr;
            if ((p_tmp[0] == p_addr[0]) &&
                (p_tmp[1] == p_addr[1]) &&
                (p_tmp[2] == p_addr[2]) &&
                (p_tmp[3] == p_addr[3]) &&
                (p_tmp[4] == p_addr[4]) &&
                (p_tmp[5] == p_addr[5]))
            {
                channel = i;
                break;
            }
        }
    }

    return channel;
}

uint16_t ble_channel_get_from_handler(uint16_t handler)
{
    uint16_t channel = 0xffff;
    uint16_t i = 0;

    for (i = 0; i < BLE_CHANNEL_NUM_MAX; i++)
    {
        // if (monet_data.ble_info[i].connect_status == BLE_CONNECTION_STATUS_CONNECTED)
        {
            if (handler == monet_data.ble_info[i].handler)
            {
                channel = i;
                break;
            }
        }
    }

    return channel;
}

uint16_t ble_connected_handler_get_from_channel(uint16_t channel)
{
    if (monet_data.ble_info[channel].connect_status == BLE_CONNECTION_STATUS_CONNECTED)
    {
        return monet_data.ble_info[channel].handler;
    }
    else
    {
        return 0xffff;
    }
}

uint16_t ble_information_set(uint16_t handler, uint8_t status, uint8_t *p_addr)
{
    uint16_t channel = ble_channel_get_from_mac(p_addr);

    CRITICAL_REGION_ENTER();
    if (channel < BLE_CHANNEL_NUM_MAX)
    {
        monet_data.ble_info[channel].handler = handler;
        monet_data.ble_info[channel].connect_status = status;
    }
    else
    {
        // TODO: It is better to find one not default 0, to support multi-channel
        channel = 0;
        memcpy(monet_data.ble_info[channel].mac_addr, p_addr, BLE_MAC_ADDRESS_LEN);
        if ((p_addr[0] | p_addr[1] | p_addr[2] | p_addr[3] | p_addr[4] | p_addr[5]) != 0)
        {
            monet_data.ble_info[channel].handler = handler;
            monet_data.ble_info[channel].connect_status = status;
        }
        else
        {
            monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_NOTVALID;
        }
    }
    CRITICAL_REGION_EXIT();
    return channel;
}

void ble_connection_channel_init(void)
{
    uint16_t channel = 0;

    for (channel = 0; channel < BLE_CHANNEL_NUM_MAX; channel++)
    {
        monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_NOTVALID;
        monet_data.ble_info[channel].handler = 0xffff;
    }
}

void ble_connected_channel_clear(void)
{
    uint16_t channel = 0;

    for (channel = 0; channel < BLE_CHANNEL_NUM_MAX; channel++)
    {
        if (monet_data.ble_info[channel].connect_status == BLE_CONNECTION_STATUS_CONNECTED)
        {
            monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_NOT_CONNECTED;
            // monet_data.ble_info[channel].handler = 0xffff;
        }
    }
}

uint16_t ble_connected_channel_num_get(void)
{
    uint16_t channel = 0;
    uint16_t num = 0;

    for (channel = 0; channel < BLE_CHANNEL_NUM_MAX; channel++)
    {
        if (monet_data.ble_info[channel].connect_status == BLE_CONNECTION_STATUS_CONNECTED)
        {
            num++;
        }
    }

    return num;
}

void ble_connection_status_inform(uint16_t channel, uint8_t state)
{
    uint8_t buf[16] = {0};

    buf[0] = 'c';
    buf[1] = channel;
    buf[2] = state;
    BuildFrame(IO_CMD_CHAR_BLE_CTRL, buf, 3);
}

void monet_bleScommand(uint8_t* pParam, uint8_t Length, uint16_t channel)
{
    if (BLE_DATA_SEND_BUFFER_CELL_LEN < Length)
    {
        pf_log_raw(atel_log_ctl.error_en, "monet_bleScommand Length(%d) err\r", Length);
    }
    //pf_log_raw(atel_log_ctl.error_en, "monet_bleScommand (%x:%d:%d:%d) \r", pParam[0],pParam[1],pParam[2],pParam[3]);
    camera_poweroff_delay_refresh();

    // pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_bleScommand len(%d)\r", Length);
    // printf_hex_and_char(pParam, Length);

    if (is_ble_send_queue_full() == 0)
    {
        if (Length)
        {
            ble_send_data_push(pParam, Length, channel);
        }
    }
    else
    {
        pf_log_raw(atel_log_ctl.error_en, "monet_bleScommand queue full drop\r");
    }
}

void monet_bleCcommand_m(uint8_t** param, uint8_t *p_len)
{
    uint8_t Param[64] = {0};
    uint8_t i = 0, j = 0;

    *param += 1;
    *p_len -= 1;

    Param[0] = 'M';
    Param[1] = 0xff;
    i += 2;
    
    memcpy(Param + 2, monet_data.ble_mac_addr, BLE_MAC_ADDRESS_LEN);
    i += BLE_MAC_ADDRESS_LEN;

    for (j = 0; j < BLE_CHANNEL_NUM_MAX; j++)
    {
        Param[i] = j;
        i++;
        memcpy(Param + i, monet_data.ble_info[j].mac_addr, BLE_MAC_ADDRESS_LEN);
        i += BLE_MAC_ADDRESS_LEN;
    }

    BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, i);
}

void monet_bleCcommand_M(uint8_t** param, uint8_t *p_len)
{
    #define BLE_CONTRIL_M_COMMAND_LEN (7)
    uint16_t channel = 0xffff;
    uint8_t Param[32] = {0};

    *param += 1;
    *p_len -= 1;

    channel = (*param)[0];

    if (channel >= BLE_CHANNEL_NUM_MAX)
    {
        pf_log_raw(atel_log_ctl.error_en, "monet_bleCcommand_M channel(%d) err\r", channel);
        return;
    }

    if (*p_len < BLE_CONTRIL_M_COMMAND_LEN)
    {
        pf_log_raw(atel_log_ctl.error_en, "monet_bleCcommand_M len(%d) err\r", *p_len);
        return;
    }

    Param[0] = 'm';
    Param[1] = channel;
    memcpy(Param + 2, *param + 1, BLE_MAC_ADDRESS_LEN);
    Param[8] = 0xff;
    memcpy(Param + 9, monet_data.ble_mac_addr, BLE_MAC_ADDRESS_LEN);

    memcpy(monet_data.ble_info[channel].mac_addr, *param + 1, BLE_MAC_ADDRESS_LEN);
    if (((*param)[1] | (*param)[2] | (*param)[3] | (*param)[4] | (*param)[5] | (*param)[6]) != 0)
    {
        monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_MAC_SET;
    }
    else
    {
        monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_NOTVALID;
    }
    // monet_data.ble_info[channel].handler = 0xffff;

    *param += BLE_CONTRIL_M_COMMAND_LEN;
    *p_len -= BLE_CONTRIL_M_COMMAND_LEN;

    BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, BLE_CONTRIL_M_COMMAND_LEN + 1 + BLE_MAC_ADDRESS_LEN + 1);
}

void monet_bleCcommand_W(uint8_t** param, uint8_t *p_len)
{
    uint8_t Param[16] = {0};

    *param += 1;
    *p_len -= 1;

    ble_aus_white_list_set();

    Param[0] = 'w';
    BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, 1);
}

// void monet_bleCcommand_A(uint8_t** param, uint8_t *p_len)
// {
//     uint8_t Param[16] = {0};
//     uint8_t err_code = 0;

//     *param += 1;
//     *p_len -= 1;

//     if ((*param)[0] == 0)
//     {
//         ble_aus_advertising_stop();
//     }
//     else if ((*param)[0] == 1)
//     {
//         err_code = ble_aus_advertising_start();
//     }
//     else
//     {
//         ble_aus_advertising_stop();
//         err_code = ble_aus_advertising_start();
//     }

//     Param[0] = 'a';
//     Param[1] = (*param)[0];
//     Param[2] = err_code;

//     *param += 1;
//     *p_len -= 1;

//     BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, 3);
// }
#endif /* BLE_FUNCTION_ONOFF */

#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
void monet_bleCcommand_P(uint8_t** param, uint8_t *p_len)
{
    #define BLE_AUS_CONN_PARAM_LEN (11)
    ble_aus_conn_param_t ble_aus_conn_param;
    uint16_t channel = 0xffff;
    uint8_t Param[16] = {0};

    *param += 1;
    *p_len -= 1;

    if (*p_len < BLE_AUS_CONN_PARAM_LEN)
    {
        NRF_LOG_RAW_INFO("monet_bleCcommand_P len(%d) err\r", *p_len);
        NRF_LOG_FLUSH();
        return;
    }

    Param[0] = 'p';
    memcpy(Param + 1, *param, BLE_AUS_CONN_PARAM_LEN);

    channel = (*param)[0];
    monet_data.ble_conn_param[channel].max_100us = ((*param)[3] | ((*param)[4] << 8));
    monet_data.ble_conn_param[channel].min_100us = ((*param)[1] | ((*param)[2] << 8));
    monet_data.ble_conn_param[channel].latency = ((*param)[5] | ((*param)[6] << 8));
    monet_data.ble_conn_param[channel].timeout_100us = ((*param)[7] | ((*param)[8] << 8) | ((*param)[9] << 16) | ((*param)[10] << 24));

    ble_aus_conn_param.min_ms = monet_data.ble_conn_param[channel].min_100us / 10.0;
    ble_aus_conn_param.max_ms = monet_data.ble_conn_param[channel].max_100us / 10.0;
    ble_aus_conn_param.latency = monet_data.ble_conn_param[channel].latency;
    ble_aus_conn_param.timeout_ms = monet_data.ble_conn_param[channel].timeout_100us / 10;

    NRF_LOG_RAW_INFO("monet_bleCcommand_P Min(%d) Max(%d) Latency(%d) Timeout(%d)\r",
                     ble_aus_conn_param.min_ms,
                     ble_aus_conn_param.max_ms,
                     ble_aus_conn_param.latency,
                     ble_aus_conn_param.timeout_ms);
    NRF_LOG_FLUSH();

    monet_data.ble_conn_param_update_ok[channel] = ble_aus_change_change_conn_params(channel, ble_aus_conn_param);

    if (monet_data.ble_conn_param_update_ok[channel] != 0)
    {
        memset(Param + 2, 0, BLE_AUS_CONN_PARAM_LEN - 1);
    }

    *param += BLE_AUS_CONN_PARAM_LEN;
    *p_len -= BLE_AUS_CONN_PARAM_LEN;

    BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, BLE_AUS_CONN_PARAM_LEN + 1);
}

void monet_bleCcommand_C(uint8_t** param, uint8_t *p_len)
{
    uint16_t channel = 0xffff;
    uint8_t Param[16] = {0};

    *param += 1;
    *p_len -= 1;

    channel = (*param)[0];

    Param[0] = 'c';
    Param[1] = channel;
    Param[2] = 0;
    if (monet_data.ble_info[channel].connect_status == BLE_CONNECTION_STATUS_CONNECTED)
    {
        Param[2] = 1;
    }

    *param += 1;
    *p_len -= 1;

    BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, 3);
}

#if BLE_DTM_ENABLE
void monet_bleCcommand_D(uint8_t** param, uint8_t *p_len)
{
    uint8_t Param[16] = {0};
    uint8_t parm_len = 0;

    *param += 1;
    *p_len -= 1;

    Param[0] = 'd';
    Param[1] = (*param)[0];
    parm_len += 2;

    pf_log_raw(atel_log_ctl.io_protocol_en, "monet_bleCcommand_D cmd(0x%x)\r", (*param)[0]);

    switch ((*param)[0])
    {
        case 'E':
        pf_dtm_enter();
        break;

        case 'C':
        pf_dtm_cmd((*param)[1], (*param)[2], (*param)[3], (*param)[4]);
        memcpy(Param + 2, *param + 1, 4);
        *param += 4;
        *p_len -= 4;
        parm_len += 4;
        break;

        case 'X':
        pf_dtm_exit();
        break;

        default:
        pf_log_raw(atel_log_ctl.error_en, "monet_bleCcommand_D unknown cmd\r");
        *p_len = 0;
        break;
    }

    NRF_LOG_FLUSH();

    *param += 1;
    *p_len -= 1;

    BuildFrame('3', Param, parm_len);
}
#endif /* BLE_DTM_ENABLE */

void ble_inform_camera_mac(void)
{
    uint8_t left_len = 16;
    uint8_t *p_tmp = NULL;

    monet_bleCcommand_m(&p_tmp, &left_len);
}

void monet_bleCcommand_f(uint8_t** param, uint8_t *p_len)
{
    static uint8_t filter_set = 0;

    #define BLE_CONTRIL_F_COMMAND_LEN (7)
    uint8_t Param[16] = {0};

    *param += 1;
    *p_len -= 1;

    Param[0] = 'F';
    Param[1] = (*param)[0];

    if (*p_len < BLE_CONTRIL_F_COMMAND_LEN)
    {
        pf_log_raw(atel_log_ctl.error_en, "monet_bleCcommand_f len(%d) err\r", *p_len);
    }
    
    memcpy(Param + 2, *param + 1, BLE_MAC_ADDRESS_LEN);

    *param += BLE_CONTRIL_F_COMMAND_LEN;
    *p_len -= BLE_CONTRIL_F_COMMAND_LEN;

    // ble_aus_set_adv_filter(Param + 2);
    // When image command received, encryption should already be ready.
    if (filter_set == 0)
    {
        filter_set = (ble_aus_white_list_set() == 0) ? 1 : 0;
    }
    pf_log_raw(atel_log_ctl.io_protocol_en, "monet_bleCcommand_f do not process!\r");

    BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, BLE_CONTRIL_F_COMMAND_LEN + 1);
}
#endif /* BLE_FUNCTION_ONOFF */

void monet_bleCcommand_L(uint8_t** param, uint8_t *p_len)
{
    uint8_t Param[16] = {0};

    *param += 1;
    *p_len -= 1;

    Param[0] = 'l';
    Param[1] = (*param)[0];

    if ((*param)[0] == 1)
    {
        monet_data.ldr_enable_status = 1;
    }
    else
    {
        monet_data.ldr_enable_status = 0;
    }

    BuildFrame(IO_CMD_CHAR_BLE_CTRL, Param, 1 + 1);
}

void monet_bleCcommand(uint8_t* pParam, uint8_t Length)
{
    uint8_t left_len = Length;
    uint8_t *p_tmp = pParam;

    if ((monet_data.CANExistChargerNoExist == 1) && 
        (monet_data.bleShouldAdvertise == 0))
    {
        if ((*p_tmp == 'W') || (*p_tmp == 'f') || (*p_tmp == 'P'))
        {
            pf_log_raw(atel_log_ctl.error_en, "monet_bleCcommand(%c) not support with CAN.\r", *p_tmp);
            return;
        }
    }

// BLE_C_AGAIN:
    switch (*p_tmp)
    {
        #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
        case 'm':
        monet_bleCcommand_m(&p_tmp, &left_len);
        break;

        case 'M':
        monet_bleCcommand_M(&p_tmp, &left_len);
        break;

        case 'W':
        monet_bleCcommand_W(&p_tmp, &left_len);
        break;

        // Zazu will advertise by default
        // case 'A':
        // monet_bleCcommand_A(&p_tmp, &left_len);
        // break;

        case 'f':
        monet_bleCcommand_f(&p_tmp, &left_len);
        break;
        #endif /* BLE_FUNCTION_ONOFF */

        case 'L':
        monet_bleCcommand_L(&p_tmp, &left_len);
        break;

        #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
        case 'P':
        monet_bleCcommand_P(&p_tmp, &left_len);
        break;

        case 'C':
        monet_bleCcommand_C(&p_tmp, &left_len);
        break;
        #endif /* BLE_FUNCTION_ONOFF */

        #if BLE_DTM_ENABLE
        case 'D':
        monet_bleCcommand_D(&p_tmp, &left_len);
        break;
        #endif /* BLE_DTM_ENABLE */

        default:
        pf_log_raw(atel_log_ctl.error_en, "monet_bleCcommand unknown cmd\r");
        left_len = 0;
        break;
    }

    // if (left_len > 0)
    // {
    //     goto BLE_C_AGAIN;
    // }
}

#if SPI_CAN_CONTROLLER_EN
void monet_canScommand(uint8_t* pParam, uint8_t Length, uint16_t channel)
{
    can_controller_tx(pParam, Length, channel);
}
#endif /* SPI_CAN_CONTROLLER_EN */

void monet_requestAdc(uint8_t adc)
{
    uint8_t pParm[8];
    uint16_t adc_bat = monet_data.AdcBackup;
    uint16_t adc_vin = monet_data.AdcMain;
    uint16_t adc_vsin = monet_data.AdcSolar;
    pf_log_raw(atel_log_ctl.io_protocol_en,"Before_is_charger_power_on(%d):%d \r",is_charger_power_on(),adc);
    switch (adc)
    {
        case 0:
        {

        }
        break;
        case 1:   // turn off the charger, and read the accuracy vbat
        {
            if (is_charger_power_on() != false)
            {
                setChargerOff();
                nrf_delay_ms(10);
                atel_adc_conv_force();
		        atel_adc_conv_force();
		        atel_adc_conv_force();
                atel_adc_conv_force();
                atel_adc_conv_force();
                //atel_adc_conv_force();
                monet_data.charge_state_report_vbat_A = true;
            }
            else
            {
                atel_adc_conv_force();
                atel_adc_conv_force();
                atel_adc_conv_force();
                atel_adc_conv_force();
                atel_adc_conv_force();
                //atel_adc_conv_force();
            }
            //if (is_charger_power_on() != false)
            {
                adc_bat = monet_data.AdcBackupAccuracy;
            }
            pf_log_raw(atel_log_ctl.io_protocol_en,"AdcBackupAccuracy:(%d) \r",PF_ADC_RAW_TO_BATT_MV(adc_bat));
            pf_log_raw(atel_log_ctl.io_protocol_en,"AdcBackup:(%d) \r",PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup));
        }
        break;
        case 2:
        {
            if (monet_data.charge_state_report_vbat_A == true)
            {
                monet_data.charge_state_report_vbat_A = false;
                setChargerOn(); 
            }
        }
        break;
        default:
        pf_log_raw(atel_log_ctl.io_protocol_en,"monet_Acommand unknow(%d) \r",is_charger_power_on());
        break;
    }
    pf_log_raw(atel_log_ctl.io_protocol_en,"After_is_charger_power_on(%d):%d \r",is_charger_power_on(),adc);
    adc_bat = (uint16_t)PF_ADC_RAW_TO_BATT_MV(adc_bat);
    adc_vin = (uint16_t)PF_ADC_RAW_TO_VIN_MV(adc_vin);
    adc_vsin = (uint16_t)PF_ADC_RAW_TO_VSIN_MV(adc_vsin);

    pParm[0]= (uint8_t)LSB_16(adc_bat);
    pParm[1]= (uint8_t)MSB_16(adc_bat);
    pParm[2]= (uint8_t)LSB_16(adc_vin);
    pParm[3]= (uint8_t)MSB_16(adc_vin);
    pParm[4]= (uint8_t)LSB_16(adc_vsin);
    pParm[5]= (uint8_t)MSB_16(adc_vsin);
    if (monet_data.solar_mode_select_delay == true && monet_data.charge_state_report_vbat_A == false)
    {
        monet_data.solar_mode_select_delay = false;
        solar_chg_mode_select(monet_data.solar_chg_mode_choose); // if need to select_chg_mode,avoid some risk,need to choose delay.
    }
    BuildFrame('a', pParm, 6);
}

//Handle when esp32 can't read its file system.
void monet_Dcommand(uint8_t* pParam)
{
    uint8_t Response[6] = {0};
    uint8_t index = 0;
    monet_data.camera_files_read_fail_times++;
    Response[0] = pParam[0];
    index = 1;
    BuildFrame('d',Response,index);
}

/* E: configure */
void monet_Ecommand(uint8_t* pParam)
{
    uint8_t pin = pParam[0];
    uint8_t status = pParam[1];
    uint16_t timer = pParam[2]+(pParam[3]<<8);

    pf_log_raw(atel_log_ctl.io_protocol_en, "monet_Ecommand pin(%d) status(0x%x) timer(%d)\r", pin, status, timer);
    CRITICAL_REGION_ENTER();
    monet_conf.gConf[pin].Reload = (timer + TIME_UNIT - 1) / TIME_UNIT;
    monet_conf.gConf[pin].status = status;
    monet_gpio.counter[pin] = 0;
    if(status&GPIO_INTO_MASK) {
        if(monet_conf.IntPin != GPIO_TO_INDEX(GPIO_NONE)) {
            monet_conf.gConf[monet_conf.IntPin].status &= (uint8_t)(~GPIO_INTO_MASK);
        }
        monet_conf.IntPin = pin;
        monet_conf.gConf[pin].status |= GPIO_INTO_MASK;
        monet_gpio.Intstatus= 0;
    } else {
        if(monet_conf.IntPin == pin) {
            monet_conf.IntPin = GPIO_TO_INDEX(GPIO_NONE);
            monet_gpio.Intstatus= 0;
        }
    }
    if(status&GPIO_WD_MASK) {
        if(monet_conf.WD.Pin != GPIO_TO_INDEX(GPIO_NONE)) {
            monet_conf.gConf[monet_conf.WD.Pin].status &= (uint8_t)(~GPIO_WD_MASK);
        }
        monet_conf.WD.Pin = pin;
    } else {
        if(monet_conf.WD.Pin == pin)
        {
            monet_conf.WD.Pin = GPIO_TO_INDEX(GPIO_NONE);
        }
    }
    CRITICAL_REGION_EXIT();
    configGPIO(pin, status);
    if ((status & GPIO_DIRECTION) == DIRECTION_OUT) {
        SetGPIOOutput(pin, (bool)((status&GPIO_SET_HIGH) >0));
    } else {
        GPIO_SET(pin, pf_gpio_read(pin));
    }
    // save_config();
    pParam[1]= monet_conf.gConf[pin].status;
    BuildFrame('e', pParam, 4);
}

void monet_Gcommand(uint8_t* pParam, uint8_t Length)
{
    uint8_t pin;
    uint8_t x, y, z, s;

    pin = pParam[0];
    x = (uint8_t)(pParam[1] ? 1 : 0); // 1: Input 0: Output
    y = (uint8_t)((monet_conf.gConf[pin].status & GPIO_MODE) ? 1 : 0);
    z = (uint8_t)((monet_conf.gConf[pin].status & GPIO_OUTPUT_HIGH) ? 1 : 0);
    s = (uint8_t)((pParam[2]) ? 1 : 0);

    // WARNING: "G" command will set [pin].status bit0-bit3 to 0, so interrupt type will lose efficacy
    monet_conf.gConf[pin].status = (uint8_t)(PIN_STATUS(x, y, z, s));
    monet_gpio.counter[pin] = 0;
    monet_conf.gConf[pin].Reload = 0;

    configGPIO(pin, monet_conf.gConf[pin].status);
    if ((monet_conf.gConf[pin].status & GPIO_DIRECTION) == DIRECTION_OUT)
    {
        SetGPIOOutput(pin, (bool)((uint8_t)pParam[2] > (uint8_t)0));
    }

    pParam[2] =  GPIO_GET(pin);

    BuildFrame('g', pParam, 3);
}

void monet_Icommand(void)
{
    uint8_t pParam[4];
    CRITICAL_REGION_ENTER();
    pParam[0] = (uint8_t)(monet_gpio.Intstatus       & 0xFF);
    pParam[1] = (uint8_t)((monet_gpio.Intstatus>>8)  & 0xFF);
    pParam[2] = (uint8_t)((monet_gpio.Intstatus>>16) & 0xFF);
    pParam[3] = (uint8_t)((monet_gpio.Intstatus>>24) & 0xFF);
    monet_gpio.Intstatus = 0;
    CRITICAL_REGION_EXIT();
    BuildFrame('i', pParam, 4);
    eventIReadyFlag = true;

    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_Icommand\r");
}

void monet_Jcommand(void) { // Added for Loader interface
    uint8_t pParam[4] = {0};

    if (monet_data.batPowerValid || monet_data.mainPowerValid)
    {
        monet_data.cameraBootEnterDelay = MCU_APP_TO_BOOT_DELAY_MS;
        // Error Code
        // 0: Able to enter updating mode
        pParam[0] = 0;
    }
    else
    {
        // Error Code
        // 1: Power Not Valid
        pParam[0] = 1;
    }
    BuildFrame('j', pParam, 1);
}

// Send baseband watchdog and reset it to default value
void monet_Kcommand(uint8_t* pParam, uint8_t Length)
{
    uint8_t response[16] = {0};
    uint32_t WDReload = 0;
    CRITICAL_REGION_ENTER();

    response[0] = (uint8_t)((monet_gpio.WDtimer >> 0) & 0x000000ff);
    response[1] = (uint8_t)((monet_gpio.WDtimer >> 8) & 0x000000ff);
    response[2] = (uint8_t)((monet_gpio.WDtimer >> 16) & 0x000000ff);
    response[3] = (uint8_t)((monet_gpio.WDtimer >> 24) & 0x000000ff);

    if (Length == 4)
    {
        WDReload = (pParam[0] << 0  |
                    pParam[1] << 8  |
                    pParam[2] << 16 |
                    pParam[3] << 24 );
    }
    else
    {
        WDReload = WATCHDOG_DEFAULT_RELOAD_VALUE;
    }

    if (WDReload != 0xffffffff)
    {
        if (WDReload < WATCHDOG_DEFAULT_RELOAD_VALUE_MIN)
        {
            monet_conf.WD.Reload = WATCHDOG_DEFAULT_RELOAD_VALUE_MIN;
        }
        else if (WDReload > WATCHDOG_DEFAULT_RELOAD_VALUE_MAX)
        {
            monet_conf.WD.Reload = WATCHDOG_DEFAULT_RELOAD_VALUE_MAX;
        }
        else
        {
            monet_conf.WD.Reload = WDReload;
        }

        monet_gpio.WDtimer = monet_conf.WD.Reload;
        monet_gpio.WDflag = 0;
    }

    response[4] = (uint8_t)((monet_conf.WD.Reload >> 0) & 0x000000ff);
    response[5] = (uint8_t)((monet_conf.WD.Reload >> 8) & 0x000000ff);
    response[6] = (uint8_t)((monet_conf.WD.Reload >> 16) & 0x000000ff);
    response[7] = (uint8_t)((monet_conf.WD.Reload >> 24) & 0x000000ff);
    CRITICAL_REGION_EXIT();

    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_Kcommand WDReload(%u) WD.Reload(%u) WD(%u).\r",
               WDReload,
               monet_conf.WD.Reload,
               monet_gpio.WDtimer);

    BuildFrame('k', response, 8);
}

void monet_Lcommand(uint8_t* pParam, uint8_t Length)
{
    monet_data.led_flash_enable = pParam[0];

    BuildFrame('l', pParam, Length);
}

void monet_ncommand(uint8_t* pParam, uint8_t Length)
{
    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_ncommand len: %d:%d Serial Number(%s)\r", 
               Length, pParam[0], pParam + 1);
    memcpy(monet_data.cameraSerialNumber, pParam, pParam[0] + 1);
}

void monet_Pcommand(uint8_t* pParam, uint8_t Length)
{
    uint16_t send_len = 0;
    uint8_t count = 0;
    uint8_t *p_data = NULL;
    uint16_t data_send_channel = 0xffff;
    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_Pcommand(%d)\r", pParam[0]);
    switch(pParam[0]) {
        case 0: {
            turnOffCamera();
            #if !CAMERA_POWER_OFF_DELAY_EN
            ble_send_timer_stop();
            MCU_TurnOff_Camera();
            #endif /* CAMERA_POWER_OFF_DELAY_EN */
            // The limit of camera power up interval
            // monet_data.cameraPowerOff = 1;
            pf_ble_peer_tx_rssi_stop();
            pf_log_raw(atel_log_ctl.platform_en, "Delete the queue esp32 active  %d %d %d\r", p_data[0],send_len,is_ble_send_queue_empty());
            while (is_ble_send_queue_empty() == 0)
            {
                
                if (monet_data.mcuDataHandleMs == 0) // whenever ble disconnect or connect, delete the queue directly.but besides the mcudataHandles not as 0.
                {
                    ble_send_data_pop(&p_data, &send_len, &data_send_channel);
                    pf_log_raw(atel_log_ctl.platform_en, "Delete the queue esp32 active  %d %d\r", p_data[0],send_len);
                    if (send_len)
                    {
                        if (p_data != NULL)
                        {
                            ble_send_data_delete_one();
                        }
                    }
                    pf_log_raw(atel_log_ctl.platform_en, "Delete the queue esp32 active  %d\r", p_data[0]);
                }
                count++;
                if (count > BLE_DATA_SEND_BUFFER_CELL_NUM)
                {
                    break;
                }
            }
        }
        break;
        case 1: {
            turnOffCameraDelay(0);
        }
        break;
        case 2:    //AT+Sleep=2,esp32 give one byte,will delay 10min,give three byte,will delay 1 hour. for factory mode
        {
            uint32_t delay_ms = 0;
            uint16_t delay_min = 0;
            if (Length == 3)
            {
                delay_min = (pParam[1] | (pParam[2] << 8));
                if (delay_min > CAMERA_POWER_ON_FACTORY_DELAY_MIN_MAX)
                {
                    delay_min = CAMERA_POWER_ON_FACTORY_DELAY_MIN_MAX;
                }
                else if (delay_min == 0)
                {
                    delay_min = CAMERA_POWER_ON_FACTORY_DELAY_MS / 1000;
                }
                delay_ms = delay_min * (60 * 1000);
            }
            else
            {
                delay_ms = CAMERA_POWER_ON_FACTORY_DELAY_MS;
            }
            turnOffCameraDelay(delay_ms);
        }
        break;
        case 3:
            // ble_connection_status_monitor(BLE_SLOW_SCAN_RESET_MAX_MS);
        break;
        case 4:
            ble_send_timer_start();
        break;
        case 5:
            ble_send_timer_stop();
            pf_ble_peer_tx_rssi_stop();
        break;
        case 6:
        turnOffCamera();
        #if !CAMERA_POWER_OFF_DELAY_EN
        ble_send_timer_stop();
        MCU_TurnOff_Camera();
        #endif /* CAMERA_POWER_OFF_DELAY_EN */
        // The limit of camera power up interval
        // monet_data.cameraPowerOff = 1;
        pf_ble_peer_tx_rssi_stop();
        //device_shipping_mode.mode = 0;
        //device_shipping_mode.pending = 1;
        //device_shipping_mode.wakesrc = 0;
        if (exist_main_power() == true)
        {
            pf_log_raw(atel_log_ctl.io_protocol_en, "Warning: Exit extern power do not enter shipping mode \r");
        }
        else
        {
            device_shipping_mode.mode = 0;
            device_shipping_mode.pending = 1;
            device_shipping_mode.wakesrc = 0;     //zazumcu -114
        }
        break;
        case 7:
        device_shipping_mode.mode = 0;
        device_shipping_mode.pending = 0;
        device_shipping_mode.wakesrc = 0;
        break;
        case 8:
        pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_Pcommand(%d) pf_sys_shutdown\r", pParam[0]);
        pf_delay_ms(100);
        pf_sys_shutdown();
        break;
        default:
        // Extract the power state config data
        // ConfigPowerMasks(pParam, Length);
        pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_Pcommand(%d) Unknown\r", pParam[0]);
        break;
    }
    BuildFrame('p', pParam, Length);
}


/*
 * First Parameter Command
 * 0x01: Read GPIO all
 * 0x02: Read GPIO x
 * 0x03: OneWireDisable
 *
 */
void monet_Qcommand(uint8_t* pParam, uint8_t Length)
{
    uint8_t command, value;
    uint8_t gpio;
    uint8_t ReportData[4] = {0};
    uint8_t size;

    command = pParam[0];
    size = 0;

    CRITICAL_REGION_ENTER();
    ReportData[0] = command;
    switch (command) {
    case 0x01:
        value = 0;
        for (gpio = 0; gpio < MAX_EXT_GPIOS; gpio++)
        {
            value = (uint8_t)(value | (GPIO_GET(gpio) << gpio));
        }
        ReportData[1] = value;
        size = 2;
        break;
    case 0x02:
        gpio = pParam[1];
        ReportData[1] = gpio;
        ReportData[2] = monet_conf.gConf[gpio].status;
        ReportData[3] = GPIO_GET(gpio);
        size = 4;
        break;
    }
    CRITICAL_REGION_EXIT();
    if (size) {
        BuildFrame('q', &ReportData[0], size);
    }
}

void ble_data_rate_inform(uint32_t tx, uint32_t rx)
{
    uint8_t pParm[8] = {0};

    memcpy(pParm, &tx, sizeof(uint32_t));
    memcpy(pParm + sizeof(uint32_t), &rx, sizeof(uint32_t));

    BuildFrame('r', pParm, sizeof(uint32_t) * 2);
}

void monet_Scommand(void)
{
    BuildFrame('s', &monet_data.CANExistChargerNoExist, 1);
}

void monet_Tcommand(uint8_t* pParam, uint8_t Length)
{
    uint16_t critical = 0, low = 0;
    uint8_t c = 0, l = 0;
    uint8_t response[8] = {0};

    if (Length < 4)
    {
        pf_log_raw(atel_log_ctl.io_protocol_en,
                   ">>>>monet_Tcommand Length(%d) < 4 Error.\r",
                   Length);
        return;
    }

    critical    = (pParam[0] << 0) | (pParam[1] << 8);
    low         = (pParam[2] << 0) | (pParam[3] << 8);

    if (critical == 0xffff)
    {
        c = 1;
        critical = monet_data.batCriticalThresh;
    }

    if (low == 0xffff)
    {
        l = 1;
        low = monet_data.batLowThresh;
    }

    if (low >= BUB_HIGH_THRESHOLD)
    {
        monet_data.batCriticalThresh = BUB_CRITICAL_THRESHOLD;
        monet_data.batLowThresh = BUB_LOW_THRESHOLD;

        // debug_tlv_info.batCriticalThresh = BUB_CRITICAL_THRESHOLD;
        // debug_tlv_info.batLowThresh = BUB_LOW_THRESHOLD;
    }
    else
    {
        monet_data.batLowThresh = low;
        // debug_tlv_info.batLowThresh = low;
        if (critical > low)
        {
            monet_data.batCriticalThresh = low;
            // debug_tlv_info.batCriticalThresh = low;
        }
        else
        {
            monet_data.batCriticalThresh = critical;
            // debug_tlv_info.batCriticalThresh = critical;
        }
    }

    pf_log_raw(atel_log_ctl.io_protocol_en,
               ">>>>monet_Tcommand Critical(%d) Low(%d).\r",
                monet_data.batCriticalThresh,
                monet_data.batLowThresh);

    response[0] = (monet_data.batCriticalThresh >> 0) & 0xff;
    response[1] = (monet_data.batCriticalThresh >> 8) & 0xff;
    response[2] = (monet_data.batLowThresh >> 0) & 0xff;
    response[3] = (monet_data.batLowThresh >> 8) & 0xff;

    if (c && l)
    {
        // Do nothing, respond all values
    }
    else if (c)
    {
        response[0] = 0xff;
        response[1] = 0xff;
    }
    else if (l)
    {
        response[2] = 0xff;
        response[3] = 0xff;
    }

    BuildFrame('t', response, 4);
}

void monet_Vcommand()
{
    uint8_t pParm[8];
    uint8_t p_buff[4] = {0};
    p_buff[0] = (DFU_FLAG_REGISTER2 >> 24) & 0xff;  //Bootloder MNT_BUILD
    p_buff[1] = (DFU_FLAG_REGISTER2 >> 16) & 0xff;  //Bootloder MNT_REVISION
    p_buff[2] = (DFU_FLAG_REGISTER2 >>  8) & 0xff;  //Bootloder MNT_MINOR
    p_buff[3] = (DFU_FLAG_REGISTER2 >>  0) & 0xff;  //Bootloder MNT_MAJOR
#if FACTORY_TESTING_FUNCTION
    pParm[0] = 0xff; // Factory version
#else
    pParm[0] = MNT_MAJOR;
#endif /* FACTORY_TESTING_FUNCTION */

    pParm[1] = MNT_MINOR;
    pParm[2] = MNT_REVISION;
    pParm[3] = MNT_BUILD;
    pParm[4] = p_buff[0];
    pParm[5] = p_buff[1];
    pParm[6] = p_buff[2];
    pParm[7] = p_buff[3];

    BuildFrame('v', pParm, 8);
    
    if (monet_data.in_upgrading_mode)
    {
        pf_device_upgrading_success();
    }
}

void monet_vcommand(uint8_t *pParam, uint8_t Length)
{
    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_vcommand len: %d Version: %d.%d.%d.%d\r", 
               Length, pParam[0], pParam[1], pParam[2], pParam[3]);
    
    monet_data.cameraFwVersion[0] = pParam[0];
    monet_data.cameraFwVersion[1] = pParam[1];
    monet_data.cameraFwVersion[2] = pParam[2];
    monet_data.cameraFwVersion[3] = pParam[3];
    monet_data.cameraHID = pParam[4] | (pParam[5] << 8);
    monet_data.cameraRev = pParam[6];
    monet_data.cameraConfVer[0] = pParam[7];
    monet_data.cameraConfVer[1] = pParam[8];

    if (monet_data.cameraHID == ZAZU_HARDWARE_ID_BLE)
    {
        if (monet_data.CANExistChargerNoExist == 1)
        {
            if (monet_data.bleShouldAdvertise == 0)
            {
                if (reset_info.reset_from_shipping_mode_donot_adv_untill_pair == 0)
                {
                    monet_data.to_advertising = 1;
                }
            }
            monet_data.bleShouldAdvertise = 1;
        }
    }

    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_vcommand HID(0x%x) CAN(%d) ADV(%d:%d) ", 
               monet_data.cameraHID, 
               monet_data.CANExistChargerNoExist, 
               monet_data.to_advertising, 
               monet_data.bleShouldAdvertise);
    pf_log_raw(atel_log_ctl.io_protocol_en, "ConfVer(%02x%02x)\r",  
               monet_data.cameraConfVer[1], 
               monet_data.cameraConfVer[0]);
}

// ('C')Charger control
void monet_Zcommand(uint8_t *pParam, uint8_t Length)
{
    uint8_t nSize = 0;
    uint8_t Response[10] = {0};
    uint32_t value;

    //    memcpy(z_buf, pParam, len);
    Response[0] = pParam[0];

    switch (pParam[0])
    {
        case 'C':
        {
            // uint16_t oldChargeValue = monet_data.ChargerRestartValue;
            // Get the new charger threshold
            value = pParam[1] + (pParam[2] << 8);
            monet_data.ChargerRestartValue = (uint16_t)(value);
            //Restart the charger
            // if (oldChargeValue != monet_data.ChargerRestartValue)
            // {
            //     if (monet_data.V3PowerOn == 0)
            //     {
            //         setChargerOn();
            //     }
            // }
            // Prepare the response
            Response[1] = pParam[1];
            Response[2] = pParam[2];
            nSize = 3;
        }
        break;
    }

    if (nSize)
    {
        BuildFrame('z', Response, nSize);
    }
}


//for 0xDD command debug_TLV.
//report perpherial infomation.
void send_string_to_nala(uint8_t* p_data)
{
    uint32_t len = 0;
    if (!p_data)  //pData = NULL
    {
        return ;
    }
    else
    {
        len = strlen((char*)p_data + 4);
        len += 4;
        //BuildFrame(0xDD,(uint8_t*)p_data,len);
        pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_xDDcommand Response %d %d %d %d %d \r",p_data[0],p_data[1],p_data[2],p_data[3],len);

        #if SPI_CAN_CONTROLLER_EN
        if ((monet_data.CANExistChargerNoExist == 1) && 
            (monet_data.bleShouldAdvertise == 0))
        {
        #if CAN_DATA_CHANNEL_SUPPORT
            monet_canScommand(send_buf, index, channel);
        #else
            //monet_canScommand(Response, index, 0);
            monet_canScommand((uint8_t*)p_data, len, 0);
        #endif /* CAN_DATA_CHANNEL_SUPPORT */
        }
        else
        #endif /* SPI_CAN_CONTROLLER_EN */
        {
            // TODO: Multi-channel support
            #if BLE_DATA_CHANNEL_SUPPORT
            uint16_t channel = 0;
           // monet_bleScommand(pParm, index, channel); // WARNING: channel not defined
            monet_bleScommand((uint8_t*)p_data, len, 0); // WARNING: channel not defined
            #else
            //monet_bleScommand(Response, index, 0);
            monet_bleScommand((uint8_t*)p_data, len, 0);
            #endif /* BLE_DATA_CHANNEL_SUPPORT */
        }
    }
}




//0309 22 add

#define COMPONENT_LIST_NALA_NORDIC_MCU        (4)
#define COMPONENT_LIST_CAMERA_NORDIC_MCU      (6)
#define COMPONENT_LIST_CAMERA_ESP32_MCU       (7)


void monet_xDDcommand(uint8_t* pParam, uint8_t Length)
{
    uint8_t stringBuff[150] = {0};
    stringBuff[0] = 0xDD;
    stringBuff[1] = pParam[0];
    stringBuff[2] = pParam[1];
    stringBuff[3] = pParam[2];

    uint8_t component = 0;
   // uint32_t type_mask = 0;
    uint8_t index = 0;
    uint8_t Response[45] = {0};
    // uint32_t type_mask_size = 8;

    Response[0] = 0xDD;
    index++;
    // Response[1] = pParam[1];
    // index++;
    // Response[2] = pParam[2];
    // index++;
    switch(pParam[0])
    {
        case 0:    //Running seconds 
        {
            Response[index + 0] = (uint8_t)((count1sec >> 0)  & 0xff);
            Response[index + 1] = (uint8_t)((count1sec >> 8)  & 0xff);
            Response[index + 2] = (uint8_t)((count1sec >> 16) & 0xff);
            Response[index + 3] = (uint8_t)((count1sec >> 24) & 0xff);
            index += 4;
        }
        break;

        case 1:   // Wake up times
        {
            //TODO: WAKEUP TIMES

        }
        break;

        case 2:  //Peripheral info
        {
            component = pParam[1];  // the st was gived by app
            //if (component != COMPONENT_LIST_NALA_NORDIC_MCU)
            if (component != 1)  
            {
                pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_xDDcommand component invalid.\r");
                return;
            }
            Response[1] = pParam[0];
            index++;
            //Nala no instance id 
            //Bit 7 presents type_mask size: 1 for 4 byte; 0 for 1 byte
            Response[2] = pParam[1];
            index++;
            Response[3] = pParam[2];
            index++;

            Response[index + 0] = (uint8_t)((monet_data.cameraTurnOnTimes >> 0)    & 0xff);
            Response[index + 1] = (uint8_t)((monet_data.cameraTurnOnTimes >> 8)    & 0xff);
            Response[index + 2] = (uint8_t)((monet_data.cameraTurnOnTimes >> 16)   & 0xff);
            Response[index + 3] = (uint8_t)((monet_data.cameraTurnOnTimes >> 24)   & 0xff);
            Response[index + 4] = (uint8_t)((monet_data.cameraTurnOffTimes >> 0)   & 0xff);
            Response[index + 5] = (uint8_t)((monet_data.cameraTurnOffTimes >> 8)   & 0xff);
            Response[index + 6] = (uint8_t)((monet_data.cameraTurnOffTimes >> 16)  & 0xff);
            Response[index + 7] = (uint8_t)((monet_data.cameraTurnOffTimes >> 24)  & 0xff);
            Response[index + 8] = ((uint16_t)(PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup)) >> 0) & 0xff;
            Response[index + 9] = ((uint16_t)(PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup)) >> 8) & 0xff;

            // Response[index + 10] = (uint8_t)((monet_data.restartTimes >> 0)     & 0xff);
            // Response[index + 11] = (uint8_t)((monet_data.restartTimes >> 8)     & 0xff);
            // Response[index + 12] = (uint8_t)((monet_data.restartTimes >> 16)    & 0xff);
            // Response[index + 13] = (uint8_t)((monet_data.restartTimes >> 24)    & 0xff);
            // Response[index + 14] = (uint8_t)((monet_data.restartReason >> 0)    & 0xff);
            // Response[index + 15] = (uint8_t)((monet_data.restartReason >> 8)    & 0xff);
            // Response[index + 16] = (uint8_t)((monet_data.restartReason >> 16)   & 0xff);
            // Response[index + 17] = (uint8_t)((monet_data.restartReason >> 24)   & 0xff);


            Response[index + 10] = (uint8_t)((debug_tlv_info.resumeTimes >> 0)     & 0xff);
            Response[index + 11] = (uint8_t)((debug_tlv_info.resumeTimes >> 8)     & 0xff);
            Response[index + 12] = (uint8_t)((debug_tlv_info.resumeTimes >> 16)    & 0xff);
            Response[index + 13] = (uint8_t)((debug_tlv_info.resumeTimes >> 24)    & 0xff);
            Response[index + 14] = (uint8_t)((debug_tlv_info.restartReason >> 0)    & 0xff);
            Response[index + 15] = (uint8_t)((debug_tlv_info.restartReason >> 8)    & 0xff);
            Response[index + 16] = (uint8_t)((debug_tlv_info.restartReason >> 16)   & 0xff);
            Response[index + 17] = (uint8_t)((debug_tlv_info.restartReason >> 24)   & 0xff);




            Response[index + 18] = (uint8_t)((monet_data.camera_commun_fail_times >> 0)    & 0xff);
            Response[index + 19] = (uint8_t)((monet_data.camera_commun_fail_times >> 8)    & 0xff);
            Response[index + 20] = (uint8_t)((monet_data.camera_commun_fail_times >> 16)   & 0xff);
            Response[index + 21] = (uint8_t)((monet_data.camera_commun_fail_times >> 24)   & 0xff);
            Response[index + 22] = (uint8_t)((monet_data.camera_files_read_fail_times >> 0)    & 0xff);
            Response[index + 23] = (uint8_t)((monet_data.camera_files_read_fail_times >> 8)    & 0xff);
            Response[index + 24] = (uint8_t)((monet_data.camera_files_read_fail_times >> 16)   & 0xff);
            Response[index + 25] = (uint8_t)((monet_data.camera_files_read_fail_times >> 24)   & 0xff);

            Response[index + 26] = (uint8_t)((count1sec >> 0)  & 0xff);
            Response[index + 27] = (uint8_t)((count1sec >> 8)  & 0xff);
            Response[index + 28] = (uint8_t)((count1sec >> 16) & 0xff);
            Response[index + 29] = (uint8_t)((count1sec >> 24) & 0xff);

            Response[index + 30] = (uint8_t)((monet_data.temp_ntc >> 0) & 0xff); 
            Response[index + 31] = (uint8_t)((monet_data.temp_ntc >> 8) & 0xff);
            Response[index + 32] = (uint8_t)((monet_data.temp_nordic >> 0)  & 0xff);
            Response[index + 33] = (uint8_t)((monet_data.temp_nordic >> 8)  & 0xff);
            Response[index + 34] = (uint8_t)((monet_data.temp_nordic >> 16) & 0xff);
            Response[index + 35] = (uint8_t)((monet_data.temp_nordic >> 24) & 0xff);
          


            index += 36;
            pf_log_raw(atel_log_ctl.error_en, "Receive nala 0xDD command4 %d %d %d %d  \r",Response[0],Response[1],Response[2],Response[3]);
        }
        break;

        default: //reserved
        pf_log_raw(atel_log_ctl.error_en, ">>>>monet_xDDcommand type invalid.\r");
        return;
        break;
    }

    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_xDDcommand Response:\r");
    printf_hex_and_char(Response, index);
    sprintf((char*)stringBuff + 4, "TEON:%"PRIu32".TEOFF:%"PRIu32".BV:%"PRIu16".TNB:%"PRIu32",%08lx.TCB:%"PRIu32".TEF:%"PRIu32".RUNTIME:%"PRIu32".TNTC:%"PRId16".TMCU:%"PRId32".BTVER:%d.%d.%d.%d.\r\n",
            monet_data.cameraTurnOnTimes,monet_data.cameraTurnOffTimes,(uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup),debug_tlv_info.resumeTimes,debug_tlv_info.restartReason,
            monet_data.camera_commun_fail_times,monet_data.camera_files_read_fail_times,count1sec,monet_data.temp_ntc,monet_data.temp_nordic,
            monet_data.boot_major,monet_data.boot_minor,monet_data.boot_revision,monet_data.boot_build);

    send_string_to_nala(stringBuff);
  

    // #if SPI_CAN_CONTROLLER_EN
    // if ((monet_data.CANExistChargerNoExist == 1) && 
    //     (monet_data.bleShouldAdvertise == 0))
    // {
    // #if CAN_DATA_CHANNEL_SUPPORT
    //     monet_canScommand(send_buf, index, channel);
    // #else
    //     monet_canScommand(Response, index, 0);
    // #endif /* CAN_DATA_CHANNEL_SUPPORT */
    // }
    // else
    // #endif /* SPI_CAN_CONTROLLER_EN */
    // {
    //     // TODO: Multi-channel support
    //     #if BLE_DATA_CHANNEL_SUPPORT
    //     uint16_t channel = 0;
    //     monet_bleScommand(pParm, index, channel); // WARNING: channel not defined
    //     #else
    //     monet_bleScommand(Response, index, 0);
    //     #endif /* BLE_DATA_CHANNEL_SUPPORT */
    // }
}









void monet_xAAcommand_nala_exception(uint8_t* pParam, uint8_t Length)
{
    uint8_t component = 0;
    uint32_t type_mask = 0;
    uint8_t i = 0, index = 0;
    uint8_t pParm[128] = {0};
    uint32_t type_mask_size = 8;

    pParm[0] = 0xAA;
    index++;

    component = pParam[0];

    if (component != COMPONENT_LIST_NALA_NORDIC_MCU)
    {
        pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_xAAcommand_nala_exception component invalid.\r");
        return;
    }

    // Nala no instance id
    // Bit 7 presents type_mask size: 1 for 4 byte; 0 for 1 byte
    if (pParam[1] & 0x80)
    {
        type_mask_size = 32;
        type_mask = pParam[1] | ((pParam[2] & 0xff) << 8) | ((pParam[3] & 0xff) << 16) | ((pParam[4] & 0xff) << 24);
    }
    else
    {
        type_mask = pParam[1];
    }

    memcpy(pParm + 1, pParam, 1 + (type_mask_size / 8));
    index += (1 + (type_mask_size / 8));

    for (i = 0; i < type_mask_size; i++)
    {
        if ((type_mask & (1 << i)) && (i != 7))
        {
            pParm[index] = i;
            index++;

            switch (i)
            {
                case 6: // 0x40  6    Radio Information
                {
                    pParm[index] = 6;
                    index++;
                    pParm[index + 0] = 1;
                    pParm[index + 1] = (monet_data.ble_rx_power >> 0) & 0xff;
                    pParm[index + 2] = (monet_data.ble_rx_power >> 8) & 0xff;
                    pParm[index + 3] = 2;
                    pParm[index + 4] = (monet_data.ble_tx_power >> 0) & 0xff;
                    pParm[index + 5] = (monet_data.ble_tx_power >> 8) & 0xff;
                    index += 6;
                }
                break;

                default:
                    pf_log_raw(atel_log_ctl.error_en, ">>>>monet_xAAcommand_nala_exception type invalid.\r");
                    return;
                    break;
            }
        }
    }

    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_xAAcommand_nala_exception Response:\r");
    printf_hex_and_char(pParm, index);

    #if SPI_CAN_CONTROLLER_EN
    if ((monet_data.CANExistChargerNoExist == 1) && 
        (monet_data.bleShouldAdvertise == 0))
    {
        #if CAN_DATA_CHANNEL_SUPPORT
        monet_canScommand(pParm, index, channel);
        #else
        monet_canScommand(pParm, index, 0);
        #endif /* CAN_DATA_CHANNEL_SUPPORT */
    }
    else
    #endif /* SPI_CAN_CONTROLLER_EN */
    {
        // TODO: Multi-channel support
        #if BLE_DATA_CHANNEL_SUPPORT
        uint16_t channel = 0;
        monet_bleScommand(pParm, index, channel); // WARNING: channel not defined
        #else
        monet_bleScommand(pParm, index, 0);
        #endif /* BLE_DATA_CHANNEL_SUPPORT */
    }
}

/*
 * Component List
 * 0 Reserved
 * 1 Simba MCU
 * 2 Simba BLE MCU
 * 3 SLP-01 MCU
 * 4 Nala MCU
 * 5 Nala MUX MCU
 * 6 Camera Nordic MCU (multi-instance)
 * 7 Camera ESP32 MCU (multi-instance)
 * 8 Nala Charger Chip
 * 9 Temperature BLE Sensor (multi-instance)
 * 10 Door BLE Sensor (multi-instance)
 * 
 * Information Type List
 * Mask    Bit  Information Type
 * 0x01    0    Version
 * 0x02    1    MAC Address
 * 0x04    2    Hardware ID and Revision
 * 0x08    3    Charger Chip Data
 * 0x10    4    Serial Number
 * 0x20    5    Power Information
 * 0x40    6    Radio Information
 * 0x80    7    32-vit Extension When set, indicates that the Information Type Mask field is 32-bit wide (instead of 8-bit field).
 * 0x100   8    Component Configuration File Version
 */

void monet_xAAcommand(uint8_t* pParam, uint8_t Length)
{
    uint8_t component = 0;
    // uint8_t instance_id = 0;
    uint32_t type_mask = 0;
    uint8_t i = 0, index = 0;
    uint8_t pParm[128] = {0};
    uint32_t type_mask_size = 8;

    pParm[0] = 0xAA;
    index++;

    component = pParam[0];

    if ((component != COMPONENT_LIST_CAMERA_NORDIC_MCU) &&
        (component != COMPONENT_LIST_CAMERA_ESP32_MCU))
    {
        pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_xAAcommand component invalid.\r");
        return;
    }

    // instance_id = pParam[1];

    // Bit 7 presents type_mask size: 1 for 4 byte; 0 for 1 byte
    if (pParam[2] & 0x80)
    {
        type_mask_size = 32;
        type_mask = pParam[2] | ((pParam[3] & 0xff) << 8) | ((pParam[4] & 0xff) << 16) | ((pParam[5] & 0xff) << 24);
    }
    else
    {
        type_mask = pParam[2];
    }

    memcpy(pParm + 1, pParam, 1 + 1 + (type_mask_size / 8));
    index += (1 + 1 + (type_mask_size / 8));

    for (i = 0; i < type_mask_size; i++)
    {
        if ((type_mask & (1 << i)) && (i != 7))
        {
            pParm[index] = i;
            index++;

            switch (i)
            {
                case 0: // 0x01  0    Version
                    if (component == COMPONENT_LIST_CAMERA_NORDIC_MCU)
                    {
                        pParm[index] = 4;
                        index++;
                        pParm[index + 0] = MNT_MAJOR;
                        pParm[index + 1] = MNT_MINOR;
                        pParm[index + 2] = MNT_REVISION;
                        pParm[index + 3] = MNT_BUILD;
                        index += 4;
                    }
                    else if (component == COMPONENT_LIST_CAMERA_ESP32_MCU)
                    {
                        pParm[index] = 4;
                        index++;
                        pParm[index + 0] = monet_data.cameraFwVersion[0];
                        pParm[index + 1] = monet_data.cameraFwVersion[1];
                        pParm[index + 2] = monet_data.cameraFwVersion[2];
                        pParm[index + 3] = monet_data.cameraFwVersion[3];
                        index += 4;
                    }
                    break;

                case 1: // 0x02  1    MAC Address
                    if (component == COMPONENT_LIST_CAMERA_NORDIC_MCU)
                    {
                        pParm[index] = 6;
                        index++;
                        pParm[index + 0] = monet_data.ble_mac_addr[0];
                        pParm[index + 1] = monet_data.ble_mac_addr[1];
                        pParm[index + 2] = monet_data.ble_mac_addr[2];
                        pParm[index + 3] = monet_data.ble_mac_addr[3];
                        pParm[index + 4] = monet_data.ble_mac_addr[4];
                        pParm[index + 5] = monet_data.ble_mac_addr[5];
                        index += 6;
                    }
                    else if (component == COMPONENT_LIST_CAMERA_ESP32_MCU)
                    {
                        // Not Supported
                    }
                break;

                case 2: // 0x04  2    Hardware ID and Revision
                    pParm[index] = 3;
                    index++;
                    pParm[index + 0] = (monet_data.cameraHID >> 0) & 0xff;
                    pParm[index + 1] = (monet_data.cameraHID >> 8) & 0xff;
                    pParm[index + 2] = monet_data.cameraRev;
                    index += 3;
                break;

                case 3: // 0x08  3    Charger Chip Data
                {
                    uint16_t input_vol = 0, input_lim = 0, back_vol = 0;
                    uint16_t input_cur = 0, charg_cur = 0;

                    if ((monet_data.CANExistChargerNoExist == 0) && 
                        (pf_charger_is_power_on()))
                    {
                        input_vol = (uint16_t)pf_charger_input_voltage_get();
                        input_lim = (uint16_t)pf_charger_input_voltage_limit_get();
                        back_vol = (uint16_t)pf_charger_battery_voltage_get();
                        input_cur = (uint16_t)pf_charger_input_current_get();
                        charg_cur = (uint16_t)pf_charger_charging_current_get();
                    }
                    pf_log_raw(atel_log_ctl.io_protocol_en, 
                               ">>>>monet_xAAcommand Charger Chip(%d:%d).\r", 
                               monet_data.CANExistChargerNoExist,
                               is_charger_power_on());
                    pParm[index] = 10;
                    index++;
                    pParm[index + 0] = (input_vol >> 0) & 0xff;
                    pParm[index + 1] = (input_vol >> 8) & 0xff;
                    pParm[index + 2] = (input_lim >> 0) & 0xff;
                    pParm[index + 3] = (input_lim >> 8) & 0xff;
                    pParm[index + 4] = (back_vol >> 0) & 0xff;
                    pParm[index + 5] = (back_vol >> 8) & 0xff;
                    pParm[index + 6] = (input_cur >> 0) & 0xff;
                    pParm[index + 7] = (input_cur >> 8) & 0xff;
                    pParm[index + 8] = (charg_cur >> 0) & 0xff;
                    pParm[index + 9] = (charg_cur >> 8) & 0xff;
                    index += 10;
                }
                break;

                case 4: // 0x10  4    Serial Number
                {
                    uint8_t length = monet_data.cameraSerialNumber[0];
                    pParm[index] = length;
                    index++;
                    memcpy(pParm + index, monet_data.cameraSerialNumber + 1, length);
                    index += length;
                }
                break;

                case 5: // 0x20  5    Power Information
                {
                    pParm[index] = 15;
                    index++;
                    pParm[index + 0] = 1;
                    pParm[index + 1] = ((uint16_t)(PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain)) >> 0) & 0xff;
                    pParm[index + 2] = ((uint16_t)(PF_ADC_RAW_TO_VIN_MV(monet_data.AdcMain)) >> 8) & 0xff;
                    pParm[index + 3] = 2;
                    pParm[index + 4] = ((uint16_t)(PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup)) >> 0) & 0xff;
                    pParm[index + 5] = ((uint16_t)(PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup)) >> 8) & 0xff;
                    pParm[index + 6] = 3;
                    pParm[index + 7] = ((uint16_t)(PF_ADC_RAW_TO_VSIN_MV(monet_data.AdcSolar)) >> 0) & 0xff;
                    pParm[index + 8] = ((uint16_t)(PF_ADC_RAW_TO_VSIN_MV(monet_data.AdcSolar)) >> 8) & 0xff;
                    pParm[index + 9] = 4;
                    pParm[index + 10] = (monet_data.batLowThresh >> 0) & 0xff;
                    pParm[index + 11] = (monet_data.batLowThresh >> 8) & 0xff;
                    pParm[index + 12] = 5;
                    pParm[index + 13] = (monet_data.batCriticalThresh >> 0) & 0xff;
                    pParm[index + 14] = (monet_data.batCriticalThresh >> 8) & 0xff;
                    index += 15;
                }
                break;

                case 6: // 0x40  6    Radio Information
                {
                    pParm[index] = 6;
                    index++;
                    pParm[index + 0] = 1;
                    pParm[index + 1] = (monet_data.ble_rx_power >> 0) & 0xff;
                    pParm[index + 2] = (monet_data.ble_rx_power >> 8) & 0xff;
                    pParm[index + 3] = 2;
                    pParm[index + 4] = (monet_data.ble_tx_power >> 0) & 0xff;
                    pParm[index + 5] = (monet_data.ble_tx_power >> 8) & 0xff;
                    index += 6;
                }
                break;

                case 8: // 0x100    Component Configuration File Version
                {
                    pParm[index] = 2;
                    index++;
                    pParm[index + 0] = monet_data.cameraConfVer[0];
                    pParm[index + 1] = monet_data.cameraConfVer[1];
                    index += 2;
                }
                break;

                default:
                    pf_log_raw(atel_log_ctl.error_en, ">>>>monet_xAAcommand type invalid.\r");
                    return;
                    break;
            }
        }
    }

    pf_log_raw(atel_log_ctl.io_protocol_en, ">>>>monet_xAAcommand Response:\r");
    printf_hex_and_char(pParm, index);

    #if SPI_CAN_CONTROLLER_EN
    if ((monet_data.CANExistChargerNoExist == 1) && 
        (monet_data.bleShouldAdvertise == 0))
    {
        #if CAN_DATA_CHANNEL_SUPPORT
        monet_canScommand(pParm, index, channel);
        #else
        monet_canScommand(pParm, index, 0);
        #endif /* CAN_DATA_CHANNEL_SUPPORT */
    }
    else
    #endif /* SPI_CAN_CONTROLLER_EN */
    {
        // TODO: Multi-channel support
        #if BLE_DATA_CHANNEL_SUPPORT
        uint16_t channel = 0;
        monet_bleScommand(pParm, index, channel); // WARNING: channel not defined
        #else
        monet_bleScommand(pParm, index, 0);
        #endif /* BLE_DATA_CHANNEL_SUPPORT */
    }
}

typedef struct
{
    uint32_t charger_on;            // 1: on, 0: off
    uint32_t mode;                  // 1: mode1, 2: mode2
    uint32_t input_vol;             // Unit in mV
    double input_cur;               // Unit in mA
    double input_power;             // Unit in uW
    uint32_t input_vol_limit;       // Unit in mV
    uint32_t bat_vol;               // Unit in mV
    double bat_chg_cur;             // Unit in mA
    double bat_chg_power;           // Unit in uW
} factory_test_charger_param_t;

// Set solar charging mode.
// "AT*SOLCHGM?\r\n", read current mode
// "AT*SOLCHGM=N1\r\n"
//    N1=1, Mode 1, Solar power --> Charger chip --> battery
//    N1=2, Mode 2, Solar power --> battery
void monet_0Ccommand(uint8_t* pParam, uint8_t Length)
{
    uint8_t response[64] = {0};
    uint32_t charger_on = 0;      // 1: on, 0: off
    uint32_t mode = 0;            // 1: mode1, 2: mode2
    uint32_t input_vol = 0;       // Unit in mV
    double input_cur = 0.0;       // Unit in mA
    double input_power = 0.0;     // Unit in uW
    uint32_t input_vol_limit = 0; // Unit in mV
    uint32_t bat_vol = 0.0;       // Unit in mV
    double bat_chg_cur = 0.0;     // Unit in mA
    double bat_chg_power = 0.0;   // Unit in uW

    factory_test_charger_param_t charger_param = {0};

    if (Length == 1 && pParam[0] == '1')
    {
        solar_chg_mode_set(SOLAR_CHG_MODE1);
        pf_log_raw(atel_log_ctl.io_protocol_en, "monet_0Ccommand Mode 1\r");
    }
    else if (Length == 1 && pParam[0] == '2')
    {
        solar_chg_mode_set(SOLAR_CHG_MODE2);
        pf_log_raw(atel_log_ctl.io_protocol_en, "monet_0Ccommand Mode 2\r");
    }

    setChargerOff();
    pf_delay_ms(100);
    setChargerOn();

    pf_delay_ms(1000); // Delay to stablize Charger chip state

    charger_on = is_charger_power_on();
    mode = solar_chg_mode_get();
    input_vol = mp2762a_input_vol_get();
    input_cur = mp2762a_input_cur_get();
    input_power = input_vol * input_cur;
    input_vol_limit = (uint32_t)mp2762a_input_vol_limit_get();
    bat_vol = (uint32_t)mp2762a_bat_vol_get();
    bat_chg_cur = mp2762a_bat_chg_cur_get();
    bat_chg_power = bat_vol * bat_chg_cur;

    charger_param.charger_on        = charger_on;
    charger_param.mode              = mode;
    charger_param.input_vol         = input_vol;
    charger_param.input_cur         = input_cur;
    charger_param.input_power       = input_power;
    charger_param.input_vol_limit   = input_vol_limit;
    charger_param.bat_vol           = bat_vol;
    charger_param.bat_chg_cur       = bat_chg_cur;
    charger_param.bat_chg_power     = bat_chg_power;

    pf_log_raw(atel_log_ctl.io_protocol_en, "Charger On %d\r\n", charger_on);
    pf_log_raw(atel_log_ctl.io_protocol_en, "Charger Mode %d\r\n", mode);
    pf_log_raw(atel_log_ctl.io_protocol_en, "InputVol %u mV\r\n", input_vol);
    pf_log_raw(atel_log_ctl.io_protocol_en, "InputCur "NRF_LOG_FLOAT_MARKER" mA\r\n", NRF_LOG_FLOAT(input_cur));
    pf_log_raw(atel_log_ctl.io_protocol_en, "InputPw "NRF_LOG_FLOAT_MARKER" uW\r\n", NRF_LOG_FLOAT(input_power));
    pf_log_raw(atel_log_ctl.io_protocol_en, "InputVolLmt %u mV\r\n", input_vol_limit);
    pf_log_raw(atel_log_ctl.io_protocol_en, "BatVol %u mV\r\n", bat_vol);
    pf_log_raw(atel_log_ctl.io_protocol_en, "ChgCur "NRF_LOG_FLOAT_MARKER" mA\r\n", NRF_LOG_FLOAT(bat_chg_cur));
    pf_log_raw(atel_log_ctl.io_protocol_en, "ChgPw "NRF_LOG_FLOAT_MARKER" uW\r\n", NRF_LOG_FLOAT(bat_chg_power));

    response[0] = 'c';
    memcpy(response + 1, &charger_param, sizeof(charger_param));

    BuildFrame('0', response, sizeof(charger_param) + 1);

    solar_chg_mode_select(FUNC_JUMP_POINT_0);
}

void monet_0command(uint8_t* pParam, uint8_t Length)
{
    uint8_t cmd = pParam[0];

    if (Length < 1)
    {
        pf_log_raw(atel_log_ctl.io_protocol_en, "monet_0command Factory Test Error Length.\r");
        return;
    }

    switch (cmd)
    {
        case 'C':
        monet_0Ccommand(pParam + 1, Length - 1);
        break;
        default:
        pf_log_raw(atel_log_ctl.io_protocol_en, "monet_0command Error Cmd(%02x).\r", cmd);
        break;
    }
}

void HandleRxCommand(void)
{
    pf_print_mdm_uart_rx(monet_data.iorxframe.cmd, &monet_data.iorxframe.data[0], monet_data.iorxframe.length);

    if ((monet_data.CANExistChargerNoExist == 1) && 
        (monet_data.bleShouldAdvertise == 0))
    {
        if (monet_data.iorxframe.cmd == IO_CMD_CHAR_BLE_SEND)
        {
            monet_data.iorxframe.cmd = IO_CMD_CHAR_CAN_SEND;
        }
    }

    switch(monet_data.iorxframe.cmd) {
    case IO_CMD_CHAR_BLE_SEND:
    #if BLE_DATA_CHANNEL_SUPPORT
        monet_bleScommand((&monet_data.iorxframe.data[0]) + 1, monet_data.iorxframe.length - 1, (uint16_t)monet_data.iorxframe.data[0]);
    #else
        monet_bleScommand((&monet_data.iorxframe.data[0]), monet_data.iorxframe.length, 0);
    #endif /* BLE_DATA_CHANNEL_SUPPORT */
        break;
    case IO_CMD_CHAR_BLE_CTRL:
        monet_bleCcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    #if SPI_CAN_CONTROLLER_EN
    case IO_CMD_CHAR_CAN_SEND:
    #if CAN_DATA_CHANNEL_SUPPORT
        monet_canScommand((&monet_data.iorxframe.data[0]) + 1, monet_data.iorxframe.length - 1, (uint16_t)monet_data.iorxframe.data[0]);
    #else
        monet_canScommand((&monet_data.iorxframe.data[0]), monet_data.iorxframe.length, 0);
    #endif /* CAN_DATA_CHANNEL_SUPPORT */
        break;
    #endif /* SPI_CAN_CONTROLLER_EN */
    case 'A':
        if (6 == monet_data.iorxframe.length) {
        } else {
            monet_requestAdc(monet_data.iorxframe.data[0]);
        }
        break;
    case 'D':
        monet_Dcommand(&monet_data.iorxframe.data[0]);
    case 'E':
        monet_Ecommand(&monet_data.iorxframe.data[0]);
        break;
    case 'G':
        monet_Gcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'I':
        monet_Icommand();
        break;
    case 'J':
        monet_Jcommand();
        break;
    case 'K':
        monet_Kcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'L':
        monet_Lcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'n':
        monet_ncommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'P':
        monet_Pcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'Q':
        monet_Qcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'S': // CAN BLE option
        monet_Scommand();
        break;
    case 'T':
        monet_Tcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'V':
        monet_Vcommand();
        break;
    case 'v':
        monet_vcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 'Z':
        monet_Zcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case 0xAA:
        monet_xAAcommand(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    case '0':
        monet_0command(&monet_data.iorxframe.data[0], monet_data.iorxframe.length);
        break;
    default:
        pf_log_raw(atel_log_ctl.error_en, "HandleRxCommand cmd(0x%x) not valid.\r", monet_data.iorxframe.cmd);
        break;
    }
}

#if 0
// int8_t queue_process(void)
// {
//     int8_t ret = 0;
//     uint32_t len = 0;
//     const uint8_t SEND_DELAY_COUNT = 10;
//     static uint8_t ble_nsend = 0;
//     static uint8_t recv_retry = 0;
//     static uint16_t recv_len = 0;
//     static uint16_t recv_offset = 0;
//     static uint16_t last_rx_count = 0;
//     uint16_t rx_count = 0;

// SEND_AGAIN:

//     rx_count = ring_buffer_num_get(&monet_data.rxQueueU1);
//     if (rx_count < BLE_DATA_SEND_BUFFER_CELL_LEN)
//     {
//         if (rx_count != last_rx_count)
//         {
//             last_rx_count = rx_count;
//             ble_nsend = 0;
//         }
//         else
//         {
//             ble_nsend++;
//         }
//     }
//     else
//     {
//         ble_nsend = SEND_DELAY_COUNT;
//     }

//     if (rx_count && (ble_nsend >= SEND_DELAY_COUNT))
//     {
//         ble_nsend = 0;

//         if (is_ble_send_queue_full() == 0)
//         {
//             memset(io_process_send_tmp_buf, 0, sizeof(io_process_send_tmp_buf));
//             len = ring_buffer_out_len(&monet_data.rxQueueU1, io_process_send_tmp_buf, BLE_DATA_SEND_BUFFER_CELL_LEN);
//             if (len)
//             {
//                 ble_send_data_push(io_process_send_tmp_buf, len);
//                 goto SEND_AGAIN;
//             }
//         }
//         else
//         {
//             ret = 1;
//         }
//     }

//     if (monet_data.uartTXDEnabled == 0)
//     {
//         return ret;
//     }

// RECV_AGAIN:
//     if (recv_retry)
//     {
//         while (recv_len)
//         {
//             if (pf_uart_tx_one(io_process_recv_tmp_buf[recv_offset]))
//             {
//                 recv_retry = 1;
//                 ret += 2;
//                 break;
//             }
//             else
//             {
//                 recv_len--;
//                 recv_offset++;

//                 if (recv_len == 0)
//                 {
//                     recv_offset = 0;
//                     recv_retry = 0;
//                 }
//             }
//         }
//     }
    
//     if (recv_retry == 0)
//     {
//         if (is_ble_recv_queue_empty() == 0)
//         {
//             recv_len = 0;
//             memset(io_process_recv_tmp_buf, 0, sizeof(io_process_recv_tmp_buf));
//             ble_recv_data_pop(io_process_recv_tmp_buf, &recv_len);

//             if (recv_len)
//             {
//                 recv_retry = 1;
//                 goto RECV_AGAIN;
//             }
//         }
//     }

//     return 0;
// }
#endif
