#ifndef __ATEL_AC61_HAL_H
#define __ATEL_AC61_HAL_H
#ifdef __cplusplus
 extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "nordic_common.h"
#include "app_error.h"
#include "app_uart.h"
#include "ble_db_discovery.h"
#include "app_timer.h"
#include "app_util.h"
#include "bsp_btn_ble.h"
#include "ble.h"
#include "ble_dtm.h"
#include "ble_gap.h"
#include "ble_hci.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_sdh_soc.h"
//#include "ble_nus_c.h"
//#include "nrf_ble_scan.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "ble_srv_common.h"
#include "ble_aus.h"

#include "nrf_pwr_mgmt.h"
#include "nrf_drv_wdt.h"
#include "nrf_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"
#include "nrf_bootloader_info.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_delay.h"

/* GPIO_MCU_SLEEP_DEV: UART to DEV TX */
/* GPIO_MCU_SLEEP_DEV1: UART to DEV RX */
#define ATEL_ALL_GPIOS \
        ATEL_GPIO_ASSOC(GPIO_MCU_SLEEP_DEV,         0, 1, 0, 1) \
        ATEL_GPIO_ASSOC(GPIO_MCU_SLEEP_DEV1,        1, 1, 0, 0)

#define PIN_NOT_VALID               (0xffffffff)
// PB14_LEU0_RX_BLE_INT
#define SIMBA_BLE_UART_TX           (6)
// PB13_LEU0_TX_MCU_WAKE_BLE
#define SIMBA_BLE_UART_RX           (8)
#define SIMBA_BLE_I2C_SDA           (9)
#define SIMBA_BLE_I2C_SCL           (10)

#ifdef __cplusplus
}
#endif
#endif /* __ATEL_AC61_HAL_H */

