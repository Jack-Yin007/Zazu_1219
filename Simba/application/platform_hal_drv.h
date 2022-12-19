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

#include "simba_hal.h"

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

extern volatile uint32_t gTimer;

int8_t pf_gpio_cfg(uint32_t index, atel_gpio_cfg_t cfg);
int8_t pf_gpio_write(uint32_t index, uint32_t value);
int8_t pf_gpio_toggle(uint32_t index);
int32_t pf_gpio_read(uint32_t index);

void pf_systick_start(uint32_t period_ms);
void pf_systick_stop(void);
void pf_systick_change(void);

void pf_print_mdm_uart_rx(uint8_t cmd, uint8_t *p_data, uint8_t len);
void pf_print_mdm_uart_tx_init(void);
void pf_print_mdm_uart_tx(uint8_t data);

void pf_uart_init(void);
void pf_uart_deinit(void);
uint32_t pf_uart_tx_one(uint8_t byte);
uint32_t pf_uart_rx_one(uint8_t *p_data);
uint32_t pf_uart_tx_string(uint8_t *buffer, int nbytes);

void pf_wdt_init(void);
void pf_wdt_kick(void);

void pf_bootloader_pre_enter(void);
uint32_t pf_bootloader_enter(void);

void pf_cfg_before_hibernation(void);
void pf_cfg_before_sleep(void);

void pf_delay_ms(uint32_t ms);

void pf_dtm_enter(void);
void pf_dtm_init(void);
void pf_dtm_process(void);
void pf_dtm_exit(void);
void pf_dtm_cmd(uint8_t cmd, uint8_t freq, uint8_t len, uint8_t payload);

#define PF_PROTOCOL_TXRX_FILTER_EN (1)
// #define PF_PROTOCOL_RXFILTER_FORMAT(x) (((x) == 'G') || ((x) == 'E'))
// #define PF_PROTOCOL_TXFILTER_FORMAT(x) (((x) == 'g') || ((x) == 'e'))
#define PF_PROTOCOL_RXFILTER_FORMAT(x) (((x) != 'A') && ((x) != 'Q') && ((x) != 'K') && ((x) != '2'))
#define PF_PROTOCOL_TXFILTER_FORMAT(x) (((x) != 'a') && ((x) != 'q') && ((x) != '5') && ((x) != '1'))

#define PF_ADC_RAW_TO_BATT_MV(v) (((((double)v) / (double)ADC_RESOLUTION_VALUE) / (double)ADC_BUB_VOL_MULTIPLIER_FACTOR) + ADC_BUB_VOL_MODIFICATION_VALUE)

#define pf_log_raw(onoff, ...) do {if (onoff) {NRF_LOG_RAW_INFO(__VA_ARGS__); NRF_LOG_FLUSH();}} while (0);
#define pf_log_raw_force(...) do {NRF_LOG_RAW_INFO(__VA_ARGS__); NRF_LOG_FLUSH();} while (0);

void printf_hex_and_char(uint8_t *p_data, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif /* __PLATFORM_HAL_DRV_H */
