
/*
 * platform_hal_drv.c
 *
 *  Created on: Mar 3, 2020
 *      Author: Yangjie Gu
 */

/* Includes ------------------------------------------------------------------*/

#include "user.h"
#include "mp2762a_drv.h"

/* Private define ------------------------------------------------------------*/
typedef struct {
    uint32_t pin;
} pin_struct;

#define UART_TX_BUF_SIZE (1024 * 8) /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE (1024 * 8) /**< UART RX buffer size. */

/* External Variables --------------------------------------------------------*/

/* External Functions --------------------------------------------------------*/

/* Variables -----------------------------------------------------------------*/
static nrf_saadc_value_t saadc_buffer[2][ADC_CHANNELS_CNT];
volatile uint8_t adc_convert_over = 0;
uint8_t pf_adc_initialized = 0;

static const pin_struct gpin[GPIO_LAST] = {
    [GPIO_MCU_A].pin                = P107_GPIO13_ESP_TO_APP,
    [GPIO_MCU_B].pin                = P012_GPIO15_ESP32_TO_BLE,
    [GPIO_MCU_C].pin                = P015_BLE_MUX,
    [GPIO_MCU_D].pin                = P105_RED_LED1,
    [GPIO_MCU_E].pin                = P023_GREEN_LED1,
    [GPIO_G_SENSOR_INT].pin         = P104_G_SENSOR_INT,
    [GPIO_LIGTH_RESISTOR_INT].pin   = PIN_NOT_VALID,
    [GPIO_BAT_ADC_EN].pin           = P003_ADC_EN,
    [GPIO_MCU_WAKE_DEV].pin         = P020_BLE_WAKEUP_ESP,
    [GPIO_DEV_WAKE_MCU].pin         = P014_GPIO14_ESP_WAKEUP_BLE,
    [GPIO_V_DEV_EN].pin             = P106_DCDC_3V3_EN,
    [GPIO_DEV_EN].pin               = P022_BLE_ESP_EN,
    [GPIO_MCU_BAT_EN].pin           = PIN_NOT_VALID,
    [GPIO_BAT_CHARGE_EN].pin        = PIN_NOT_VALID,
    [GPIO_MCU_DEV_IO_EN].pin        = PIN_NOT_VALID,
    [GPIO_MCU_SLEEP_DEV].pin        = PIN_NOT_VALID, // P006_GPIO37_UART1_RXD, Nordic TX
    [GPIO_MCU_SLEEP_DEV1].pin       = PIN_NOT_VALID, // P008_GPIO4_UART1_TXD, Nordic RX
    [GPIO_CAN_UART_SWITCH_KEY].pin  = P007_SWITCH_KEY,
    [GPIO_SYS_PWR_EN].pin           = P011_VSYS_EN,
    [GPIO_CHRG_SLEEP_EN].pin        = P021_CHRG_SLEEP_EN,
    [GPIO_CAN_VREF].pin             = P101_CAN_VREF,
    [GPIO_CAN_INT].pin              = P025_CAN_INT,
    [GPIO_POWER_KEY].pin            = P103_POWER_KEY,
    [GPIO_TMP_ALARM].pin            = P019_TMP_ALARM,
    [GPIO_SOLAR_CHARGE_SWITCH].pin  = P013_SOLAR_CHARGER_SWITCH,
};

uint8_t ssadc_ready = 0;
APP_TIMER_DEF(m_timer_ssadc_ready);
uint8_t ssadc_timer_started = 0;

uint8_t button_pattern_timer_started = 0;
uint32_t button_pattern_timer_count = 0;
APP_TIMER_DEF(m_timer_button_pattern);

gpio_pattern_t gpio_pattern_array[GPIO_LAST] = {0};
uint8_t gpio_pattern_enabled = 0;
uint8_t gpio_pattern_timer_started = 0;
uint32_t gpio_pattern_timer_count = 0;
APP_TIMER_DEF(m_timer_gpio_pattern);

#define SPI_INSTANCE  2 /**< SPI instance index. */
static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(SPI_INSTANCE);  /**< SPI instance. */
static volatile bool spi_xfer_done;  /**< Flag used to indicate that SPI instance completed the transfer. */

volatile uint32_t gTimer = 0;
volatile uint32_t sys_time_ms = 0;
APP_TIMER_DEF(pf_systick_timer);

APP_TIMER_DEF(pf_systick_shipping_timer);

#define TWI_INSTANCE_ID0  0
#define TWI_INSTANCE_ID1  1
const nrf_drv_twi_t m_twi0 = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID0);	// TWI0, for pins BLE_I2C1_SCL and BLE_I2C1_SDA
const nrf_drv_twi_t m_twi1 = NRF_DRV_TWI_INSTANCE(TWI_INSTANCE_ID1);    // TWI1, for pins BLE_I2C2_SCL and BLE_I2C2_SDA

nrf_drv_wdt_channel_id m_pf_wdt_channel_id;

static uint8_t led_in_use = 0;

/* On Chip Peripheral's Drivers ----------------------------------------------*/

static void ssadc_ready_timer_handler(void * p_context)
{
    CRITICAL_REGION_ENTER();
    ssadc_timer_started = 0;
    ssadc_ready++;
    CRITICAL_REGION_EXIT();
}

void pf_adc_ready_timer_init(void)
{
    ret_code_t err_code;
    err_code = app_timer_create(&m_timer_ssadc_ready, APP_TIMER_MODE_SINGLE_SHOT, ssadc_ready_timer_handler);
    APP_ERROR_CHECK(err_code);
}

void pf_adc_ready_timer_start(void)
{
    ret_code_t err_code;

    if (ssadc_timer_started == 1)
    {
        return;
    }

    err_code = app_timer_start(m_timer_ssadc_ready, APP_TIMER_TICKS(SAADC_READY_TIMER_PERIOD_MS),NULL);
    APP_ERROR_CHECK(err_code);

    ssadc_timer_started = 1;
}

uint8_t pf_saadc_ready(void)
{
    return ssadc_ready;
}

void pf_saadc_ready_clear(void)
{
    ssadc_ready = 0;
}

static void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
    #if ADC_ARRAY_BUFFER_EN
    static int16_t adc_array[ADC_ARRAY_SIZE][ADC_CHANNELS_CNT] = {0};
    static uint32_t adc_count = 0;
    static uint32_t adc_index = 0;
    uint8_t i = 0;
    #endif /* ADC_ARRAY_BUFFER_EN */
    if (p_event->type == NRF_DRV_SAADC_EVT_DONE)
    {
        ret_code_t err_code;
        int16_t bub_val = 0;
        int16_t vin_val = 0;
        int16_t vsin_val = 0;

        err_code = nrf_drv_saadc_buffer_convert(p_event->data.done.p_buffer, ADC_CHANNELS_CNT);
        APP_ERROR_CHECK(err_code);

        bub_val = p_event->data.done.p_buffer[0];
        vin_val = p_event->data.done.p_buffer[1];
        vsin_val = p_event->data.done.p_buffer[2];

        #if ADC_ARRAY_BUFFER_EN
        adc_array[adc_index][0] = (bub_val > 0) ? bub_val : 0;
        adc_array[adc_index][1] = (vin_val > 0) ? vin_val : 0;
        adc_array[adc_index][2] = (vsin_val > 0) ? vsin_val : 0;
        adc_index++;
        adc_index %= ADC_ARRAY_SIZE;
        adc_count++;
        if (adc_count >= ADC_ARRAY_SIZE)
        {
            uint32_t AdcBackup = 0;
            uint32_t AdcMain = 0;
            uint32_t AdcSolar = 0;

            for (i = 0; i < ADC_ARRAY_SIZE; i++)
            {
                AdcBackup += adc_array[i][0];
                AdcMain += adc_array[i][1];
                AdcSolar += adc_array[i][2];
            }

           // monet_data.AdcBackup = AdcBackup / ADC_ARRAY_SIZE;
            monet_data.AdcBackupAccuracy = AdcBackup / ADC_ARRAY_SIZE;
            // monet_data.AdcMain = AdcMain / ADC_ARRAY_SIZE;         //0206 maybe this can be use.
            // monet_data.AdcSolar = AdcSolar / ADC_ARRAY_SIZE;
            // pf_log_raw(atel_log_ctl.platform_en, "<<<<<<<111(%d) (%d)  (%d).\r", AdcBackup,adc_count,monet_data.AdcBackupAccuracy);

        }
       // else    //0210 22 add.  two ways of data processing at the same time which for support xModem read vbat.
        #endif /* ADC_ARRAY_BUFFER_EN */
        {
            monet_data.AdcBackupRaw = (bub_val > 0)? bub_val: 0;
            monet_data.AdcMain = (vin_val > 0) ? vin_val : 0;
            monet_data.AdcSolar = (vsin_val > 0) ? vsin_val : 0;
        }
        CRITICAL_REGION_ENTER();
        adc_convert_over = 1;
        CRITICAL_REGION_EXIT();
    }
    // else if (p_event->type == NRFX_SAADC_EVT_CALIBRATEDONE)
    // {
    //     NRF_LOG_RAW_INFO(">>>>NRFX_SAADC_EVT_CALIBRATEDONE.\r");
    // }
}

// nrf_drv_saadc_calibrate_offset should be used after nrf_drv_saadc_init
static void pf_adc_calibration(void)
{
    uint16_t count = 0;
    ret_code_t err_code = 0;

    err_code = nrf_drv_saadc_calibrate_offset();

    if (err_code)
    {
        pf_log_raw(atel_log_ctl.platform_en, "pf_adc_calibration err_code(%d).\r", err_code);
    }

    if (nrf_drv_saadc_is_busy())
    {
        pf_delay_ms(1);
        count++;
        if (count > 5)
        {
            pf_log_raw(atel_log_ctl.platform_en, "pf_adc_calibration fail.\r");
            return;
        }
    }
}

void pf_adc_init(void)
{
    ret_code_t err_code;

    if (pf_adc_initialized) return;

    nrf_saadc_channel_config_t channel_config = NRF_DRV_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_AIN4);
    channel_config.acq_time = NRF_SAADC_ACQTIME_40US;

    err_code = nrf_drv_saadc_init(NULL, saadc_callback);
    APP_ERROR_CHECK(err_code);

    pf_adc_calibration();

    err_code = nrf_drv_saadc_channel_init(0, &channel_config);
    APP_ERROR_CHECK(err_code);

    channel_config.pin_p = (nrf_saadc_input_t)(NRF_SAADC_INPUT_AIN5);
    err_code = nrf_drv_saadc_channel_init(1, &channel_config);
    APP_ERROR_CHECK(err_code);

    channel_config.pin_p = (nrf_saadc_input_t)(NRF_SAADC_INPUT_AIN7);
    err_code = nrf_drv_saadc_channel_init(2, &channel_config);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(saadc_buffer[0], ADC_CHANNELS_CNT);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_drv_saadc_buffer_convert(saadc_buffer[1], ADC_CHANNELS_CNT);
    APP_ERROR_CHECK(err_code);

    pf_adc_initialized = 1;
}

void pf_adc_start(void)
{
    adc_convert_over = 0;
    nrf_drv_saadc_sample();
}

#define BAT_ADC_RAW_ARRAY_SIZE (60)
#define BAT_ADC_RAW_ARRAY_FRONT (10)
#define BAT_ADC_RAW_ARRAY_BACK (10)
static uint16_t bat_adc_array[BAT_ADC_RAW_ARRAY_SIZE] = {0};
static uint16_t bat_adc_in = 0;

static void swap(uint16_t array[], uint16_t i, uint16_t j)
{
    uint16_t temp = array[i];

    array[i] = array[j];

    array[j] = temp;
}

static void bubble_sort(uint16_t array[], uint16_t len)
{
    uint16_t i = 0;
    uint16_t j = 0;
    uint16_t exchange = 1;

    for (i = 0; (i < len) && exchange; i++)
    {
        exchange = 0;

        for (j = len - 1; j > i; j--)
        {
            if (array[j] < array[j - 1])
            {
                swap(array, j, j - 1);

                exchange = 1;
            }
        }
    }
}

void pf_adc_poll_finish(void)
{
    uint16_t count = 0;
    uint16_t i = 0;
    uint16_t len = 0;
    uint16_t front = 0;
    uint16_t back = 0;
    uint32_t tmp = 0;
    uint16_t bat_sort_array[BAT_ADC_RAW_ARRAY_SIZE] = {0};

    while (adc_convert_over == 0)
    {
        pf_delay_ms(1);
        count++;
        if (count > 5)
        {
            pf_log_raw(atel_log_ctl.platform_en, "pf_adc_poll_finish fail In(%d) Raw(0x%x: %u Mv).\r",
                       bat_adc_in,
                       monet_data.AdcBackupRaw,
                       (uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackupRaw));
            return;
        }
    }

    bat_adc_array[bat_adc_in % BAT_ADC_RAW_ARRAY_SIZE] = monet_data.AdcBackupRaw;
    bat_adc_in++;
    if (bat_adc_in >= (2 * BAT_ADC_RAW_ARRAY_SIZE))
    {
        bat_adc_in = BAT_ADC_RAW_ARRAY_SIZE;
    }

    if (bat_adc_in < BAT_ADC_RAW_ARRAY_SIZE)
    {
        len = bat_adc_in;
        front = bat_adc_in / 6;
        back = bat_adc_in / 6;
    }
    else
    {
        len = BAT_ADC_RAW_ARRAY_SIZE;
        front = BAT_ADC_RAW_ARRAY_FRONT;
        back = BAT_ADC_RAW_ARRAY_BACK;
    }

    memcpy(bat_sort_array, bat_adc_array, len * sizeof(bat_adc_array[0]));

    bubble_sort(bat_sort_array, len);

    for (i = front;
         i < (len - back);
         i++)
    {
        tmp += bat_sort_array[i];
    }
    monet_data.AdcBackup = tmp / (len - front - back);
    //NRF_LOG_RAW_INFO("Backup   %d %d %d %d %d %d \r",PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackupRaw),PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup),tmp,len,front,back);
  
    // pf_log_raw(atel_log_ctl.platform_en, "<<<<<<<222(%d) .\r", monet_data.AdcBackup);
    //Warnig:
    if (monet_data.AdcBackup >= 3527)
    {
        pf_log_raw(atel_log_ctl.platform_en, "<<High AdcBackup(0x%x: %u Mv).\r",
        monet_data.AdcBackup,(uint16_t)PF_ADC_RAW_TO_BATT_MV(monet_data.AdcBackup));
        monet_data.AdcBackup = 3520; // after transfer,the vat will be about 8.48v
    }
}

void pf_adc_deinit(void)
{
    if (0 == pf_adc_initialized) return;
    nrf_drv_saadc_uninit();
    pf_adc_initialized = 0;
}

static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    static uint32_t can_int_count = 0;
    // NRF_LOG_RAW_INFO(">>>>gpiote_event_handler(%d:%d).\r", pin, action);
    switch (pin)
    {
        case P107_GPIO13_ESP_TO_APP:
        NRF_LOG_RAW_INFO(">>>>P107_GPIO13_ESP_TO_APP.\r");
            break;
        
        case P012_GPIO15_ESP32_TO_BLE:
        NRF_LOG_RAW_INFO(">>>>P012_GPIO15_ESP32_TO_BLE.\r");
            break;

        case P014_GPIO14_ESP_WAKEUP_BLE:
        NRF_LOG_RAW_INFO(">>>>P014_GPIO14_ESP_WAKEUP_BLE.\r");
            break;
        
        case P104_G_SENSOR_INT:
        NRF_LOG_RAW_INFO(">>>>P104_G_SENSOR_INT.\r");
            break;
        
        case P007_SWITCH_KEY:
        NRF_LOG_RAW_INFO(">>>>P007_SWITCH_KEY.\r");
            break;
        
        case P025_CAN_INT:
        can_int_count++;
        if (can_int_count >= 10)
        {
            can_int_count = 0;
            NRF_LOG_RAW_INFO(">>>>P025_CAN_INT.\r");
        }
            break;
        
        case P103_POWER_KEY:
        NRF_LOG_RAW_INFO(">>>>P103_POWER_KEY.\r");
            break;
        
        case P019_TMP_ALARM:
        NRF_LOG_RAW_INFO(">>>>P019_TMP_ALARM.\r");
            break;

        default:
            break;
    }
}

int8_t pf_gpio_cfg(uint32_t index, atel_gpio_cfg_t cfg)
{
    nrfx_err_t err_code = NRF_SUCCESS;
    nrf_drv_gpiote_in_config_t config = NRFX_GPIOTE_CONFIG_IN_SENSE_LOTOHI(false);
    nrf_gpio_pin_dir_t dir = NRF_GPIO_PIN_DIR_INPUT;
    nrf_gpio_pin_input_t input = NRF_GPIO_PIN_INPUT_CONNECT;
    nrf_gpio_pin_drive_t drive = NRF_GPIO_PIN_S0S1;
    nrf_gpio_pin_sense_t sense = NRF_GPIO_PIN_NOSENSE;
    nrf_gpio_pin_pull_t pull = NRF_GPIO_PIN_NOPULL;

    if ((gpin[index].pin == PIN_NOT_VALID) || (gpin[index].pin == GPIO_MCU_BAT_EN))
    {
        pf_log_raw(atel_log_ctl.platform_en, "pf_gpio_cfg(index%d:%x) no need to config.\r", index, gpin[index].pin);
        return 1;
    }

    if (cfg.pull == ATEL_GPIO_PULLUP) {
        pull = NRF_GPIO_PIN_PULLUP;
    }
    else if (cfg.pull == ATEL_GPIO_PULLDOWN) {
        pull = NRF_GPIO_PIN_PULLDOWN;
    }

    if (cfg.func == ATEL_GPIO_FUNC_INT) {
        config.pull = pull;

        if (!nrfx_gpiote_is_init()) {
            if (nrfx_gpiote_init() != NRF_SUCCESS) {
                pf_log_raw(atel_log_ctl.error_en, "pf_gpio_cfg gpiote_init fail.\r");
            }
            return -1;
        }

        if (cfg.sense == ATEL_GPIO_SENSE_TOGGLE) {
            config.sense = NRF_GPIOTE_POLARITY_TOGGLE;
        }
        else if (cfg.sense == ATEL_GPIO_SENSE_HITOLOW)
        {
            config.sense = NRF_GPIOTE_POLARITY_HITOLO;
        }

        pf_log_raw(atel_log_ctl.platform_en, "pf_gpio_cfg index(%d:%d) pull(%d) sense(%d).\r", index, gpin[index].pin, pull, config.sense);

        err_code = nrfx_gpiote_in_init(gpin[index].pin, &config, gpiote_event_handler);
        if (err_code != NRF_SUCCESS) {
            return err_code;
        }
        nrfx_gpiote_in_event_enable(gpin[index].pin, true);
    }
    else
    {
        nrf_gpio_cfg_default(gpin[index].pin);

        if ((cfg.func == ATEL_GPIO_FUNC_OUT) ||
            (cfg.func == ATEL_GPIO_FUNC_OD)) {
            dir = NRF_GPIO_PIN_DIR_OUTPUT;
            if (cfg.func == ATEL_GPIO_FUNC_OD) {
                drive = NRF_GPIO_PIN_S0D1;
            }

            input = NRF_GPIO_PIN_INPUT_DISCONNECT;

            // WARNING: not using the setting passed down
            if ((index == GPIO_MCU_E) || (index == GPIO_MCU_D))
            {
                pull = NRF_GPIO_PIN_NOPULL;
                drive = NRF_GPIO_PIN_S0H1;
            }
	
            nrf_gpio_pin_clear(gpin[index].pin);
        }

        pf_log_raw(atel_log_ctl.platform_en, "pf_gpio_cfg index(%d:%d) out(%d) pull(%d) drive(%d).\r", index, gpin[index].pin, dir, pull, drive);

        nrf_gpio_cfg(gpin[index].pin, dir, input, pull, drive, sense);
    }

    return 0;
}

int8_t pf_gpio_write(uint32_t index, uint32_t value)
{
    if (gpin[index].pin == PIN_NOT_VALID)
    {
        return 0;
    }
    else if (nrf_gpio_pin_dir_get(gpin[index].pin) == NRF_GPIO_PIN_DIR_OUTPUT)
    {
        // pf_log_raw(atel_log_ctl.platform_en, "pf_gpio_write index(%d:%d) value(%d).", index, gpin[index].pin, value);
        if (value == 0)
        {
            nrf_gpio_pin_clear(gpin[index].pin);
        }
        else
        {
            nrf_gpio_pin_set(gpin[index].pin);
        }
    }
    else {
        return -1;
    }

    return 0;
}

int8_t pf_gpio_toggle(uint32_t index)
{
    if (gpin[index].pin == PIN_NOT_VALID)
    {
        return 0;
    }
    else if (nrf_gpio_pin_dir_get(gpin[index].pin) == NRF_GPIO_PIN_DIR_OUTPUT) {
        nrf_gpio_pin_toggle(gpin[index].pin);
    }
    else {
        return -1;
    }

    return 0;
}

int32_t pf_gpio_read(uint32_t index)
{
    if (gpin[index].pin == PIN_NOT_VALID)
    {
        return -1;
    }
    else if (nrf_gpio_pin_dir_get(gpin[index].pin) == NRF_GPIO_PIN_DIR_OUTPUT) {
        return nrf_gpio_pin_out_read(gpin[index].pin);
    }
    else {
        return nrf_gpio_pin_read(gpin[index].pin);
    }
}

static void button_pattern_timer_handler(void * p_context)
{
    button_pattern_timer_count++;
}

void pf_button_pattern_timer_init(void)
{
    ret_code_t err_code;
    err_code = app_timer_create(&m_timer_button_pattern, APP_TIMER_MODE_REPEATED, button_pattern_timer_handler);
    APP_ERROR_CHECK(err_code);
}

void pf_button_pattern_timer_start(void)
{
    ret_code_t err_code;
    if (button_pattern_timer_started)
    {
        return;
    }
    pf_log_raw(atel_log_ctl.normal_en, "pf_button_pattern_timer_start\r");
    err_code = app_timer_start(m_timer_button_pattern, APP_TIMER_TICKS(BUTTON_PATTERN_TIMER_PERIOD_MS),NULL);
    APP_ERROR_CHECK(err_code);
    button_pattern_timer_started = 1;
    button_pattern_timer_count = 0;
}

void pf_button_pattern_timer_stop(void)
{
    ret_code_t err_code;
    if (button_pattern_timer_started == 0)
    {
        return;
    }
    pf_log_raw(atel_log_ctl.normal_en, "pf_button_pattern_timer_stop\r");
    err_code = app_timer_stop(m_timer_button_pattern);
    APP_ERROR_CHECK(err_code);
    button_pattern_timer_started = 0;
}

uint32_t pf_button_pattern_timer_get_ms(void)
{
    return button_pattern_timer_count * BUTTON_PATTERN_TIMER_PERIOD_MS;
}

void pf_device_power_normal_indicate(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_power_on_indicate.\r");

    pf_status_led_in_use(1);

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = LED_FLASH_TOGGLE_MS;
    gpio_pattern.dur_ms2 = LED_FLASH_TOGGLE_MS;
    gpio_pattern.total_ms = GREEN_LED_FLASH_TIME_MS;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
}

#if DEVICE_BUTTON_FOR_FACTORY
void pf_device_button_for_factory_init(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_button_for_factory_init.\r");

    pf_status_led_in_use(1);

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = 0;
    gpio_pattern.dur_ms2 = 0;
    gpio_pattern.total_ms = 5000;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
}

void pf_device_button_for_factory_enter(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_button_for_factory_enter.\r");

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    pf_status_led_in_use(0);

    // if (monet_data.cameraPowerOn == 0)
    {
        // When button pressed for waking up device, turn Camera off then power on it
        turnOffCamera();
        #if !CAMERA_POWER_OFF_DELAY_EN
        ble_send_timer_stop();
        MCU_TurnOff_Camera();
        #endif /* CAMERA_POWER_OFF_DELAY_EN */

        monet_data.cameraPowerOnPending = 1;

        // When button pressed for waking up device, start advertising
        if (monet_data.bleShouldAdvertise == 1)
        {
            pf_ble_adv_connection_reset();
            if (reset_info.reset_from_shipping_mode_donot_adv_untill_pair == 0)
            {
                monet_data.to_advertising = 1;
            }
        }
    }
}
#endif /* DEVICE_BUTTON_FOR_FACTORY */

#if DEVICE_POWER_KEY_SUPPORT
void pf_device_power_off_mode_init(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_power_off_mode_init.\r");

    pf_status_led_in_use(1);

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = 0;
    gpio_pattern.dur_ms2 = 0;
    gpio_pattern.total_ms = 0x7fffffff;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
}

void pf_device_power_off_mode_enter(void)
{
    pf_log_raw(atel_log_ctl.core_en, "pf_device_power_off_mode_enter.\r");
    pf_camera_pwr_init();
    pf_camera_pwr_ctrl(false);
    nrf_gpio_pin_clear(P011_VSYS_EN);
    nrf_gpio_cfg_output(P011_VSYS_EN);
    DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_POWEROFF;
    DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
    //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_INVALID);
    while (1);
}
#endif /* DEVICE_POWER_KEY_SUPPORT */

void pf_device_unpairing_mode_enter(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_unpairing_mode_enter.\r");

    reset_info.ble_peripheral_paired = 0;

    if (reset_info.reset_from_shipping_mode == 1)
        {
            //reset_info.reset_from_shipping_mode = 0;     // 0214 22 wfy add.Here should clear the flag,otherwise maybe casued a problem during the device enter shipping mode automatically.
            reset_info.reset_from_shipping_mode_donot_adv_untill_pair = 1;
            pf_log_raw(atel_log_ctl.error_en, "reset_from_shipping_mode_donot_adv_untill_pair.\r");
        }
    pf_ble_delete_bonds();

    CRITICAL_REGION_ENTER();
    pf_status_led_in_use(1);
    CRITICAL_REGION_EXIT();

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = 100;
    gpio_pattern.dur_ms2 = 100;
    gpio_pattern.total_ms = 0x7fffffff;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
}

void pf_device_unpairing_mode_exit(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_unpairing_mode_exit.\r");

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    pf_status_led_in_use(0);
}

void pf_device_pairing_mode_enter(void)
{
    gpio_pattern_t gpio_pattern = {0};
    CRITICAL_REGION_ENTER();
    pf_status_led_in_use(1);
    CRITICAL_REGION_EXIT();

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = 100;
    gpio_pattern.dur_ms2 = 100;
    gpio_pattern.total_ms = 0x7fffffff;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
}

void pf_device_pairing_fail(void)
{
    if (reset_info.reset_from_shipping_mode == 1)
    {
        //reset_info.reset_from_shipping_mode = 0;     // 0214 22 wfy add.Here should clear the flag,otherwise maybe casued a problem during the device enter shipping mode automatically.
        CRITICAL_REGION_ENTER();
        reset_info.reset_from_shipping_mode_donot_adv_untill_pair = 1;
        CRITICAL_REGION_EXIT();
        pf_log_raw(atel_log_ctl.error_en, "reset_from_shipping_mode_donot_adv_untill_pair.\r");
    }
    gpio_pattern_t gpio_pattern = {0};

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    CRITICAL_REGION_ENTER();
    pf_status_led_in_use(0);
    CRITICAL_REGION_EXIT();
}

void pf_device_pairing_success(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_pairing_success\r");

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = 1000;
    gpio_pattern.dur_ms2 = 1000;
    gpio_pattern.total_ms = 10000;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
}

void pf_device_upgrading_mode_enter(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_upgrading_mode_enter\r");

    pf_status_led_in_use(1);

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = 100;
    gpio_pattern.dur_ms2 = 100;
    gpio_pattern.total_ms = 0x7fffffff;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    if (monet_data.cameraPowerOn != 0)
    {
        monet_data.cameraPowerOnDelay = 0;
        monet_data.upgrading_mode_enter = 0;
    }
    else
    {
        monet_data.upgrading_mode_enter = 1;
    }
    monet_data.in_upgrading_mode = 1;
    monet_data.in_upgrading_mode_time = 0;
}

void pf_device_upgrading_fail(void)
{
    gpio_pattern_t gpio_pattern = {0};

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 0;
    gpio_pattern.state_e = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    pf_status_led_in_use(0);

    monet_data.in_upgrading_mode = 0;
    monet_data.cameraPowerOnDelay = CAMERA_POWER_ON_DELAY_MS;
}

void pf_device_upgrading_success(void)
{
    gpio_pattern_t gpio_pattern = {0};

    pf_log_raw(atel_log_ctl.core_en, "pf_device_upgrading_success\r");

    gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern.pin = GPIO_MCU_D;
    gpio_pattern.state_s = 1;
    gpio_pattern.state_e = 0;
    gpio_pattern.dur_ms1 = 1000;
    gpio_pattern.dur_ms2 = 1000;
    gpio_pattern.total_ms = 10000;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    gpio_pattern.pin = GPIO_MCU_E;
    gpio_pattern.state_s = 0;
    pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

    monet_data.in_upgrading_mode = 0;
    monet_data.cameraPowerOnDelay = CAMERA_POWER_ON_DELAY_MS;
}

//0209 22 add 
//check the conditions to indicate extern power charging LED status 
static bool extern_charging_led_condition_check(void)
{
    bool case1 = (only_solar_power() != true && monet_data.device_in_shipping_mode == 1);
    bool case2 = (only_solar_power() != true && monet_data.appActive != 1);
    bool case3 = is_charger_power_on();
    if ((case1 == true && case3 == true) || (case2 == true && case3 == true))
    {
        return true;
    }
    else
    {
        return false;
    }
}
void pf_device_extern_power_charging_process(void)
{
    static uint8_t charge_resume_count = CHANGER_RESUME_S;
    gpio_pattern_t gpio_pattern = {0};
    pf_log_raw(atel_log_ctl.core_en,"pf_device_extern_charging_process (%d :%d :%d) \r",
                                    power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume,
                                    power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button,
                                    power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button);
    if (extern_charging_led_condition_check() == true) // charger status only be displayed when main power and the device in sleep.                                                                                                                                                    
    {       
        if (power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_button == 1 || power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_change_in_pairing_button == 1)
        {
            return;
        }  
        if (power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume)  
        {
            charge_resume_count --;
            pf_log_raw(atel_log_ctl.core_en,"process_charge_resume_count(%d) \r",charge_resume_count);
            if (charge_resume_count == 0)
            {
                power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].led_state_resume = 0;
                goto LED_CHARGER_PROCESS;
            }
        }
        else
        {
LED_CHARGER_PROCESS:            
            gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
            gpio_pattern.pin = GPIO_MCU_D;
            gpio_pattern.state_s = 1;
            gpio_pattern.state_e = 0;
            gpio_pattern.dur_ms1 = 500;
            gpio_pattern.dur_ms2 = 500;
            gpio_pattern.total_ms = 1000;
            pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);

            gpio_pattern.pin = GPIO_MCU_E;
            gpio_pattern.state_s = 0;
            gpio_pattern.state_e = 0;
            gpio_pattern.dur_ms1 = 0;
            gpio_pattern.dur_ms2 = 0;
            gpio_pattern.total_ms = 0;
            pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
            power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].charging_process = 1;
            power_state.led_state[EXTERN_POWER_CHARGING_FULL].charging_process = 0 ;
            charge_resume_count = CHANGER_RESUME_S;
        } 
    }
}

void pf_device_extern_power_charging_full(void)
{
    gpio_pattern_t gpio_pattern = {0};
    static uint8_t charger_resume_count = CHANGER_RESUME_S;
    pf_log_raw(atel_log_ctl.core_en,"pf_device_extern_charging_FULL (%d :%d :%d) \r",
                                    power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume,
                                    power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button,
                                    power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button);
    if (only_solar_power() != true)  // only when 12v still connected, the green led will hold the state.
    {
        if (power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_button == 1 || 
            power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_change_in_pairing_button == 1)
        {
            return;
        }  
        if (power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume)
        {
            charger_resume_count --;
            pf_log_raw(atel_log_ctl.core_en,"full_charge_resume_count(%d) \r",charger_resume_count);
            if (charger_resume_count == 0)
            {
                power_state.led_state[EXTERN_POWER_CHARGING_FULL].led_state_resume = 0;
                goto LED_CHARGER_PROCESS;
            }
        }
        else
        {
 LED_CHARGER_PROCESS:
            gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
            gpio_pattern.pin = GPIO_MCU_D;
            gpio_pattern.state_s = 0;
            gpio_pattern.state_e = 0;
            pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin,GPIO_PATTERN_PARAM_MAX_BYTE);

            gpio_pattern.pin = GPIO_MCU_E;
            gpio_pattern.state_s = 1;
            gpio_pattern.state_e = 0;
            // gpio_pattern.dur_ms1 = 0xffffffff;
            // gpio_pattern.dur_ms2 = 0;
            // gpio_pattern.total_ms = 0xffffffff;

            gpio_pattern.dur_ms1 = 2000;
            gpio_pattern.dur_ms2 = 0;
            gpio_pattern.total_ms = 2000;
         
            pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
            charger_resume_count = CHANGER_RESUME_S;
            power_state.led_state[EXTERN_POWER_CHARGING_PROCESS].charging_process = 0;
            power_state.led_state[EXTERN_POWER_CHARGING_FULL].charging_process = 1;

        }
    }

}

// // device_mode: 0 represent shipping mode. 1 represent sleep mode.
void pf_device_extern_power_charging_exit(void)
{
        gpio_pattern_t gpio_pattern = {0};
        pf_log_raw(atel_log_ctl.core_en,"pf_device_extern_charging_full and extern power pull out \r");
        gpio_pattern.state = GPIO_PATTERN_STATE_CHANGED;
        gpio_pattern.pin = GPIO_MCU_D;
        gpio_pattern.state_s = 0;
        gpio_pattern.state_e = 0;
        pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin,GPIO_PATTERN_PARAM_MAX_BYTE);

        gpio_pattern.pin = GPIO_MCU_E;
        gpio_pattern.state_s = 0;
        gpio_pattern.state_e = 0;
        pf_gpio_pattern_set((uint8_t *)&gpio_pattern.pin, GPIO_PATTERN_PARAM_MAX_BYTE);
       // monet_data.in_extern_power_charging_process = 0;
}




static void gpio_pattern_timer_handler(void * p_context)
{
    CRITICAL_REGION_ENTER();
    gpio_pattern_timer_count++;
    CRITICAL_REGION_EXIT();
}

void pf_gpio_pattern_timer_init(void)
{
    ret_code_t err_code;
    err_code = app_timer_create(&m_timer_gpio_pattern, APP_TIMER_MODE_REPEATED, gpio_pattern_timer_handler);
    APP_ERROR_CHECK(err_code);
}

void gpio_pattern_timer_start(void)
{
    ret_code_t err_code;
    if (gpio_pattern_timer_started)
    {
        return;
    }
    pf_log_raw(atel_log_ctl.normal_en, "gpio_pattern_timer_start\r");
    err_code = app_timer_start(m_timer_gpio_pattern, APP_TIMER_TICKS(GPIO_PATTERN_TIMER_PERIOD_MS),NULL);
    APP_ERROR_CHECK(err_code);
    CRITICAL_REGION_ENTER();
    gpio_pattern_timer_started = 1;
    CRITICAL_REGION_EXIT();
}

void gpio_pattern_timer_stop(void)
{
    ret_code_t err_code;
    if (gpio_pattern_timer_started == 0)
    {
        return;
    }
    pf_log_raw(atel_log_ctl.normal_en, "gpio_pattern_timer_stop\r");
    err_code = app_timer_stop(m_timer_gpio_pattern);
    APP_ERROR_CHECK(err_code);
    gpio_pattern_timer_started = 0;
}

void pf_gpio_pattern_set(uint8_t *data, uint8_t len)
{
    uint8_t pin = data[0];
    uint8_t state_s = data[1];

    pf_log_raw(atel_log_ctl.normal_en, "pf_gpio_pattern_set:\r");
    printf_hex_and_char(data, len);

    CRITICAL_REGION_ENTER();
    gpio_pattern_enabled = 1;
    memset(gpio_pattern_array[pin].raw_param, 0, GPIO_PATTERN_PARAM_MAX_BYTE);
    memcpy(gpio_pattern_array[pin].raw_param, data, len);
    gpio_pattern_array[pin].state = GPIO_PATTERN_STATE_CHANGED;
    gpio_pattern_array[pin].state_s = state_s;
    CRITICAL_REGION_EXIT();
    gpio_pattern_timer_start();
    pf_gpio_write(pin, (state_s > 0) ? 1 : 0);
}

void pf_gpio_pattern_process(uint32_t ms)
{
    uint16_t i = 0;
    uint8_t enabled = 0;
    static uint8_t led_on = 0;

    if (gpio_pattern_timer_count == 0)
    {
        return;
    }
    else
    {
        gpio_pattern_timer_count--;
    }

    if (gpio_pattern_enabled == 0)
    {
        pf_status_led_in_use(0);
        gpio_pattern_timer_stop();
        return;
    }
    else
    {
        // WARNING: Turn on the power of Camera here
        // if (((gpio_pattern_array[GPIO_MCU_C].state == GPIO_PATTERN_STATE_CHANGED) && (gpio_pattern_array[GPIO_MCU_C].state_s != 0)) || 
        //     (led_on != 0))
        // {
            pf_gpio_write(GPIO_V_DEV_EN, 0);
        // }
    }

    for (i = 0; i < GPIO_LAST; i++)
    {
        if (gpio_pattern_array[i].state == GPIO_PATTERN_STATE_CHANGED)
        {
            uint8_t pin, state_s, state_e;
            uint32_t dur_ms1, dur_ms2, total_ms;

            pin = gpio_pattern_array[i].raw_param[0];
            state_s = gpio_pattern_array[i].raw_param[1];
            state_e = gpio_pattern_array[i].raw_param[2];
            memcpy(&dur_ms1, &(gpio_pattern_array[i].raw_param[3]), sizeof(dur_ms1));
            memcpy(&dur_ms2, &(gpio_pattern_array[i].raw_param[3 + sizeof(dur_ms1)]), sizeof(dur_ms2));
            memcpy(&total_ms, &(gpio_pattern_array[i].raw_param[3 + sizeof(dur_ms1) + sizeof(dur_ms2)]), sizeof(total_ms));

            gpio_pattern_array[i].pin = pin;
            gpio_pattern_array[i].state_s = state_s;
            gpio_pattern_array[i].state_e = state_e;
            gpio_pattern_array[i].dur_ms1 = dur_ms1;
            gpio_pattern_array[i].dur_ms2 = dur_ms2;
            gpio_pattern_array[i].total_ms = total_ms;

            gpio_pattern_array[i].deb_ms1 = dur_ms1;
            gpio_pattern_array[i].deb_ms2 = dur_ms2;

            if ((dur_ms1 == 0) && (dur_ms2 == 0) && (total_ms == 0))
            {
                // if ((pin == GPIO_MCU_C) && (state_s)) // LED Pin
                // {
                //     led_on = 1;
                //     gpio_pattern_array[i].state = GPIO_PATTERN_STATE_ENABLED;
                //     enabled = 1;
                // }
                // else
                {
                    gpio_pattern_array[i].state = GPIO_PATTERN_STATE_DISABLE;
                }

                // if((pin == GPIO_MCU_C) && (state_s == 0))
                // {
                //     led_on = 0;
                // }
            }
            else
            {
                gpio_pattern_array[i].state = GPIO_PATTERN_STATE_ENABLED;
                enabled = 1;
            }
            pf_gpio_write(pin, (state_s > 0) ? 1 : 0);
            pf_log_raw(atel_log_ctl.normal_en, "GPIO pattern change: P(%d) S(%d:%d) ", pin, state_s, state_e);
            pf_log_raw(atel_log_ctl.normal_en, "D1(%d) D2(%d) T(%d)\r", dur_ms1, dur_ms2, total_ms);
        }
        
        if (gpio_pattern_array[i].state == GPIO_PATTERN_STATE_ENABLED)
        {
            if (gpio_pattern_array[i].deb_ms1)
            {
                if (gpio_pattern_array[i].deb_ms1 > ms)
                {
                    gpio_pattern_array[i].deb_ms1 -= ms;
                    pf_gpio_write(gpio_pattern_array[i].pin, (gpio_pattern_array[i].state_s > 0) ? 1 : 0);
                }
                else
                {
                    gpio_pattern_array[i].deb_ms1 = 0;
                    gpio_pattern_array[i].deb_ms2 = gpio_pattern_array[i].dur_ms2;
                    if (gpio_pattern_array[i].deb_ms2)
                    {
                        pf_gpio_write(gpio_pattern_array[i].pin, (gpio_pattern_array[i].state_s > 0) ? 0 : 1);
                        // pf_log_raw(atel_log_ctl.normal_en, "pf_gpio_pattern_process write(%d:%d)\r", gpio_pattern_array[i].pin, (gpio_pattern_array[i].state_s > 0) ? 0 : 1);
                    }
                }
            }
            else if (gpio_pattern_array[i].deb_ms2)
            {
                if (gpio_pattern_array[i].deb_ms2 > ms)
                {
                    gpio_pattern_array[i].deb_ms2 -= ms;
                    pf_gpio_write(gpio_pattern_array[i].pin, (gpio_pattern_array[i].state_s > 0) ? 0 : 1);
                }
                else
                {
                    gpio_pattern_array[i].deb_ms1 = gpio_pattern_array[i].dur_ms1;
                    gpio_pattern_array[i].deb_ms2 = 0;
                    if (gpio_pattern_array[i].deb_ms1)
                    {
                        pf_gpio_write(gpio_pattern_array[i].pin, (gpio_pattern_array[i].state_s > 0) ? 1 : 0);
                        // pf_log_raw(atel_log_ctl.normal_en, "pf_gpio_pattern_process write(%d:%d)\r", gpio_pattern_array[i].pin, (gpio_pattern_array[i].state_s > 0) ? 1 : 0);
                    }
                }
            }

            if (gpio_pattern_array[i].total_ms)
            {
                if (gpio_pattern_array[i].total_ms > ms)
                {
                    if (gpio_pattern_array[i].total_ms == 0xffffffff)   //if the total_ms is biggest value, hold the led state.  0209 22 add
                    {
                        gpio_pattern_array[i].total_ms += ms;
                    }
                    gpio_pattern_array[i].total_ms -= ms;
                    enabled = 1;
                }
                else
                {
                    gpio_pattern_array[i].total_ms = 0;
                    gpio_pattern_array[i].state = GPIO_PATTERN_STATE_DISABLE;
                    pf_gpio_write(gpio_pattern_array[i].pin, (gpio_pattern_array[i].state_e > 0) ? 1 : 0);
                    pf_log_raw(atel_log_ctl.normal_en, "GPIO pattern stop(%d)\r", i);
                }
            }
        }
    }

    if ((enabled == 1) || (led_on))
    {
        gpio_pattern_enabled = 1;
        gpio_pattern_timer_start();
    }
    else
    {
        gpio_pattern_enabled = 0;
        pf_status_led_in_use(0);
        gpio_pattern_timer_stop();

        // WARNING: Turn off the power of Camera here
        if (monet_data.cameraPowerOn == 0)
        {
            pf_gpio_write(GPIO_V_DEV_EN, 1);
        }
    }
}

/**
 * @brief SPI user event handler.
 * @param event
 */
void spi_event_handler(nrf_drv_spi_evt_t const * p_event,
                       void *                    p_context)
{
    CRITICAL_REGION_ENTER();
    spi_xfer_done = true;
    CRITICAL_REGION_EXIT();
    // NRF_LOG_INFO("Transfer completed.");
}

void pf_spi_init(void)
{
    nrf_drv_spi_config_t spi_config = NRF_DRV_SPI_DEFAULT_CONFIG;
    spi_config.frequency = NRF_DRV_SPI_FREQ_8M;
    spi_config.ss_pin   = P026_BLE_SPI1_CS_FAKE;
    spi_config.miso_pin = P005_BLE_SPI1_MISO;
    spi_config.mosi_pin = P004_BLE_SPI1_MOSI;
    spi_config.sck_pin  = P027_BLE_SPI1_SCK;
    APP_ERROR_CHECK(nrf_drv_spi_init(&spi, &spi_config, spi_event_handler, NULL));

    nrf_gpio_pin_set(P026_BLE_SPI1_CS);
    nrf_gpio_cfg_output(P026_BLE_SPI1_CS);
}

void pf_spi_uninit(void)
{
    nrf_drv_spi_uninit(&spi);
    nrf_gpio_cfg_default(P026_BLE_SPI1_CS);
}

uint32_t pf_spi_transmit_receive(uint8_t* p_tx_buf, uint8_t tx_length, uint8_t* p_rx_buf, uint8_t rx_length)
{
    uint32_t ret = 0;
    spi_xfer_done = false;

    ret = nrf_drv_spi_transfer(&spi, p_tx_buf, tx_length, p_rx_buf, rx_length);

    if (ret)
    {
        return ret;
    }

    while (!spi_xfer_done)
    {
        __WFE();
    }

    return ret;
}

static void timer_systick_handler(void * p_context)
{
    ret_code_t ret = 0;
    UNUSED_PARAMETER(p_context);
    CRITICAL_REGION_ENTER();
    gTimer++;
    CRITICAL_REGION_EXIT();
    sys_time_ms += monet_data.sysTickUnit;

    #if TIME_UNIT_CHANGE_WHEN_SLEEP
    if (1 == monet_data.SleepStateChange)
    {
        monet_data.SleepStateChange = 2;
    }

    if (SLEEP_OFF == monet_data.SleepState)
    {
        ret = app_timer_start(pf_systick_timer, APP_TIMER_TICKS(TIME_UNIT_IN_SLEEP_NORMAL), NULL);
        APP_ERROR_CHECK(ret);
    }
    else
    {
        ret = app_timer_start(pf_systick_timer, APP_TIMER_TICKS(TIME_UNIT_IN_SLEEP_HIBERNATION), NULL);
        APP_ERROR_CHECK(ret);
    }
    #else
    ret = app_timer_start(pf_systick_timer, APP_TIMER_TICKS(TIME_UNIT_IN_SLEEP_NORMAL), NULL);
    APP_ERROR_CHECK(ret);
    #endif /* TIME_UNIT_CHANGE_WHEN_SLEEP */
}

void pf_systick_start(uint32_t period_ms)
{
    ret_code_t ret;

    ret = app_timer_create(&pf_systick_timer, APP_TIMER_MODE_SINGLE_SHOT, timer_systick_handler);
    APP_ERROR_CHECK(ret);

    ret = app_timer_start(pf_systick_timer, APP_TIMER_TICKS(period_ms), NULL);
    APP_ERROR_CHECK(ret);
}

void pf_systick_stop(void)
{
    app_timer_stop(pf_systick_timer);
}

void pf_systick_change(void)
{
#if TIME_UNIT_CHANGE_WHEN_SLEEP
    if (2 == monet_data.SleepStateChange)
    {
        if (gTimer)
        {
            atel_timerTickHandler(monet_data.sysTickUnit);
        }

        if (SLEEP_OFF == monet_data.SleepState)
        {
            if (monet_data.sysTickUnit != TIME_UNIT)
            {
                monet_data.sysTickUnit = TIME_UNIT;
            }
        }
        else
        {
            if (monet_data.sysTickUnit != TIME_UNIT_IN_SLEEP_HIBERNATION)
            {
                monet_data.sysTickUnit = TIME_UNIT_IN_SLEEP_HIBERNATION;
            }
        }

        monet_data.SleepStateChange = 0;

        NRF_LOG_RAW_INFO("========pf_systick_change SC(%d)\r", monet_data.SleepStateChange);
        NRF_LOG_FLUSH();
    }
#endif /* TIME_UNIT_CHANGE_WHEN_SLEEP */
}

static void timer_systick_shipping_handler(void * p_context)
{
   // ret_code_t ret = 0;
    
    device_shipping_mode.count++;

    // ret = app_timer_start(pf_systick_shipping_timer, APP_TIMER_TICKS(TIME_UNIT_IN_SHIPPING_MODE), NULL);
    // APP_ERROR_CHECK(ret);
}

void pf_systick_shippping_start(void)
{
    ret_code_t ret;

    ret = app_timer_create(&pf_systick_shipping_timer, APP_TIMER_MODE_REPEATED, timer_systick_shipping_handler);
    APP_ERROR_CHECK(ret);

    ret = app_timer_start(pf_systick_shipping_timer, APP_TIMER_TICKS(TIME_UNIT_IN_SHIPPING_MODE), NULL);
    APP_ERROR_CHECK(ret);
}

void pf_systick_shipping_stop(void)
{
    app_timer_stop(pf_systick_shipping_timer);
    device_shipping_mode.count = 0;
}

uint32_t pf_systime_get(void)
{
    return sys_time_ms;
}

// Nordic TWI is I2C compatible two-wire interface
static uint8_t i2c0_inited = 0;

void pf_i2c_0_init(void)
{
    ret_code_t err_code;

    if (i2c0_inited == 1)
    {
        return;
    }

    const nrf_drv_twi_config_t twi_config0 = {
        .scl = P109_BLE_I2C1_SCL,
        .sda = P108_BLE_I2C1_SDA,
        .frequency = NRF_DRV_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false};

    err_code = nrf_drv_twi_init(&m_twi0, &twi_config0, NULL, NULL);
    APP_ERROR_CHECK(err_code);

    nrf_drv_twi_enable(&m_twi0);

    i2c0_inited = 1;
}

void pf_i2c_1_init(void)
{
    ret_code_t err_code;

    const nrf_drv_twi_config_t twi_config1 = {
        .scl = P024_BLE_I2C2_SCL,
        .sda = P100_BLE_I2C2_SDA,
        .frequency = NRF_DRV_TWI_FREQ_100K,
        .interrupt_priority = APP_IRQ_PRIORITY_HIGH,
        .clear_bus_init = false};

    err_code = nrf_drv_twi_init(&m_twi1, &twi_config1, NULL, NULL);
    APP_ERROR_CHECK(err_code);

    nrf_drv_twi_enable(&m_twi1);
}

void pf_i2c_0_uninit(void)
{
    if (i2c0_inited == 0)
    {
        return;
    }

    nrf_drv_twi_disable(&m_twi0);
    nrf_drv_twi_uninit(&m_twi0);

    i2c0_inited = 0;
}

void pf_i2c_1_uninit(void)
{
    nrf_drv_twi_disable(&m_twi1);
    nrf_drv_twi_uninit(&m_twi1);
}

// Return value: 0, OK; others, error
int32_t pf_i2c_0_write(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len)
{
    ret_code_t err_code;
    uint8_t buf[256] = {0};

    if(pbuf == NULL || len > 255)
    {
        return 1;
    }
    buf[0] = reg;
    memcpy(buf + 1, pbuf, len);
    err_code = nrf_drv_twi_tx(&m_twi0, addr, buf, len + 1, false);
    if (err_code != NRF_SUCCESS)
    {
        return 1;
    }
    return 0;
}

int32_t pf_i2c_1_write(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len)
{	
    ret_code_t err_code;
    uint8_t buf[256] = {0};

    if(pbuf == NULL || len > 255)
    {
        return 1;
    }
    buf[0] = reg;
    memcpy(buf + 1, pbuf, len);
    err_code = nrf_drv_twi_tx(&m_twi1, addr, buf, len + 1, false);
    if (err_code != NRF_SUCCESS)
    {
        return 1;
    }
    return 0;
}

int32_t pf_i2c_0_read(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len)
{
    ret_code_t err_code;

    err_code = nrf_drv_twi_tx(&m_twi0, addr, &reg, 1, true);
    if (err_code != NRF_SUCCESS)
    {
        return 1;
    }

    err_code = nrf_drv_twi_rx(&m_twi0, addr, pbuf, len);
    if (err_code != NRF_SUCCESS)
    {
        return 1;
    }
    return 0;
}

int32_t pf_i2c_1_read(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len)
{
    ret_code_t err_code;

    err_code = nrf_drv_twi_tx(&m_twi1, addr, &reg, 1, true);
    if (err_code != NRF_SUCCESS)
    {
        return 1;
    }

    err_code = nrf_drv_twi_rx(&m_twi1, addr, pbuf, len);
    if (err_code != NRF_SUCCESS)
    {
        return 1;
    }
    return 0;
}

void pf_print_mdm_uart_rx(uint8_t cmd, uint8_t *p_data, uint8_t len)
{
#if PF_PROTOCOL_TXRX_FILTER_EN
    if (PF_PROTOCOL_RXFILTER_FORMAT(cmd))
    {
        NRF_LOG_RAW_INFO("IO_CMD(%c, 0x%x) Value:\r", cmd,cmd);
        NRF_LOG_FLUSH();
        printf_hex_and_char(p_data, len);
    }
#else
    NRF_LOG_RAW_INFO("IO_CMD(%c) Value:\r", cmd);
    NRF_LOG_FLUSH();
    printf_hex_and_char(print_uart_rx_buf, print_uart_rx_in);
#endif /* PF_PROTOCOL_TXRX_FILTER_EN */
}

#define PRINT_UART_TX_BUF_SIZE (256)
static uint8_t print_uart_tx_buf[PRINT_UART_TX_BUF_SIZE] = {0};
static uint16_t print_uart_tx_in = 0;

void pf_print_mdm_uart_tx_init(void)
{
    print_uart_tx_in = 0;
    memset(print_uart_tx_buf, 0, PRINT_UART_TX_BUF_SIZE);
}

void pf_print_mdm_uart_tx(uint8_t data)
{
    print_uart_tx_buf[print_uart_tx_in] = data;
    print_uart_tx_in++;
}

void pf_print_mdm_uart_tx_flush(void)
{
#if PF_PROTOCOL_TXRX_FILTER_EN
    if (print_uart_tx_buf[0] == '$')
    {
        if (PF_PROTOCOL_TXFILTER_FORMAT(print_uart_tx_buf[2]))
        {
            printf_hex_and_char(print_uart_tx_buf, print_uart_tx_in);
        }
    }
#else
    printf_hex_and_char(print_uart_tx_buf, print_uart_rx_in);
#endif /* PF_PROTOCOL_TXRX_FILTER_EN */

    print_uart_tx_in = 0;
    memset(print_uart_tx_buf, 0, PRINT_UART_TX_BUF_SIZE);
}

/**@brief   Function for handling app_uart events.
 *
 * @details This function receives a single character from the app_uart module and appends it to
 *          a string. The string is sent over BLE when the last character received is a
 *          'new line' '\n' (hex 0x0A) or if the string reaches the maximum data length.
 */
static void uart_event_handle(app_uart_evt_t * p_event)
{
    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            // pf_log_raw(atel_log_ctl.irq_handler_en, "uart_event_handle(0x%x)\r", tmp);
            // pf_log_raw(atel_log_ctl.irq_handler_en, "%c", tmp);
            break;

        case APP_UART_COMMUNICATION_ERROR:
            // APP_ERROR_HANDLER(p_event->data.error_communication);
            pf_log_raw(atel_log_ctl.error_en, "APP_UART_COMMUNICATION_ERROR :%x.\r",p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            // APP_ERROR_HANDLER(p_event->data.error_code);
            pf_log_raw(atel_log_ctl.error_en, "APP_UART_FIFO_ERROR. :%x.\r",p_event->data.error_code);
            break;

        default:
            break;
    }
}

/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
void pf_uart_init(void)
{
    ret_code_t err_code;

    if (monet_data.uartTXDEnabled == 1)
    {
        return;
    }

    pf_log_raw(atel_log_ctl.normal_en, "pf_uart_init.\r");

    app_uart_comm_params_t const comm_params =
        {
            .rx_pin_no = P008_GPIO4_UART1_TXD, // Nordic RX
            .tx_pin_no = P006_GPIO37_UART1_RXD, // Nordic TX
            .rts_pin_no = UART_PIN_DISCONNECTED,
            .cts_pin_no = UART_PIN_DISCONNECTED,
            .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
            .use_parity = false,
            .baud_rate = UART_TO_APP_BAUDRATE
        };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_HIGHEST,
                       err_code);

    APP_ERROR_CHECK(err_code);

    monet_data.uartTXDEnabled = 1;
}
/**@snippet [UART Initialization] */

void pf_uart_deinit(void)
{
    if (monet_data.uartTXDEnabled == 0)
    {
        return;
    }

    pf_log_raw(atel_log_ctl.normal_en, "pf_uart_deinit.\r");

    app_uart_close();

    monet_data.uartTXDEnabled = 0;
}

uint32_t pf_uart_tx_one(uint8_t byte)
{
    return app_uart_put(byte);
}

uint32_t pf_uart_rx_one(uint8_t *p_data)
{
    return app_uart_get(p_data);
}

uint32_t pf_uart_tx_string(uint8_t *buffer, int nbytes)
{
  int i;

  for (i = 0; i < nbytes; i++) {
	app_uart_put(*buffer++);
  }
  
  return nbytes;
}


/**
 * @brief WDT events handler.
 */
static void wdt_event_handler(void)
{
    //NOTE: The max amount of time we can spend in WDT interrupt is two cycles of 32768[Hz] clock - after that, reset occurs
}

void pf_wdt_init(void)
{
    uint32_t err_code = NRF_SUCCESS;

    //Configure WDT.
    nrf_drv_wdt_config_t config = NRF_DRV_WDT_DEAFULT_CONFIG;
    err_code = nrf_drv_wdt_init(&config, wdt_event_handler);
    APP_ERROR_CHECK(err_code);
    err_code = nrf_drv_wdt_channel_alloc(&m_pf_wdt_channel_id);
    APP_ERROR_CHECK(err_code);
    nrf_drv_wdt_enable();
}

void pf_wdt_kick(void)
{
    nrf_drv_wdt_channel_feed(m_pf_wdt_channel_id);
}

void pf_bootloader_pre_enter(void)
{
    pf_log_raw(atel_log_ctl.core_en, "In pf_bootloader_pre_enter\r");

    nrfx_gpiote_uninit();
    pf_adc_poll_finish();
    pf_adc_deinit();
    pf_systick_stop();
    app_timer_stop_all();
    pf_uart_deinit();
    pf_wdt_kick();
}

uint32_t pf_bootloader_enter(void)
{
    pf_bootloader_pre_enter();

    uint32_t err_code;

    pf_log_raw(atel_log_ctl.core_en, "In pf_bootloader_enter\r");

    err_code = sd_power_gpregret_clr(0, 0xffffffff);
    APP_ERROR_CHECK(err_code);

    err_code = sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
    APP_ERROR_CHECK(err_code);

    err_code = sd_softdevice_disable();
    APP_ERROR_CHECK(err_code);

    DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_APP;
    DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
    //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_INVALID);

    // Signal that DFU mode is to be enter to the power management module
    #if PF_USE_JUMP_TO_ENTER_BOOT
    pf_bootloader_start();
    #else
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU);
    #endif /* PF_USE_JUMP_TO_ENTER_BOOT */

    return NRF_SUCCESS;
}

/* External Peripheral's Drivers ---------------------------------------------*/
void pf_cfg_before_hibernation(void)
{
    nrf_gpio_cfg_default(P006_GPIO37_UART1_RXD);
    nrf_gpio_cfg_default(P007_SWITCH_KEY);
    nrf_gpio_cfg_default(P008_GPIO4_UART1_TXD);
    nrf_gpio_cfg_default(P012_GPIO15_ESP32_TO_BLE);
    nrf_gpio_cfg_default(P014_GPIO14_ESP_WAKEUP_BLE);
    nrf_gpio_cfg_default(P020_BLE_WAKEUP_ESP);
    nrf_gpio_cfg_default(P101_CAN_VREF);
    nrf_gpio_cfg_default(P104_G_SENSOR_INT);
    nrf_gpio_cfg_default(P107_GPIO13_ESP_TO_APP);
    nrf_gpio_cfg_default(P025_CAN_INT);
    if (led_in_use == 0)
    {
        // nrf_gpio_cfg_default(P023_GREEN_LED1);
        nrf_gpio_pin_clear(P023_GREEN_LED1);
        // nrf_gpio_cfg_default(P105_RED_LED1);
        nrf_gpio_pin_clear(P105_RED_LED1);
    }
    // nrf_gpio_cfg_default(P103_POWER_KEY);
    nrf_gpio_cfg_default(P019_TMP_ALARM);

#if 1 // test
    // 0 uA
    // nrf_gpio_cfg_default(P026_BLE_SPI1_CS_FAKE);
    // nrf_gpio_cfg_default(P026_BLE_SPI1_CS);
    // nrf_gpio_cfg_default(P027_BLE_SPI1_SCK);
    // nrf_gpio_cfg_default(P004_BLE_SPI1_MOSI);
    // nrf_gpio_cfg_default(P005_BLE_SPI1_MISO);

    // 0 uA
    // nrf_gpio_cfg_default(P108_BLE_I2C1_SDA);
    // nrf_gpio_cfg_default(P109_BLE_I2C1_SCL);

    // 240 uA
    nrf_gpio_cfg_default(P015_BLE_MUX);

    // 0 uA
    // nrf_gpio_cfg_default(P016_BLE_UART2_TXD);
    // nrf_gpio_cfg_default(P017_BLE_UART2_RXD);

    // 35 uA
    // nrf_gpio_cfg_output(P021_CHRG_SLEEP_EN);
    // nrf_gpio_pin_clear(P021_CHRG_SLEEP_EN);

    // 0 uA
    // nrf_gpio_cfg_default(P022_BLE_ESP_EN);
    // nrf_gpio_cfg_default(P024_BLE_I2C2_SCL);
    // nrf_gpio_cfg_default(P100_BLE_I2C2_SDA);
    // nrf_gpio_cfg_output(P106_DCDC_3V3_EN);
    // nrf_gpio_pin_set(P106_DCDC_3V3_EN);

    // 250 uA
    // nrf_gpio_cfg_default(P003_ADC_EN);

    // 0 uA
    // nrf_gpio_cfg_default(P028_VBAT_ADC);
    // nrf_gpio_cfg_default(P029_12V_IN_ADC);
    // nrf_gpio_cfg_default(P031_12V_S_IN_ADC);
#endif // test
}

void pf_cfg_before_sleep(void)
{
}

void pf_cfg_before_shipping(void)
{
    nrf_gpio_cfg_default(P006_GPIO37_UART1_RXD);
    nrf_gpio_cfg_default(P007_SWITCH_KEY);
    nrf_gpio_cfg_default(P008_GPIO4_UART1_TXD);
    nrf_gpio_cfg_default(P012_GPIO15_ESP32_TO_BLE);
    nrf_gpio_cfg_default(P014_GPIO14_ESP_WAKEUP_BLE);
    nrf_gpio_cfg_default(P020_BLE_WAKEUP_ESP);
    nrf_gpio_cfg_default(P101_CAN_VREF);
    nrf_gpio_cfg_default(P104_G_SENSOR_INT);
    nrf_gpio_cfg_default(P107_GPIO13_ESP_TO_APP);
    nrf_gpio_cfg_default(P025_CAN_INT);
    // nrf_gpio_cfg_default(P023_GREEN_LED1);
    // nrf_gpio_cfg_default(P105_RED_LED1);
    // nrf_gpio_cfg_default(P103_POWER_KEY);
    nrf_gpio_cfg_default(P019_TMP_ALARM);

#if 1 // test
    // 0 uA
    nrf_gpio_cfg_default(P026_BLE_SPI1_CS_FAKE);
    nrf_gpio_cfg_default(P026_BLE_SPI1_CS);
    nrf_gpio_cfg_default(P027_BLE_SPI1_SCK);
    nrf_gpio_cfg_default(P004_BLE_SPI1_MOSI);
    nrf_gpio_cfg_default(P005_BLE_SPI1_MISO);

    // 0 uA
    nrf_gpio_cfg_default(P108_BLE_I2C1_SDA);
    nrf_gpio_cfg_default(P109_BLE_I2C1_SCL);

    // 240 uA
    nrf_gpio_cfg_default(P015_BLE_MUX);

    // 0 uA
    nrf_gpio_cfg_default(P016_BLE_UART2_TXD);
    nrf_gpio_cfg_default(P017_BLE_UART2_RXD);

    // 35 uA
    nrf_gpio_cfg_output(P021_CHRG_SLEEP_EN);
    nrf_gpio_pin_clear(P021_CHRG_SLEEP_EN);

    // 0 uA
    nrf_gpio_cfg_default(P022_BLE_ESP_EN);
    nrf_gpio_cfg_default(P024_BLE_I2C2_SCL);
    nrf_gpio_cfg_default(P100_BLE_I2C2_SDA);
    nrf_gpio_cfg_output(P106_DCDC_3V3_EN);
    nrf_gpio_pin_set(P106_DCDC_3V3_EN);

    // 250 uA
    nrf_gpio_cfg_default(P003_ADC_EN);

    // 0 uA
    // nrf_gpio_cfg_default(P028_VBAT_ADC);
    // nrf_gpio_cfg_default(P029_12V_IN_ADC);
    // nrf_gpio_cfg_default(P031_12V_S_IN_ADC);
#endif // test
}

uint8_t pf_charger_is_power_on(void)
{
    if (is_charger_power_on() != false)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

uint32_t pf_charger_input_voltage_get(void)
{
    return mp2762a_input_vol_get();
}

uint32_t pf_charger_input_voltage_limit_get(void)
{
    return mp2762a_input_vol_limit_get();
}

uint32_t pf_charger_battery_voltage_get(void)
{
    return mp2762a_bat_vol_get();
}

uint32_t pf_charger_input_current_get(void)
{
    return mp2762a_input_cur_get();
}

uint32_t pf_charger_charging_current_get(void)
{
    return mp2762a_bat_chg_cur_get();
}

void pf_delay_ms(uint32_t ms)
{
    nrf_delay_ms(ms);
}

#if BLE_DTM_ENABLE
void pf_dtm_enter(void)
{
    DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_BOOT;
    DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_DTME;
    //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_DTME);

    pf_log_raw(atel_log_ctl.core_en, "pf_dtm_enter\r\n");

    pf_delay_ms(10);

    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);

    while (1);
}

void pf_dtm_init(void)
{
    uint32_t dtm_error_code;

    dtm_error_code = dtm_init();

    if (dtm_error_code != DTM_SUCCESS)
    {
        // DTM cannot be correctly initialized.
    }

    pf_log_raw(atel_log_ctl.core_en, "pf_dtm_init(%d)\r\n", dtm_error_code);

    monet_data.cameraPowerOn = 1;
    monet_data.appActive = 1;

    io_tx_queue_init();

    pf_uart_init();

    pf_wdt_init();
}

void pf_dtm_process(void)
{
    uint8_t buf[] = "dE";
    BuildFrame('3', buf, 2);
    while (1)
    {
        pf_wdt_kick();
        atel_io_queue_process();
    }
}

void pf_dtm_exit(void)
{
    DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_BOOT;
    DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_DTMEX;
    //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_DTMEX);

    pf_log_raw(atel_log_ctl.core_en, "pf_dtm_exit\r\n");

    pf_delay_ms(10);

    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);

    while (1);
}

void pf_dtm_cmd(uint8_t cmd, uint8_t freq, uint8_t len, uint8_t payload)
{
    dtm_cmd_t      command_code = cmd & 0x03;
    dtm_freq_t     command_freq = freq & 0x3F;
    uint32_t       length       = len & 0x3F;
    dtm_pkt_type_t command_payload = payload & 0x03;
    uint32_t       err_code;
    dtm_event_t    result; // Result of a DTM operation.

    err_code = dtm_cmd(command_code, command_freq, length, command_payload);

    pf_log_raw(atel_log_ctl.core_en, "pf_dtm_cmd C(%d) F(%d) L(%d) P(%d) E(%d)\r\n"
                                   , cmd
                                   , freq
                                   , len
                                   , payload
                                   , err_code);

    if (dtm_event_get(&result))
    {
        pf_log_raw(atel_log_ctl.core_en, "dtm_event_get(0x%04x)\r\n", result);
    }
}
#endif /* BLE_DTM_ENABLE */

// This API may not shut system down totally
void pf_sys_shutdown(void)
{
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_RESET);
}

void pf_status_led_in_use(uint8_t in_use)
{
    led_in_use = in_use;
}

void pf_status_led_indicate(uint32_t delta_ms)
{
    static uint32_t flash_count = LED_FLASH_TOGGLE_MS;

    if (led_in_use)
    {
        return;
    }

    if (flash_count > delta_ms)
    {
        flash_count -= delta_ms;
    }
    else
    {
        flash_count = 0;
    }

    if (flash_count == 0)
    {
        if ((monet_data.cameraPowerOn == 1) && (monet_data.led_flash_enable == 1))
        {
            pf_gpio_toggle(GPIO_MCU_D);
        }

        flash_count = LED_FLASH_TOGGLE_MS;
    }
}

APP_TIMER_DEF(timer_solar_chg_mode);	// Timer for orange LED blink
uint32_t interval_solar_chg_mode = 300;//60;//300;		// Default value is 5 min (300 s)
bool need_to_check_sol_chg_mode = false;
//static bool solar_mode_running = false;		// State flag for Solar mode checking algorithm
uint8_t timer_solar_chg_mode_start = 0;

void timer_handler_solar_chg_mode(void *p_context)
{
    if (p_context == (void *)timer_solar_chg_mode)
    {
        need_to_check_sol_chg_mode = true;
    }
}

void pf_timer_solar_chg_mode_init(void)
{
    app_timer_create(&timer_solar_chg_mode, APP_TIMER_MODE_REPEATED, timer_handler_solar_chg_mode);
}

// Restart timer to check solar charging mode
void timer_solar_chg_mode_restart(void)
{
    if (timer_solar_chg_mode_start == 1)
    {
        return;
    }

    app_timer_start(timer_solar_chg_mode, APP_TIMER_TICKS(interval_solar_chg_mode * 1000), (void *)timer_solar_chg_mode);
    // solar_mode_running = true;

    timer_solar_chg_mode_start = 1;
}

// Stop timer
void timer_solar_chg_mode_stop(void)
{
    if (timer_solar_chg_mode_start == 0)
    {
        return;
    }

    app_timer_stop(timer_solar_chg_mode);

    timer_solar_chg_mode_start = 0;
}

// Set timer interval
// Param. value: unit in seconds
void timer_solar_chg_mode_intvl_set(uint32_t value)
{
    interval_solar_chg_mode = value;
}

// Read timer interval
// Return value: unit in seconds
uint32_t timer_solar_chg_mode_intvl_get(void)
{
    return interval_solar_chg_mode;
}

// Check solar charging mode
solar_chg_mode_t solar_chg_mode_get(void)
{
	return (!pf_gpio_read(GPIO_SOLAR_CHARGE_SWITCH)) ? SOLAR_CHG_MODE1 : SOLAR_CHG_MODE2;
}

// Check solar charging mode
void solar_chg_mode_set(solar_chg_mode_t mode)
{
    pf_gpio_write(GPIO_SOLAR_CHARGE_SWITCH, (mode == SOLAR_CHG_MODE1) ? 0 : 1);
}

void pf_camera_pwr_init(void)
{
    nrf_gpio_cfg_output(P106_DCDC_3V3_EN);
    nrf_gpio_cfg_output(P022_BLE_ESP_EN);
}

void pf_camera_pwr_ctrl(bool state)
{
    if (state == true)
    {
        nrf_gpio_pin_clear(P106_DCDC_3V3_EN);
        nrf_gpio_pin_set(P022_BLE_ESP_EN);
    }
    else
    {
        nrf_gpio_pin_set(P106_DCDC_3V3_EN);
        nrf_gpio_pin_clear(P022_BLE_ESP_EN);
    }
}

void uint8_to_ascii(uint8_t *p_buf, uint8_t num)
{
    uint8_t temp = num;
    uint16_t i = 0, j = 0;

    for (i = 0; i < 2; i++)
    {
        j = (temp >> ((1 - i) * 4)) & 0x0ful;
        if (j > 9)
        {
            p_buf[i] = j - 10 + 'a';
        }
        else
        {
            p_buf[i] = j + '0';
        }
    }
}

void printf_hex_and_char(uint8_t *p_data, uint16_t len)
{
    uint16_t i = 0, j = 0, k = 0;//, n = 0;
    uint8_t buf[64] = {0};

    if (atel_log_ctl.io_protocol_en == 0)
    {
        return;
    }

    for (i = 0; i < len; i++) // 0x20-0x7e
    {
        // sprintf((char *)(buf + k), "%02x ", p_data[i]);
        uint8_to_ascii(buf + k, p_data[i]);
        *(buf + k + 2) = ' ';
        k += 3;

        if ((((i + 1) % 16) == 0) || ((i + 1) == len))
        {
            pf_log_raw(1, "%s\r", buf);
            // n = ((((i + 1) % 16) == 0) ? 16 : (i % 16 + 1));

            // for (k = 0; k < (16 - n); k++)
            // {
            //     sprintf((char *)(buf + k), "   ");
            //     k += 3;
            // }
            // sprintf((char *)(buf + k), "|  ");
            // k += 3;

            memset(buf, 0, sizeof(buf));
            k = 0;

            for (j = ((((i + 1) % 16) == 0) ? (i - 15) : (i - (i % 16))); j <= i; j++)
            {
                if ((p_data[j] >= 0x20) && (p_data[j] <= 0x7e))
                {
                    // sprintf((char *)(buf + k), "%c  ", p_data[j]);
                    *(buf + k) = p_data[j];
                }
                else
                {
                    // sprintf((char *)(buf + k), ".  ");
                    *(buf + k) = '.';
                }
                *(buf + k + 1) = ' ';
                *(buf + k + 2) = ' ';
                k += 3;
            }
            // sprintf((char *)(buf + k), "\r\n");
            // k += 2;

            pf_log_raw(1, "%s\r", buf);

            memset(buf, 0, sizeof(buf));
            k = 0;
        }
    }
}

