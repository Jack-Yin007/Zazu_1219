
/*
 * platform_hal_drv.c
 *
 *  Created on: Mar 3, 2020
 *      Author: Yangjie Gu
 */

/* Includes ------------------------------------------------------------------*/
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "user.h"

/* Private define ------------------------------------------------------------*/
typedef struct {
    uint32_t pin;
} pin_struct;


#define UART_TX_BUF_SIZE        1024                /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE        1024                /**< UART RX buffer size. */

/* External Variables --------------------------------------------------------*/

/* External Functions --------------------------------------------------------*/

/* Variables -----------------------------------------------------------------*/
static const pin_struct gpin[GPIO_LAST] = {
    [GPIO_MCU_SLEEP_DEV].pin =      SIMBA_BLE_UART_TX,
    [GPIO_MCU_SLEEP_DEV1].pin =     SIMBA_BLE_UART_RX
};

volatile uint32_t gTimer = 0;
APP_TIMER_DEF(pf_systick_timer);

nrf_drv_wdt_channel_id m_pf_wdt_channel_id;

/* On Chip Peripheral's Drivers ----------------------------------------------*/
static void gpiote_event_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    NRF_LOG_RAW_INFO(">>>>gpiote_event_handler(%d:%d).\r", pin, action);
    switch (pin)
    {
        case SIMBA_BLE_UART_RX:
        if (pf_gpio_read(GPIO_MCU_SLEEP_DEV1) == 1)
        {
            monet_data.deviceWakeupDetect++;
        }
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

    if (gpin[index].pin == PIN_NOT_VALID)
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
        if (index == GPIO_MCU_SLEEP_DEV1)
        {
            pf_log_raw(atel_log_ctl.platform_en, "pf_gpio_cfg index(%d:%d) gpiote_in_uninit.\r",
                                                  index, gpin[index].pin);
            nrfx_gpiote_in_uninit(gpin[index].pin);
        }

        nrf_gpio_cfg_default(gpin[index].pin);

        if ((cfg.func == ATEL_GPIO_FUNC_OUT) ||
            (cfg.func == ATEL_GPIO_FUNC_OD)) {
            dir = NRF_GPIO_PIN_DIR_OUTPUT;
            if (cfg.func == ATEL_GPIO_FUNC_OD) {
                drive = NRF_GPIO_PIN_S0D1;
            }

            input = NRF_GPIO_PIN_INPUT_DISCONNECT;
	
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

static void timer_systick_handler(void * p_context)
{
    ret_code_t ret = 0;
    UNUSED_PARAMETER(p_context);

    gTimer++;

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

void pf_print_mdm_uart_rx(uint8_t cmd, uint8_t *p_data, uint8_t len)
{
#if PF_PROTOCOL_TXRX_FILTER_EN
    if (PF_PROTOCOL_RXFILTER_FORMAT(cmd))
    {
        NRF_LOG_RAW_INFO("IO_CMD(%c, %x, %d) Value:\r", cmd, cmd, len);
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

    if (data == 0x0d)
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
}

/**@brief   Function for handling app_uart events.
 *
 * @details This function receives a single character from the app_uart module and appends it to
 *          a string. The string is sent over BLE when the last character received is a
 *          'new line' '\n' (hex 0x0A) or if the string reaches the maximum data length.
 */
void uart_event_handle(app_uart_evt_t * p_event)
{
    switch (p_event->evt_type)
    {
        /**@snippet [Handling data from UART] */
        case APP_UART_DATA_READY:
            // pf_log_raw(atel_log_ctl.irq_handler_en, "uart_event_handle(0x%x)\r", tmp);
            // pf_log_raw(atel_log_ctl.irq_handler_en, "%c", tmp);
            break;

        /**@snippet [Handling data from UART] */
        case APP_UART_COMMUNICATION_ERROR:
            // APP_ERROR_HANDLER(p_event->data.error_communication);
            pf_log_raw(atel_log_ctl.error_en, "APP_UART_COMMUNICATION_ERROR.\r");
            break;

        case APP_UART_FIFO_ERROR:
            // APP_ERROR_HANDLER(p_event->data.error_code);
            pf_log_raw(atel_log_ctl.error_en, "APP_UART_FIFO_ERROR.\r");
            break;

        default:
            break;
    }
}


/**@brief Function for initializing the UART. */
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
        .rx_pin_no    = SIMBA_BLE_UART_RX,
        .tx_pin_no    = SIMBA_BLE_UART_TX,
        .rts_pin_no   = UART_PIN_DISCONNECTED,
        .cts_pin_no   = UART_PIN_DISCONNECTED,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
        .baud_rate    = UART_BAUDRATE_BAUDRATE_Baud115200
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

    app_uart_close();

    pf_log_raw(atel_log_ctl.normal_en, "pf_uart_deinit.\r");

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
}

uint32_t pf_bootloader_enter(void)
{
    uint32_t err_code;

    pf_log_raw(atel_log_ctl.core_en, "In pf_bootloader_enter\r\n");

    err_code = sd_power_gpregret_clr(0, 0xffffffff);
    VERIFY_SUCCESS(err_code);

    err_code = sd_power_gpregret_set(0, BOOTLOADER_DFU_START);
    VERIFY_SUCCESS(err_code);

    // Signal that DFU mode is to be enter to the power management module
    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU);

    return NRF_SUCCESS;
}

/* External Peripheral's Drivers ---------------------------------------------*/
void pf_cfg_before_hibernation(void)
{
    // nrf_gpio_cfg_default(SIMBA_BLE_UART_TX);
}

void pf_cfg_before_sleep(void)
{
}

void pf_delay_ms(uint32_t ms)
{
    nrf_delay_ms(ms);
}

void pf_dtm_enter(void)
{
    *((uint32_t *)(0x2000fffc)) = 0xDDDDDDDD;

    pf_log_raw(atel_log_ctl.error_en, "pf_dtm_enter\r\n");

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

    pf_log_raw(atel_log_ctl.error_en, "pf_dtm_init(%d)\r\n", dtm_error_code);

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
    *((uint32_t *)(0x2000fffc)) = 0;

    pf_log_raw(atel_log_ctl.error_en, "pf_dtm_exit\r\n");

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

    pf_log_raw(atel_log_ctl.error_en, "pf_dtm_cmd C(%d) F(%d) L(%d) P(%d) E(%d)\r\n"
                                    , cmd
                                    , freq
                                    , len
                                    , payload
                                    , err_code);

    if (dtm_event_get(&result))
    {
        pf_log_raw(atel_log_ctl.error_en, "dtm_event_get(0x%04x)\r\n", result);
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

