/*
 * platform_hal_drv.h
 *
 *  Created on: Mar 3, 2020
 *      Author: Yangjie Gu
 */

#ifndef __PLATFORM_HAL_DRV_H
#define __PLATFORM_HAL_DRV_H
#ifdef __cplusplus
 extern "C" {
#endif

#include "zazu_hal.h"
#include "ble_data.h"
#include "drv_mcp25xxdf.h"
#include "charger_process.h"

typedef enum
{
    ATEL_GPIO_PIN_NOT_VALID = -1,
    ATEL_GPIO_PIN_RESET = 0,
    ATEL_GPIO_PIN_SET = 1
} atel_gpio_state_t;

typedef enum
{
    ATEL_GPIO_FUNC_IN = 0,
    ATEL_GPIO_FUNC_INT,
    ATEL_GPIO_FUNC_OUT,
    ATEL_GPIO_FUNC_OD,

    ATEL_GPIO_FUNC_MAX,
} atel_gpio_func_t;

typedef enum
{
    ATEL_GPIO_SENSE_LOWTOHI = 0,
    ATEL_GPIO_SENSE_HITOLOW,
    ATEL_GPIO_SENSE_TOGGLE,

    ATEL_GPIO_SENSE_MAX,
} atel_gpio_sense_t;

typedef enum
{
    ATEL_GPIO_NOPULL = 0,
    ATEL_GPIO_PULLUP,
    ATEL_GPIO_PULLDOWN,

    ATEL_GPIO_PULL_MAX,
} atel_gpio_pull_t;

typedef struct
{
    atel_gpio_func_t        func;
    atel_gpio_sense_t       sense;
    atel_gpio_pull_t        pull;
} atel_gpio_cfg_t;

#define GPIO_PATTERN_PARAM_MAX_BYTE (16)
#define GPIO_PATTERN_STATE_DISABLE (0)
#define GPIO_PATTERN_STATE_ENABLED (1)
#define GPIO_PATTERN_STATE_CHANGED (2)
typedef struct
{
    uint8_t state;
    uint8_t pin;
    uint8_t state_s;
    uint8_t state_e;
    uint32_t dur_ms1;
    uint32_t dur_ms2;
    uint32_t total_ms;
    uint32_t deb_ms1;
    uint32_t deb_ms2;
    uint8_t raw_param[GPIO_PATTERN_PARAM_MAX_BYTE];
} gpio_pattern_t;

extern volatile uint8_t adc_convert_over;
extern volatile uint32_t gTimer;

#define SAADC_READY_TIMER_PERIOD_MS (20)
void pf_adc_ready_timer_init(void);
void pf_adc_ready_timer_start(void);
uint8_t pf_saadc_ready(void);
void pf_saadc_ready_clear(void);

void pf_adc_init(void);
void pf_adc_start(void);
void pf_adc_poll_finish(void);
void pf_adc_deinit(void);

int8_t pf_gpio_cfg(uint32_t index, atel_gpio_cfg_t cfg);
int8_t pf_gpio_write(uint32_t index, uint32_t value);
int8_t pf_gpio_toggle(uint32_t index);
int32_t pf_gpio_read(uint32_t index);

#define BUTTON_PATTERN_TIMER_PERIOD_MS (50)
void pf_button_pattern_timer_init(void);
void pf_button_pattern_timer_start(void);
void pf_button_pattern_timer_stop(void);
uint32_t pf_button_pattern_timer_get_ms(void);
void pf_device_power_normal_indicate(void);

#if DEVICE_BUTTON_FOR_FACTORY
void pf_device_button_for_factory_init(void);
void pf_device_button_for_factory_enter(void);
#endif /* DEVICE_BUTTON_FOR_FACTORY */

#if DEVICE_POWER_KEY_SUPPORT
void pf_device_power_off_mode_init(void);
void pf_device_power_off_mode_enter(void);
#endif /* DEVICE_POWER_KEY_SUPPORT */

void pf_device_unpairing_mode_enter(void);
void pf_device_unpairing_mode_exit(void);
void pf_device_pairing_mode_enter(void);
void pf_device_pairing_fail(void);
void pf_device_pairing_success(void);
void pf_device_upgrading_mode_enter(void);
void pf_device_upgrading_fail(void);
void pf_device_upgrading_success(void);
void pf_device_extern_power_charging_process(void);
void pf_device_extern_power_charging_full(void);
void pf_device_extern_power_charging_exit(void);


#define GPIO_PATTERN_TIMER_PERIOD_MS (50)
void pf_gpio_pattern_timer_init(void);
void pf_gpio_pattern_set(uint8_t *data, uint8_t len);
void pf_gpio_pattern_process(uint32_t ms);

void pf_spi_init(void);
void pf_spi_uninit(void);
uint32_t pf_spi_transmit_receive(uint8_t* p_tx_buf, uint8_t tx_length, uint8_t* p_rx_buf, uint8_t rx_length);

void pf_systick_start(uint32_t period_ms);
void pf_systick_stop(void);
void pf_systick_change(void);
void pf_systick_shippping_start(void);
void pf_systick_shipping_stop(void);

uint32_t pf_systime_get(void);

void pf_i2c_0_init(void);
void pf_i2c_1_init(void);
void pf_i2c_0_uninit(void);
void pf_i2c_1_uninit(void);
int32_t pf_i2c_0_write(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len);
int32_t pf_i2c_1_write(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len);
int32_t pf_i2c_0_read(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len);
int32_t pf_i2c_1_read(uint8_t addr, uint8_t reg, uint8_t *pbuf, uint16_t len);

void pf_print_mdm_uart_rx(uint8_t cmd, uint8_t *p_data, uint8_t len);
void pf_print_mdm_uart_tx_init(void);
void pf_print_mdm_uart_tx(uint8_t data);
void pf_print_mdm_uart_tx_flush(void);

void pf_uart_init(void);
void pf_uart_deinit(void);
uint32_t pf_uart_tx_one(uint8_t byte);
uint32_t pf_uart_rx_one(uint8_t *p_data);
uint32_t pf_uart_tx_string(uint8_t *buffer, int nbytes);

void pf_wdt_init(void);
void pf_wdt_kick(void);

void pf_ble_adv_start_without_whitelist(void);
void pf_ble_adv_start(void);
void pf_ble_adv_connection_reset(void);
void pf_ble_white_list_reset(void);
void pf_ble_delete_bonds(void);
void pf_ble_load_white_list(void);
void pf_ble_peer_tx_rssi_get(int16_t *p_rssi);
void pf_ble_peer_tx_rssi_stop(void);

void pf_bootloader_pre_enter(void);
uint32_t pf_bootloader_enter(void);

void pf_cfg_before_hibernation(void);
void pf_cfg_before_sleep(void);
void pf_cfg_before_shipping(void);

uint8_t pf_charger_is_power_on(void);
uint32_t pf_charger_input_voltage_get(void);
uint32_t pf_charger_input_voltage_limit_get(void);
uint32_t pf_charger_battery_voltage_get(void);
uint32_t pf_charger_input_current_get(void);
uint32_t pf_charger_charging_current_get(void);

void pf_device_enter_idle_state(void);

void pf_delay_ms(uint32_t ms);

void pf_dtm_enter(void);
void pf_dtm_init(void);
void pf_dtm_process(void);
void pf_dtm_exit(void);
void pf_dtm_cmd(uint8_t cmd, uint8_t freq, uint8_t len, uint8_t payload);

void pf_sys_shutdown(void);

void pf_status_led_in_use(uint8_t in_use);

void pf_status_led_indicate(uint32_t delta_ms);

typedef enum
{
    SOLAR_CHG_MODE1 = 1,    // Mode 1, Solar power --> Charger chip --> battery
    SOLAR_CHG_MODE2         // Mode 2, Solar power --> battery. Charger chip is bypassed
} solar_chg_mode_t;         // Solar charging mode.

extern bool need_to_check_sol_chg_mode;

void pf_timer_solar_chg_mode_init(void);
void timer_solar_chg_mode_restart(void);
void timer_solar_chg_mode_stop(void);
void timer_solar_chg_mode_intvl_set(uint32_t value);
uint32_t timer_solar_chg_mode_intvl_get(void);

solar_chg_mode_t solar_chg_mode_get(void);
void solar_chg_mode_set(solar_chg_mode_t mode);

void pf_camera_pwr_init(void);
void pf_camera_pwr_ctrl(bool state);

#define PF_PROTOCOL_TXRX_FILTER_EN (1)
// #define PF_PROTOCOL_RXFILTER_FORMAT(x) (((x) == 'G') || ((x) == 'E'))
// #define PF_PROTOCOL_TXFILTER_FORMAT(x) (((x) == 'g') || ((x) == 'e'))
#define PF_PROTOCOL_RXFILTER_FORMAT(x) (((x) != '2') && ((x) != '5'))
#define PF_PROTOCOL_TXFILTER_FORMAT(x) (((x) != '1') && ((x) != '4'))

#define PF_ADC_RAW_TO_BATT_MV(v) (((((double)v) / (double)ADC_RESOLUTION_VALUE) / (double)ADC_BUB_VOL_MULTIPLIER_FACTOR) + ADC_BUB_VOL_MODIFICATION_VALUE)
#define PF_ADC_RAW_TO_VIN_MV(v) (((((double)v) / (double)ADC_RESOLUTION_VALUE) / (double)ADC_VIN_VOL_MULTIPLIER_FACTOR) + ADC_VIN_VOL_MODIFICATION_VALUE)
#define PF_ADC_RAW_TO_VSIN_MV(v) (((((double)v) / (double)ADC_RESOLUTION_VALUE) / (double)ADC_VSIN_VOL_MULTIPLIER_FACTOR) + ADC_VSIN_VOL_MODIFICATION_VALUE)

#define pf_log_raw(onoff, ...) do {if (onoff) {NRF_LOG_RAW_INFO(__VA_ARGS__); NRF_LOG_FLUSH();}} while (0);
#define pf_log_raw_force(...) do {NRF_LOG_RAW_INFO(__VA_ARGS__); NRF_LOG_FLUSH();} while (0);

#define PF_USE_JUMP_TO_ENTER_BOOT (0)

void printf_hex_and_char(uint8_t *p_data, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif /* __PLATFORM_HAL_DRV_H */
