/**
 * Copyright (c) 2014 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_uart_over_ble_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service.
 * This application uses the @ref srvlib_conn_params module.
 */


#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_aus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"
#include "peer_manager.h"
#include "peer_manager_handler.h"
#include "fds.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_lesc.h"
#include "nrf_ble_qwr.h"
#include "ble_conn_state.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_gpiote.h"
#include "nrf_gpio.h"
#include "nrf_drv_gpiote.h"

#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "platform_hal_drv.h"
#include "nrf_delay.h"
#include "user.h"
#include "version.h"
#include "ble_data.h"
#include "nrf_drv_clock.h"

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define DEVICE_NAME                     "ZAZU"                                      /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_INTERVAL                906                                         /**< The advertising interval (in units of 0.625 ms. This value corresponds to 800 ms). */

#define APP_ADV_DURATION                18000                                       /**< The advertising duration (180 seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(15, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(30, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(5000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    0                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define LESC_DEBUG_MODE                     0                                       /**< Set to 1 to use LESC debug keys, allows you to use a sniffer to inspect traffic. */

#define SEC_PARAM_BOND                      1                                       /**< Perform bonding. */
#define SEC_PARAM_MITM                      0                                       /**< Man In The Middle protection not required. */
#define SEC_PARAM_LESC                      1                                       /**< LE Secure Connections enabled. */
#define SEC_PARAM_KEYPRESS                  0                                       /**< Keypress notifications not enabled. */
#define SEC_PARAM_IO_CAPABILITIES           BLE_GAP_IO_CAPS_NONE                    /**< No I/O capabilities. */
#define SEC_PARAM_OOB                       0                                       /**< Out Of Band data not available. */
#define SEC_PARAM_MIN_KEY_SIZE              7                                       /**< Minimum encryption key size. */
#define SEC_PARAM_MAX_KEY_SIZE              16                                      /**< Maximum encryption key size. */

#define APP_BEACON_INFO_LENGTH          0x17                               /**< Total length of information advertised by the Beacon. */
#define APP_ADV_DATA_LENGTH             0x15                               /**< Length of manufacturer specific data in the advertisement. */
#define APP_DEVICE_TYPE                 0x01                               /**< 0x02 refers to Beacon. */
#define APP_MEASURED_RSSI               0xC3                               /**< The Beacon's measured RSSI at 1 meter distance in dBm. */
#define APP_COMPANY_IDENTIFIER          0x0059                             /**< Company identifier for Nordic Semiconductor ASA. as per www.bluetooth.org. */
// #define APP_MAJOR_VALUE                 0x01, 0x02                         /**< Major value used to identify Beacons. */
// #define APP_MINOR_VALUE                 0x03, 0x04                         /**< Minor value used to identify Beacons. */
#define APP_BEACON_UUID                 0xFB, 0xDD, 0x00, 0x01, \
                                        0xAA, 0x76, 0x42, 0x3A, \
                                        0xB5, 0x2F, 0x6F, 0x74, \
                                        0xEC, 0xDE, 0x9E, 0x3C            /**< Proprietary UUID for Beacon. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

static int8_t tx_power_level_default = 8;
#endif /* BLE_FUNCTION_ONOFF */

ble_gap_addr_t peripheral_addr = {0};

ble_gap_addr_t whitelist_addrs[BLE_CHANNEL_NUM_MAX] = {0};
uint16_t whitelist_count = 0;

#if BLE_DATA_THROUGHPUT_ERROR_CHECK_EN
uint8_t data_throughput_last = 0;
bool data_throughput_wait_index0 = true;
uint32_t data_throughput_total_count = 0, data_throughput_total_lost = 0;
#endif /* BLE_DATA_THROUGHPUT_ERROR_CHECK_EN */

static ble_advdata_manuf_data_t m_manuf_specific_data = {0}; 

static uint8_t m_beacon_info[APP_BEACON_INFO_LENGTH] =                    /**< Information advertised by the Beacon. */
{
    APP_DEVICE_TYPE,     // Manufacturer specific information. Specifies the device type in this
                         // implementation.
    APP_ADV_DATA_LENGTH, // Manufacturer specific information. Specifies the length of the
                         // manufacturer specific data in this implementation.
    APP_BEACON_UUID,     // 128 bit UUID value.
    MNT_MAJOR,
    MNT_MINOR,
    MNT_REVISION,
    MNT_BUILD,
    // APP_MAJOR_VALUE,     // Major arbitrary value that can be used to distinguish between Beacons.
    // APP_MINOR_VALUE,     // Minor arbitrary value that can be used to distinguish between Beacons.
    APP_MEASURED_RSSI    // Manufacturer specific information. The Beacon's measured TX power in
                         // this implementation.
};

uint16_t get_ble_conn_handle(void)
{
    return m_conn_handle;
}

/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Clear bond information from persistent storage.
 */
static void delete_bonds(void)
{
    ret_code_t err_code;

    NRF_LOG_INFO("Erase bonds!");

    err_code = pm_peers_delete();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for starting advertising.
 */
void advertising_start(bool erase_bonds)
{
    if (monet_data.is_advertising == 1)
    {
        pf_log_raw(atel_log_ctl.core_en, "advertising_start: already.\r");
        return;
    }
    if (erase_bonds == true)
    {
        delete_bonds();
        // Advertising is started by PM_EVT_PEERS_DELETE_SUCCEEDED event.
    }
    else
    {
        ret_code_t err_code;

        err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
        pf_log_raw(atel_log_ctl.core_en, "advertising_start: %d.\r", err_code);
        CRITICAL_REGION_ENTER();
        monet_data.to_advertising_result = err_code;
        if (monet_data.to_advertising_result == NRF_SUCCESS)
        {
            monet_data.is_advertising = 1;
        }

        if (err_code)
        {
            if (err_code == NRF_ERROR_INVALID_PARAM)
            {
                monet_data.to_advertising_without_white_list = 1;
            }
            monet_data.to_advertising_recover = 1;
        }
        else
        {
            monet_data.to_advertising_recover = 0;
        }
        CRITICAL_REGION_EXIT();
        // APP_ERROR_CHECK(err_code);
    }
}

void pf_pm_handler_disconnect_on_sec_failure(pm_evt_t const * p_pm_evt)
{
    ret_code_t err_code;

    if (p_pm_evt->evt_id == PM_EVT_CONN_SEC_FAILED)
    {
        NRF_LOG_WARNING("Disconnecting conn_handle %d.", p_pm_evt->conn_handle);
        err_code = sd_ble_gap_disconnect(p_pm_evt->conn_handle,
                                         BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        if ((err_code != NRF_ERROR_INVALID_STATE) && (err_code != BLE_ERROR_INVALID_CONN_HANDLE))
        {
           // APP_ERROR_CHECK(err_code);
        }
    }
}

/**@brief Function for handling Peer Manager events.
 *
 * @param[in] p_evt  Peer Manager event.
 */
static void pm_evt_handler(pm_evt_t const * p_evt)
{
    pm_handler_on_pm_evt(p_evt);
    pf_pm_handler_disconnect_on_sec_failure(p_evt);
    pm_handler_flash_clean(p_evt);

    switch (p_evt->evt_id)
    {
        case PM_EVT_PEERS_DELETE_SUCCEEDED:
            // advertising_start(false);
            pf_log_raw(atel_log_ctl.core_en, "pm_evt_handler: PM_EVT_PEERS_DELETE_SUCCEEDED\r");
            break;
        
        case PM_EVT_PEERS_DELETE_FAILED:
            pf_log_raw(atel_log_ctl.core_en, "pm_evt_handler: PM_EVT_PEERS_DELETE_FAILED\r");
            break;

        case PM_EVT_BONDED_PEER_CONNECTED:
            pf_log_raw(atel_log_ctl.core_en,"pm_evt_handler: PM_EVT_BONDED_PEER_CONNECTED, peer id %x\r", p_evt->peer_id);
            break;

        case PM_EVT_CONN_SEC_SUCCEEDED:
            pf_log_raw(atel_log_ctl.core_en,"pm_evt_handler: PM_EVT_CONN_SEC_SUCCEEDED.\r");
            break;

        case PM_EVT_CONN_SEC_FAILED:   // if fail, we should advertise again.
        {
            pf_ble_adv_connection_reset();
            monet_data.to_advertising = 1;
            pf_log_raw(atel_log_ctl.core_en,"pm_evt_handler: PM_EVT_CONN_SEC_FAILED Fail.\r");
        }
            break;
        case PM_EVT_CONN_SEC_CONFIG_REQ:
        {
            pm_conn_sec_config_t cfg;
            cfg.allow_repairing = true;
            pm_conn_sec_config_reply(p_evt->conn_handle, &cfg);
        }
            break;

        default:
            break;
    }
}

/**@brief Function for initializing the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}
#endif /* BLE_FUNCTION_ONOFF */

#if BLE_DATA_THROUGHPUT_ERROR_CHECK_EN
__STATIC_INLINE void ble_data_throughput_check_correction_reset(void)
{
    data_throughput_index = 0;
    data_throughput_last = 0;
    data_throughput_total_lost = 0;
    data_throughput_total_count = 0;
    data_throughput_wait_index0 = true;
}

__STATIC_INLINE void ble_data_throughput_check_correction(uint8_t *p_data, uint8_t len)
{
    uint8_t delta = 0;

    if (data_throughput_wait_index0)
    {
        NRF_LOG_RAW_INFO(">>>>wait_index0 p_data[0]: %u\r", p_data[0]);
        NRF_LOG_FLUSH();
        data_throughput_last = 0;
        data_throughput_total_lost = 0;
        data_throughput_total_count = 0;

        if (p_data[0] != 0)
        {
            return;
        }
        data_throughput_wait_index0 = false;
    }

    delta = p_data[0] - data_throughput_last;
    data_throughput_last = p_data[0];
    // NRF_LOG_RAW_INFO(">>>>p_data[0]: %u, last: %u, delta: %u\r", p_data[0], data_throughput_last, delta);
    data_throughput_total_count += delta;
    if (delta > 1)
    {
        NRF_LOG_RAW_INFO(">>>>Peripheral detected data lost: %u\r", data_throughput_total_lost);
        NRF_LOG_FLUSH();
        data_throughput_total_lost += (delta - 1);
    }

    if (my_checksum_8(p_data, len - 1) != p_data[len - 1])
    {
        NRF_LOG_RAW_INFO(">>>>Peripheral checksum_8 fail(%u:%u)\r", data_throughput_total_count, data_throughput_total_lost);
        NRF_LOG_FLUSH();
    }

    if ((data_throughput_total_count % 256) == 255)
    {
        NRF_LOG_RAW_INFO(">>>>Peripheral AUS(%u) (%u:%u) lost:"NRF_LOG_FLOAT_MARKER"\r",
                    len,
                    data_throughput_total_count,
                    data_throughput_total_lost,
                    NRF_LOG_FLOAT((double)data_throughput_total_lost / (double)data_throughput_total_count));
        NRF_LOG_FLUSH();
    }
}
#endif /* BLE_DATA_THROUGHPUT_ERROR_CHECK_EN */

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

/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
static void nus_data_handler(ble_nus_evt_t * p_evt)
{
    uint16_t channel = 0xffff;

    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
        uint8_t *p_data = (uint8_t *)p_evt->params.rx_data.p_data;
        uint8_t data_len = (uint8_t)p_evt->params.rx_data.length;
        m_len_recv += p_evt->params.rx_data.length;
        channel = ble_channel_get_from_handler(p_evt->conn_handle);
        // NRF_LOG_INFO("BLE_AUS_EVT_RX_DATA CH: %d len: %d.", channel, p_evt->params.rx_data.length);
        // NRF_LOG_FLUSH();
        #if BLE_CONNECTION_SUPPORT_HEARTBEAT
        if ((((uint8_t *)p_evt->params.rx_data.p_data)[0] == 0x65) &&
            (((uint8_t *)p_evt->params.rx_data.p_data)[1] == 0x00))
        {
            uint32_t beat = (((uint8_t *)p_evt->params.rx_data.p_data)[2] << 0) |
                             (((uint8_t *)p_evt->params.rx_data.p_data)[3] << 8) |
                             (((uint8_t *)p_evt->params.rx_data.p_data)[4] << 16)|
                             (((uint8_t *)p_evt->params.rx_data.p_data)[5] << 24);
            uint16_t min_100us = (((uint8_t *)p_evt->params.rx_data.p_data)[12] << 0) |
                                 (((uint8_t *)p_evt->params.rx_data.p_data)[13] << 8);
            uint32_t timeout_100us = (((uint8_t *)p_evt->params.rx_data.p_data)[18] << 0) |
                                     (((uint8_t *)p_evt->params.rx_data.p_data)[19] << 8) |
                                     (((uint8_t *)p_evt->params.rx_data.p_data)[20] << 16)|
                                     (((uint8_t *)p_evt->params.rx_data.p_data)[21] << 24);
            uint8_t camera_on = ((uint8_t *)p_evt->params.rx_data.p_data)[22];
            uint8_t camera_act = ((uint8_t *)p_evt->params.rx_data.p_data)[23];
            channel = ble_channel_get_from_mac((uint8_t *)(p_evt->params.rx_data.p_data + 6));

            if (channel != 0xffff)
            {
                if (monet_data.ble_info[channel].connect_status != BLE_CONNECTION_STATUS_CONNECTED)
                {
                    monet_data.bleConnectionEvent = 1 + channel;
                    monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_CONNECTED;
                    monet_data.ble_info[channel].handler = p_evt->conn_handle;
                    if ((min_100us != monet_data.ble_conn_param[channel].min_100us) &&
                        (monet_data.ble_conn_param[channel].min_100us))
                    {
                        (monet_data.ble_conn_param_check_in_heartbeat[channel])++;
                        if (monet_data.ble_conn_param_check_in_heartbeat[channel] >= BLE_CONN_PARM_CHECK_IN_HEARTBEAT_COUNT)
                        {
                            monet_data.ble_conn_param_check_in_heartbeat[channel] = 0;
                            monet_data.ble_conn_param_update_ok[channel] = 0xff;
                            NRF_LOG_RAW_INFO(">>>ble_conn_param_check_in_heartbeat.\r");
                        }
                    }
                    else
                    {
                        monet_data.ble_conn_param_check_in_heartbeat[channel] = 0;
                    }
                }
            }

            NRF_LOG_RAW_INFO(">>>CH: %d HB(%d) Min(%d) Out(%d) On(%d) Act(%d).\r",
                            channel, beat, min_100us, timeout_100us, camera_on, camera_act);
            return;
        }
        #endif /* BLE_CONNECTION_SUPPORT_HEARTBEAT */

        if ((p_data[0] == 0x66) && (p_data[1] == 0x00))
        {
            pf_gpio_pattern_set(p_data + 2, data_len - 2);
            NRF_LOG_INFO("AUS GPIO Control(%d:%d).", p_data[2], p_data[3]);
            return;
        }
        if (p_data[0] == 0xDD)
        {
            NRF_LOG_RAW_INFO("Receive nala 0xDD command 1 \r");
        }
        else
        {
            if (p_data[0] == 0xAA)
            {
                CRITICAL_REGION_ENTER();
                monet_data.mcuDataHandleMs = MCU_DATA_HANDLE_TIMEOUT_MS;		
                CRITICAL_REGION_EXIT();
                NRF_LOG_RAW_INFO("AUS AA Com(%x) InstID(%d) BM_Len(%d) ", 
                                p_data[1], 
                                p_data[2],
                                data_len);
                NRF_LOG_RAW_INFO("BM(%02x:%02x:%02x:%02x)\r", 
                                p_data[3], 
                                p_data[4], 
                                p_data[5], 
                                p_data[6]);
            }
            else
            {
                CRITICAL_REGION_ENTER();
                monet_data.RecvDataEvent = 1;
                CRITICAL_REGION_EXIT();
            }
        }

        ble_recv_data_push((uint8_t *)p_evt->params.rx_data.p_data, p_evt->params.rx_data.length, channel);
        camera_poweroff_delay_refresh();
        #if BLE_DATA_THROUGHPUT_ERROR_CHECK_EN
        ble_data_throughput_check_correction((uint8_t *)p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
        #endif /* BLE_DATA_THROUGHPUT_ERROR_CHECK_EN */

        // NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
        // NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);
    }
    else if (p_evt->type == BLE_NUS_EVT_COMM_STARTED)
    {
        #if BLE_DATA_THROUGHPUT_ERROR_CHECK_EN
        ble_data_throughput_check_correction_reset();
        #endif /* BLE_DATA_THROUGHPUT_ERROR_CHECK_EN */

        channel = ble_channel_get_from_handler(p_evt->conn_handle);
        NRF_LOG_RAW_INFO("AUS_EVT_COMM_STARTED CH: %d.\r", channel);
        NRF_LOG_FLUSH();

        if (channel != 0xffff)
        {
            CRITICAL_REGION_ENTER();
            ble_connection_status_refresh();
            monet_data.bleConnectionEvent = 1 + channel;
            monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_CONNECTED;
            CRITICAL_REGION_EXIT();
        }
    }
    else if (p_evt->type == BLE_NUS_EVT_TX_RDY)
    {
        ble_data_send_with_queue();
    }
}
#endif /* BLE_FUNCTION_ONOFF */
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}
#endif /* BLE_FUNCTION_ONOFF */

/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
// static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
// {
//     uint32_t err_code;

//     if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
//     {
//         err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
//         APP_ERROR_CHECK(err_code);
//     }
// }


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
// static void conn_params_error_handler(uint32_t nrf_error)
// {
//     APP_ERROR_HANDLER(nrf_error);
// }


/**@brief Function for initializing the Connection Parameters module.
 */
#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    // cp_init.evt_handler                 = on_conn_params_evt;
    cp_init.evt_handler                    = NULL;
    // cp_init.error_handler               = conn_params_error_handler;
    cp_init.error_handler                  = NULL;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}
#endif /* BLE_FUNCTION_ONOFF */

/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
// static void sleep_mode_enter(void)
// {
//     uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
//     APP_ERROR_CHECK(err_code);

//     // Prepare wakeup buttons.
//     err_code = bsp_btn_ble_sleep_mode_prepare();
//     APP_ERROR_CHECK(err_code);

//     // Go to system-off mode (this function will not return; wakeup will cause a reset).
//     err_code = sd_power_system_off();
//     APP_ERROR_CHECK(err_code);
// }


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    // uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
            // err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            // APP_ERROR_CHECK(err_code);
            break;
        case BLE_ADV_EVT_IDLE:
            // sleep_mode_enter();
            break;
        case BLE_ADV_EVT_FAST_WHITELIST:
            NRF_LOG_INFO("Fast advertising with Whitelist");
            break;
        case BLE_ADV_EVT_WHITELIST_REQUEST:
        {
            // Apply the whitelist.
            APP_ERROR_CHECK(ble_advertising_whitelist_reply(&m_advertising, whitelist_addrs, whitelist_count, NULL, 0));
        }
        break;
        default:
            break;
    }
}
#endif /* BLE_FUNCTION_ONOFF */

char const * phy_str(ble_gap_phys_t phys)
{
    static char const * str[] =
    {
        "1 Mbps",
        "2 Mbps",
        "Coded",
        "Unknown"
    };

    switch (phys.tx_phys)
    {
        case BLE_GAP_PHY_1MBPS:
            return str[0];

        case BLE_GAP_PHY_2MBPS:
        case BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS:
        case BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_CODED:
            return str[1];

        case BLE_GAP_PHY_CODED:
            return str[2];

        default:
            return str[3];
    }
}

/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;
    #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
    uint16_t channel = 0xffff;
    #endif /* BLE_FUNCTION_ONOFF */

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            // err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            // APP_ERROR_CHECK(err_code);
            CRITICAL_REGION_ENTER();
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            monet_data.ble_disconnnet_delayTimeMs = 0;
            CRITICAL_REGION_EXIT();
            #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
            channel = ble_information_set(p_ble_evt->evt.gap_evt.conn_handle,
                                BLE_CONNECTION_STATUS_CONNECTED,
                                (uint8_t *)(p_ble_evt->evt.gap_evt.params.connected.peer_addr.addr));
            NRF_LOG_RAW_INFO("Connected CH: %d.\r", channel);
            monet_data.is_advertising = 0;
            monet_data.to_advertising_recover = 0;
            #endif /* BLE_FUNCTION_ONOFF */
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
            err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_CONN, m_conn_handle, tx_power_level_default);
            APP_ERROR_CHECK(err_code);
            {
                NRF_LOG_RAW_INFO("PHY update connected.\r");
                ble_gap_phys_t const phys =
                {
                    .rx_phys = BLE_GAP_PHY_2MBPS,
                    .tx_phys = BLE_GAP_PHY_2MBPS,
                };
                err_code = sd_ble_gap_phy_update(m_conn_handle, &phys);
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
            channel = ble_channel_get_from_handler(p_ble_evt->evt.gap_evt.conn_handle);
            NRF_LOG_RAW_INFO("Disconnected CH: %d\r", channel);
            monet_data.is_advertising = 0;
            monet_data.to_advertising = 1;
            #endif /* BLE_FUNCTION_ONOFF */
            // LED indication will be changed when advertising starts.
            CRITICAL_REGION_ENTER();
            m_conn_handle = BLE_CONN_HANDLE_INVALID;

            #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
            if (channel != 0xffff)
            {
                monet_data.bleDisconnectEvent = 1 + channel;
                monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_NOT_CONNECTED;
                monet_data.ble_info[channel].handler = 0xffff;
            }
            #endif /* BLE_FUNCTION_ONOFF */
            CRITICAL_REGION_EXIT();
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
            #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
            channel = ble_channel_get_from_handler(p_ble_evt->evt.gap_evt.conn_handle);
            NRF_LOG_RAW_INFO(">>>EVT_CONN_PARAM_UPDATE CH(%d) Min(%d) Max(%d) Late(%d) Timeout(%d).\r",
                         channel,
                         p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.min_conn_interval,
                         p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.max_conn_interval,
                         p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.slave_latency,
                         p_ble_evt->evt.gap_evt.params.conn_param_update.conn_params.conn_sup_timeout);
            #endif /* BLE_FUNCTION_ONOFF */
            break;
        
        case BLE_GAP_EVT_PHY_UPDATE:
        {
            ble_gap_evt_phy_update_t const * p_phy_evt = &p_ble_evt->evt.gap_evt.params.phy_update;

            if (p_phy_evt->status == BLE_HCI_STATUS_CODE_LMP_ERROR_TRANSACTION_COLLISION)
            {
                // Ignore LL collisions.
                NRF_LOG_RAW_INFO("LL transaction collision during PHY update.\r");
                break;
            }

            ble_gap_phys_t phys = {0};
            phys.tx_phys = p_phy_evt->tx_phy;
            phys.rx_phys = p_phy_evt->rx_phy;
            NRF_LOG_RAW_INFO("PHY update %s. PHY set to %s.\r",
                             (p_phy_evt->status == BLE_HCI_STATUS_CODE_SUCCESS) ?
                             "accepted" : "rejected",
                             phy_str(phys));
        }
        break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_RAW_INFO("PHY update request.\r");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        // case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
        //     // Pairing not supported
        //     err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
        //     APP_ERROR_CHECK(err_code);
        //     break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            NRF_LOG_INFO("BLE_GAP_EVT_SEC_PARAMS_REQUEST");
            break;

        // case BLE_GAP_EVT_PASSKEY_DISPLAY:
        // {
        //     char passkey[BLE_GAP_PASSKEY_LEN + 1];
        //     memcpy(passkey, p_ble_evt->evt.gap_evt.params.passkey_display.passkey, BLE_GAP_PASSKEY_LEN);
        //     passkey[BLE_GAP_PASSKEY_LEN] = 0x00;
        //     NRF_LOG_INFO("=== PASSKEY: %s =====", nrf_log_push(passkey));
        // }
        // break;

        case BLE_GAP_EVT_AUTH_KEY_REQUEST:
            NRF_LOG_INFO("BLE_GAP_EVT_AUTH_KEY_REQUEST");
            break;

        case BLE_GAP_EVT_LESC_DHKEY_REQUEST:
            NRF_LOG_INFO("BLE_GAP_EVT_LESC_DHKEY_REQUEST");
            break;

         case BLE_GAP_EVT_AUTH_STATUS:
             NRF_LOG_INFO("BLE_GAP_EVT_AUTH_STATUS: status=0x%x bond=0x%x lv4: %d kdist_own:0x%x kdist_peer:0x%x",
                          p_ble_evt->evt.gap_evt.params.auth_status.auth_status,
                          p_ble_evt->evt.gap_evt.params.auth_status.bonded,
                          p_ble_evt->evt.gap_evt.params.auth_status.sm1_levels.lv4,
                          *((uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_own),
                          *((uint8_t *)&p_ble_evt->evt.gap_evt.params.auth_status.kdist_peer));
            // WARNING: If pair fail will it still advertise?
            monet_data.in_pairing_mode = (p_ble_evt->evt.gap_evt.params.auth_status.bonded + 1) % 2;
            monet_data.pairing_success = p_ble_evt->evt.gap_evt.params.auth_status.bonded;
            if (monet_data.pairing_success)
            {
                CRITICAL_REGION_ENTER();
                reset_info.ble_peripheral_paired = 1;
                CRITICAL_REGION_EXIT();
            }
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        CRITICAL_REGION_ENTER();
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        CRITICAL_REGION_EXIT();
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
// void bsp_event_handler(bsp_event_t event)
// {
//     uint32_t err_code;
//     switch (event)
//     {
//         case BSP_EVENT_SLEEP:
//             sleep_mode_enter();
//             break;

//         case BSP_EVENT_DISCONNECT:
//             err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
//             if (err_code != NRF_ERROR_INVALID_STATE)
//             {
//                 APP_ERROR_CHECK(err_code);
//             }
//             break;

//         case BSP_EVENT_WHITELIST_OFF:
//             if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
//             {
//                 err_code = ble_advertising_restart_without_whitelist(&m_advertising);
//                 if (err_code != NRF_ERROR_INVALID_STATE)
//                 {
//                     APP_ERROR_CHECK(err_code);
//                 }
//             }
//             break;

//         default:
//             break;
//     }
// }

/**@brief Function for the Peer Manager initialization.
 */
static void erase_fds_areas(void);
static void peer_manager_init(void)
{
    ble_gap_sec_params_t sec_param;
    ret_code_t           err_code;

    err_code = pm_init();

    if (err_code != NRF_SUCCESS)
    {
        erase_fds_areas();
    }
    APP_ERROR_CHECK(err_code);

    memset(&sec_param, 0, sizeof(ble_gap_sec_params_t));

    // Security parameters to be used for all security procedures.
    sec_param.bond           = SEC_PARAM_BOND;
    sec_param.mitm           = SEC_PARAM_MITM;
    sec_param.lesc           = SEC_PARAM_LESC;
    sec_param.keypress       = SEC_PARAM_KEYPRESS;
    sec_param.io_caps        = SEC_PARAM_IO_CAPABILITIES;
    sec_param.oob            = SEC_PARAM_OOB;
    sec_param.min_key_size   = SEC_PARAM_MIN_KEY_SIZE;
    sec_param.max_key_size   = SEC_PARAM_MAX_KEY_SIZE;
    sec_param.kdist_own.enc  = 1;
    sec_param.kdist_own.id   = 1;
    sec_param.kdist_peer.enc = 1;
    sec_param.kdist_peer.id  = 1;

    err_code = pm_sec_params_set(&sec_param);
    APP_ERROR_CHECK(err_code);

    err_code = pm_register(pm_evt_handler);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the Advertising functionality.
 */
#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
void advertising_init(bool adv_on_disconnect, uint32_t adv_interval)
{
    uint32_t               err_code;
    ble_advertising_init_t init;
    // int8_t tx_power_level = tx_power_level_default;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    // init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_LIMITED_DISC_MODE;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
    // init.advdata.p_tx_power_level   = &tx_power_level;
    init.advdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.advdata.uuids_complete.p_uuids  = m_adv_uuids;

    // init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    // init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    m_manuf_specific_data.company_identifier = APP_COMPANY_IDENTIFIER;
    m_manuf_specific_data.data.size = APP_BEACON_INFO_LENGTH;
    m_manuf_specific_data.data.p_data = (uint8_t *)m_beacon_info;
    init.srdata.p_manuf_specific_data = (ble_advdata_manuf_data_t *)&m_manuf_specific_data;

    init.config.ble_adv_on_disconnect_disabled = adv_on_disconnect;
    init.config.ble_adv_whitelist_enabled = true;
    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = adv_interval;
    init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
    init.config.ble_adv_fast_timeout  = 0;
    // init.config.ble_adv_primary_phy   = BLE_GAP_PHY_1MBPS; // Must be changed to connect in long range. (BLE_GAP_PHY_CODED)
    // init.config.ble_adv_secondary_phy = BLE_GAP_PHY_1MBPS;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}

void advertising_init_slow(void)
{
    advertising_init(true, APP_ADV_INTERVAL);
    advertising_start(false);
}

void advertising_init_fast(void)
{
    advertising_init(true, APP_ADV_INTERVAL / 6);
    advertising_start(false);
}

void pf_ble_adv_start_without_whitelist(void)
{
    ret_code_t err_code;
    if (monet_data.is_advertising == 1)
    {
        pf_log_raw(atel_log_ctl.core_en, "pf_ble_adv_start_without_whitelist: already.\r");
        return;
    }

    err_code = ble_advertising_restart_without_whitelist(&m_advertising);

    pf_log_raw(atel_log_ctl.core_en, "ble_advertising_restart_without_whitelist: %d.\r", err_code);

    CRITICAL_REGION_ENTER();
    monet_data.to_advertising_result = err_code;
    if (monet_data.to_advertising_result == NRF_SUCCESS)
    {
        monet_data.is_advertising = 1;
    }

    if (err_code)
    {
        if (err_code == NRF_ERROR_INVALID_PARAM)
        {
            monet_data.to_advertising_without_white_list = 1;
        }

        monet_data.to_advertising_recover = 1;
    }
    else
    {
        monet_data.to_advertising_recover = 0;
    }
    CRITICAL_REGION_EXIT();
}

void pf_ble_adv_start(void)
{
    if (monet_data.in_pairing_mode == 1)
    {
        advertising_init_fast();
    }
    else
    {
        advertising_init_slow();
    }
}

void pf_ble_adv_connection_reset(void)
{
    uint32_t err_code = 0;

    CRITICAL_REGION_ENTER();
    monet_data.is_advertising = 0;
    CRITICAL_REGION_EXIT();

    if(m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        pf_log_raw(atel_log_ctl.core_en, "ble_adv_connection_reset Handler(%d) Err(%d).\r", m_conn_handle, err_code);
    }
    else
    {
        pf_log_raw(atel_log_ctl.core_en, "ble_adv_connection_reset Adv_Stop Handler(%d).\r", m_advertising.adv_handle);
        sd_ble_gap_adv_stop(m_advertising.adv_handle);
    }
}

void pf_ble_white_list_reset(void)
{
    uint32_t ret = 0;
    ret = sd_ble_gap_whitelist_set(NULL, 0);
    pf_log_raw(atel_log_ctl.core_en, "pf_ble_white_list_reset(%d).\r", ret);
}

void pf_ble_delete_bonds(void)
{
    pf_log_raw(atel_log_ctl.core_en, "pf_ble_delete_bonds.\r");
    delete_bonds();
}

void pf_ble_load_white_list(void)
{
    static uint8_t entered = 0;
    pm_peer_id_t current_peer_id = PM_PEER_ID_INVALID;

    if (entered)
    {
        pf_log_raw(atel_log_ctl.error_en, "pf_ble_load_white_list Entered.\r");
        return;
    }

    entered = 1;

    current_peer_id = pm_next_peer_id_get(PM_PEER_ID_INVALID);
    if (current_peer_id != PM_PEER_ID_INVALID)
    {
    	pm_peer_data_bonding_t bonding_data;
        uint8_t * p_addr = NULL;
    	pm_peer_data_bonding_load(current_peer_id, &bonding_data);
        p_addr = bonding_data.peer_ble_id.id_addr_info.addr;
    	pf_log_raw(atel_log_ctl.core_en, 
                   "pf_ble_load_white_list MAC Addr(%02x:%02x:%02x:%02x:%02x:%02x)\r", 
                   p_addr[0], 
                   p_addr[1], 
                   p_addr[2], 
                   p_addr[3], 
                   p_addr[4], 
                   p_addr[5]);
        if ((p_addr[1] | p_addr[2] | p_addr[3] | p_addr[4] | p_addr[5] | p_addr[6]) == 0)
        {
            pf_log_raw(atel_log_ctl.error_en, "pf_ble_load_white_list Mac Err.\r");
            return;
        }
        ble_information_set(0xffff, 
                            BLE_CONNECTION_STATUS_MAC_SET, 
                            p_addr);
        ble_aus_white_list_set();

        CRITICAL_REGION_ENTER();
        reset_info.ble_peripheral_paired = 1;
        CRITICAL_REGION_EXIT();
    }
    else
    {
        pf_log_raw(atel_log_ctl.error_en, "pf_ble_load_white_list no valid.\r");

        CRITICAL_REGION_ENTER();
        reset_info.ble_peripheral_paired = 0;
        CRITICAL_REGION_EXIT();

        if (reset_info.reset_from_shipping_mode == 1)
        {
            CRITICAL_REGION_ENTER();
            reset_info.reset_from_shipping_mode_donot_adv_untill_pair = 1;
            CRITICAL_REGION_EXIT();
            pf_log_raw(atel_log_ctl.error_en, "reset_from_shipping_mode_donot_adv_untill_pair.\r");
        }
    }
}

void pf_ble_peer_tx_rssi_get(int16_t *p_rssi)
{
    if(m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        int8_t rssi = 0;
        uint8_t ch_index = 0;

        sd_ble_gap_rssi_start(m_conn_handle,BLE_GAP_RSSI_THRESHOLD_INVALID, 0);
        pf_delay_ms(1);
        sd_ble_gap_rssi_get(m_conn_handle, &rssi, &ch_index);

        *p_rssi = (int16_t)rssi;

        pf_log_raw(atel_log_ctl.core_en, 
                   "pf_ble_peer_tx_rssi_get conn(%d) rssi(%d) channel(%d).\r",
                   m_conn_handle,
                   rssi,
                   ch_index);
    }
    else
    {
        pf_log_raw(atel_log_ctl.core_en, 
                   "pf_ble_peer_tx_rssi_get conn_handle invalid.\r");
    }
}

void pf_ble_peer_tx_rssi_stop(void)
{
    if(m_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        sd_ble_gap_rssi_stop(m_conn_handle);
        pf_log_raw(atel_log_ctl.core_en, 
                   "pf_ble_peer_tx_rssi_stop conn_handle(%d).\r",
                   m_conn_handle);
    }
}

#endif /* BLE_FUNCTION_ONOFF */

/**@brief Function for initializing buttons and leds.
 *
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
// static void buttons_leds_init(bool * p_erase_bonds)
// {
//     bsp_event_t startup_event;

//     uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
//     APP_ERROR_CHECK(err_code);

//     err_code = bsp_btn_ble_init(NULL, &startup_event);
//     APP_ERROR_CHECK(err_code);

//     *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
// }


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_lesc_request_handler();
    APP_ERROR_CHECK(err_code);

    if (NRF_LOG_PROCESS() == false)
    {
        if (monet_data.cameraPowerOn == 0)
        {
            nrf_pwr_mgmt_run();
        }
    }
}

void pf_device_enter_idle_state(void)
{
    idle_state_handle();
}

static void device_power_state_restore(void)
{
    // Turn on Camera
    nrf_gpio_cfg_output(P106_DCDC_3V3_EN);
    nrf_gpio_cfg_output(P022_BLE_ESP_EN);
    nrf_gpio_pin_clear(P106_DCDC_3V3_EN);
    nrf_gpio_pin_set(P022_BLE_ESP_EN);
}


//0616 22 wfy add
//UICR register store in flash, we can store some data and record it when device no power in last times
 void pf_uicr_record_restart(void)
{          
      NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos; 
      while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {} 
      if (NRF_UICR->CUSTOMER[0] == 0xFFFFFFFF)
      {
          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;  
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}          
          NRF_NVMC->ERASEUICR = NVMC_ERASEUICR_ERASEUICR_Erase << NVMC_ERASEUICR_ERASEUICR_Pos;  
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;  
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {} 
          NRF_UICR->CUSTOMER[0] = 0x00;    
          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos; 
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {} 
          NRF_LOG_RAW_INFO("First Init UICR regester \r");
      }
      else
      {
          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos; 
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {} 
          debug_tlv_info.recordTimes = NRF_UICR->CUSTOMER[0];
          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een << NVMC_CONFIG_WEN_Pos;  
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}          
          NRF_NVMC->ERASEUICR = NVMC_ERASEUICR_ERASEUICR_Erase << NVMC_ERASEUICR_ERASEUICR_Pos;   
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {} 
          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen << NVMC_CONFIG_WEN_Pos;  
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {} 
          NRF_UICR->CUSTOMER[0] = debug_tlv_info.recordTimes + 1;  
          NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren << NVMC_CONFIG_WEN_Pos; 
          while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
          //NRF_LOG_RAW_INFO("debug_tlv_info.recordTimes 0x%08x 0x%08x \r",NRF_UICR->CUSTOMER[0],NRF_UICR->CUSTOMER[1]);
      }
      debug_tlv_info.resumeTimes = NRF_UICR->CUSTOMER[0];
      NRF_LOG_RAW_INFO("The device restart times 0x%08x \r",debug_tlv_info.resumeTimes);
 
}

// static void clock_init(void) //ZAZUMCU-149
// {
//     ret_code_t err_code = nrf_drv_clock_init();	
//     if ((err_code != NRF_SUCCESS) &&
//         (err_code != NRF_ERROR_MODULE_ALREADY_INITIALIZED))
//     {
//         APP_ERROR_CHECK(err_code);
//     }

//     clock_hfclk_request();
// }


/**@brief Application main function.
 */
int main(void)
{
    nrf_gpio_cfg_output(P013_SOLAR_CHARGER_SWITCH);
    nrf_gpio_pin_set(P013_SOLAR_CHARGER_SWITCH);
    nrf_gpio_cfg_output(P021_CHRG_SLEEP_EN);
    nrf_gpio_pin_clear(P021_CHRG_SLEEP_EN);

    pf_uicr_record_restart();
    // bool erase_bonds;

    // Initialize.
    // uart_init();

    // Turn on system power
    #if DEVICE_POWER_KEY_SUPPORT
    nrf_gpio_pin_set(P011_VSYS_EN);
    nrf_gpio_cfg_output(P011_VSYS_EN);
    #else
    nrf_gpio_cfg_default(P011_VSYS_EN);
    #endif /* DEVICE_POWER_KEY_SUPPORT */

    log_init();

    //clock_init(); //ZAZUMCU-149::Need to manually switch to the external 32 MHz crystal
    uint8_t p_buff[4] = {0};
    p_buff[0] = (DFU_FLAG_REGISTER2 >> 24) & 0xff;
    p_buff[1] = (DFU_FLAG_REGISTER2 >> 16) & 0xff;
    p_buff[2] = (DFU_FLAG_REGISTER2 >>  8) & 0xff;
    p_buff[3] = (DFU_FLAG_REGISTER2 >>  0) & 0xff;
    NRF_LOG_RAW_INFO("Read Bootloder version %d %d %d %d \r",p_buff[3],p_buff[2],p_buff[1],p_buff[0]);
    if (DFU_FLAG_REGISTER == DFU_FLAG_VALUE_FROM_BOOT)
    {
        device_power_state_restore();

        #if BLE_DTM_ENABLE
        if (DFU_FLAG_REGISTER1 == DFU_FLAG_VALUE_FROM_DTME)
		//if (DEVICE_RESET_SOURCE_BIT_IS_VALID(DFU_FLAG_REGISTER1,DEVICE_RESET_FROM_DTME))
        {
            pf_dtm_init();
            pf_dtm_process();
        }
        else if (DFU_FLAG_REGISTER1 == DFU_FLAG_VALUE_FROM_DTMEX)
		//else if (DEVICE_RESET_SOURCE_BIT_IS_VALID(DFU_FLAG_REGISTER1,DEVICE_RESET_FROM_DTMEX))
        {
            NRF_LOG_RAW_INFO("Reset from DTMEX\r");
        }
        #endif /* BLE_DTM_ENABLE */

        DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_INVALID;
        DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
        //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_INVALID);
        reset_info.button_power_on = 1;
        reset_info.reset_from_dfu = 1;
        NRF_LOG_RAW_INFO("Reset from DFU\r");
    }
    else
    {
        if (DFU_FLAG_REGISTER == DFU_FLAG_VALUE_FROM_SHIPPING_MODE)
        {
            DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_INVALID;
            DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
            //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_INVALID);
            reset_info.reset_from_shipping_mode = 1;
            NRF_LOG_RAW_INFO("Reset from Shipping Mode\r");
        }
        // Turn off Camera
        pf_camera_pwr_init();
        pf_camera_pwr_ctrl(false);
        NRF_LOG_RAW_INFO("Reset Camera Power Off\r");
    }

    if (DFU_FLAG_REGISTER == DFU_FLAG_VALUE_FROM_POWEROFF)
    {
        DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_INVALID;
        DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
        //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_INVALID);
        NRF_LOG_RAW_INFO("Reset from Power Off\r");
    }
 
    debug_tlv_info.restartReason = NRF_POWER->RESETREAS;
    NRF_POWER->RESETREAS = NRF_POWER->RESETREAS;
    NRF_LOG_RAW_INFO("The resume reason 0x%08x  %d \r",debug_tlv_info.restartReason,debug_tlv_info.resumeTimes);


    pf_wdt_init();

    timers_init();
    // buttons_leds_init(&erase_bonds);
    power_management_init();
    ble_stack_init();

    monet_data.batCriticalThresh = (BUB_CRITICAL_THRESHOLD + BUB_CRITICAL_THRESHOLD_FIRST_DELTA );  // Threshold  MV
    monet_data.batPowerCritical = 0;
    device_low_power_mode_check(0);

    gap_params_init();
    gatt_init();
    #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
    services_init();
    advertising_init(true, APP_ADV_INTERVAL);
    conn_params_init();

    peer_manager_init();
    #endif /* BLE_FUNCTION_ONOFF */

    // Start execution.
    // printf("\r\nZazu UART started: %d.%d.%d.%d.\r\n", MNT_MAJOR, MNT_MINOR, MNT_REVISION, MNT_BUILD);
    #if FACTORY_TESTING_FUNCTION
    NRF_LOG_INFO("Zazu Factory Started: %d.%d.%d.%d.\r", 0xff, MNT_MINOR, MNT_REVISION, MNT_BUILD);
    #else
    NRF_LOG_INFO("Zazu Started: %d.%d.%d.%d.\r", MNT_MAJOR, MNT_MINOR, MNT_REVISION, MNT_BUILD);
    #endif /* FACTORY_TESTING_FUNCTION */

    sd_ble_gap_addr_get(&peripheral_addr);
    NRF_LOG_RAW_INFO("id_peer(%d) add_type(%d).\r", peripheral_addr.addr_id_peer, peripheral_addr.addr_type);
    NRF_LOG_RAW_INFO("Self Mac:(%02x:%02x:%02x:%02x:%02x:%02x)\r",
                         peripheral_addr.addr[0],
                         peripheral_addr.addr[1],
                         peripheral_addr.addr[2],
                         peripheral_addr.addr[3],
                         peripheral_addr.addr[4],
                         peripheral_addr.addr[5]
                         );
    NRF_LOG_FLUSH();

    #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
    uint32_t err_code;
    err_code = sd_ble_gap_tx_power_set(BLE_GAP_TX_POWER_ROLE_ADV, m_advertising.adv_handle, tx_power_level_default);
    APP_ERROR_CHECK(err_code);
    pf_ble_load_white_list();
    #endif /* BLE_FUNCTION_ONOFF */

    #if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
    ble_send_recv_init();
    #endif /* BLE_FUNCTION_ONOFF */

    #if BLE_ADVERTISING_ENABLE_BY_DEFAULT
    // ble_data_test(0);
    // uint8_t addr_filter[BLE_GAP_ADDR_LEN] = {0x5E, 0x3A, 0x30, 0x99, 0x91, 0xC5}; // 52832 DEV Board
    // ble_aus_set_adv_filter(addr_filter);
    ble_aus_advertising_start();
    #endif /* BLE_ADVERTISING_ENABLE_BY_DEFAULT */

    if (!nrfx_gpiote_is_init()) {

        if (nrfx_gpiote_init() != NRF_SUCCESS) {
            pf_log_raw(atel_log_ctl.error_en, "nrfx_gpiote_init fail.\r");
        }
    }

    InitApp(0);

    memcpy(monet_data.ble_mac_addr, peripheral_addr.addr, BLE_MAC_ADDRESS_LEN);
    if (ZAZU_HARDWARE_VERSION == ZAZU_HARDWARE_NOCAN_WITHCHARGER)
    {
        if (monet_data.CANExistChargerNoExist == 0)
        {
            monet_data.bleShouldAdvertise = 1;
            if (reset_info.reset_from_shipping_mode_donot_adv_untill_pair == 0)
            {
                monet_data.to_advertising = 1;
            }
        }
    }

    ble_inform_camera_mac();

    // Enter main loop.
    for (;;)
    {
#if !(BLE_BYPASS_TEST_ENABLE)
        
        idle_state_handle();

        atel_io_queue_process();

        atel_timerTickHandler(monet_data.sysTickUnit);

        #if TIME_UNIT_CHANGE_WHEN_SLEEP
        pf_systick_change();
        #endif /* TIME_UNIT_CHANGE_WHEN_SLEEP */

        pf_gpio_pattern_process(GPIO_PATTERN_TIMER_PERIOD_MS);

        // Check if something to send
        CheckInterrupt();

        ble_send_data_rate_show(0);

        #if SPI_CAN_CONTROLLER_EN
        if ((monet_data.CANExistChargerNoExist == 1) && 
            (monet_data.bleShouldAdvertise == 0))
        {
            can_controller_process((can_rx_pool_queue_is_full() == 0), pf_systime_get());
        }
        #endif /* SPI_CAN_CONTROLLER_EN */

        atel_adc_converion(0, 1);

        if (monet_data.bleShouldAdvertise == 1)
        {
            if (monet_data.to_advertising != 0)
            {
                // pf_ble_adv_start();
                // CRITICAL_REGION_ENTER();
                // monet_data.to_advertising = 0;
                // CRITICAL_REGION_EXIT();
                CRITICAL_REGION_ENTER();
                if (monet_data.to_advertising_without_white_list == 1)
                {
                    monet_data.to_advertising_without_white_list = 0;
                    pf_ble_adv_start_without_whitelist();
                }
                else
                {
                    pf_ble_adv_start();
                }
                monet_data.to_advertising = 0;
                CRITICAL_REGION_EXIT();
            }

            if (monet_data.pairing_success != 0)
            {
                monet_data.pairing_success = 0;
                pf_device_pairing_success();
                ble_aus_white_list_set();
            }
        }

        device_button_pattern_process();
#else
        idle_state_handle();
        ble_send_timer_start();
        // idle_state_handle();
        pf_wdt_kick();
        NRF_LOG_FLUSH();

        if (is_ble_recv_queue_empty() == 0)
        {
            uint8_t *tmp_buf;
            uint16_t tmp_len = 0;
            uint16_t tmp_ch = 0;

            if (is_ble_send_queue_full() == 0)
            {
                ble_recv_data_pop(&tmp_buf, &tmp_len, &tmp_ch);
                ble_send_data_push(tmp_buf, tmp_len, tmp_ch);
                ble_recv_data_delete_one();
            }
        }

        atel_timerTickHandler(monet_data.sysTickUnit);

        pf_systick_change();
        
        pf_gpio_pattern_process(GPIO_PATTERN_TIMER_PERIOD_MS);

        // Check if something to send
        CheckInterrupt();

        ble_send_data_rate_show(0);
        #if 0
        static uint32_t onway_tx_count = 0;
        uint8_t buf[128] = {0};
        memset(buf, (onway_tx_count % 26) + 'a', 128);
        buf[126] = '\r';
        buf[127] = '\n';
        memcpy(buf, &onway_tx_count, 4);
        if (ring_buffer_left_get(&monet_data.txQueueU1) > (2 * (BLE_DATA_RECV_BUFFER_CELL_LEN + 8)))
        {
            BuildFrame(IO_CMD_CHAR_BLE_RECV, buf, 128);
            onway_tx_count++;
            nrf_delay_ms(20);
        }
        #endif /* 0 */
#endif /* BLE_BYPASS_TEST_ENABLE */
    }
}

#if (BLE_FUNCTION_ONOFF == BLE_FUNCTION_ON)
ret_code_t ble_aus_data_send_periheral(uint8_t * p_data, uint16_t data_len, uint16_t channel)
{
    ret_code_t ret_val;
    if (monet_data.ble_info[channel].connect_status != BLE_CONNECTION_STATUS_CONNECTED)
    {
        // pf_log_raw(atel_log_ctl.error_en, "ble_aus_data_send_periheral channel(%d) connect_status(%d).\r", 
        //            channel,
        //            monet_data.ble_info[channel].connect_status);
        monet_data.ble_send_result.channel = channel;
        monet_data.ble_send_result.status = monet_data.ble_info[channel].connect_status;
        monet_data.ble_send_result.error = NRF_ERROR_INVALID_STATE;
        return NRF_ERROR_INVALID_STATE;
    }
    // do
    // {
    //     ret_val = ble_nus_data_send(&m_nus, p_data, &data_len, monet_data.ble_info[channel].handler);
    //     if ((ret_val != NRF_ERROR_INVALID_STATE) &&
    //         (ret_val != NRF_ERROR_RESOURCES) &&
    //         (ret_val != NRF_ERROR_NOT_FOUND) &&
    //         (ret_val != NRF_ERROR_BUSY))
    //     {
    //         APP_ERROR_CHECK(ret_val);
    //     }
    // }while (ret_val == NRF_ERROR_RESOURCES || ret_val == NRF_ERROR_BUSY);
    ret_val = ble_nus_data_send(&m_nus, p_data, &data_len, monet_data.ble_info[channel].handler);
    return ret_val;
}

uint16_t get_ble_aus_max_data_len(void)
{
    return m_ble_nus_max_data_len;
}

uint32_t ble_aus_white_list_set(void)
{
    uint32_t err_code;
    uint16_t i = 0;
    ble_gap_addr_t const * addr_ptrs[BLE_CHANNEL_NUM_MAX];
    uint8_t addr_buf[BLE_CHANNEL_NUM_MAX * BLE_MAC_ADDRESS_LEN] = {0};
    uint8_t *addr = addr_buf;
    uint16_t channel = 0, len = 0;

    for (channel = 0; channel < BLE_CHANNEL_NUM_MAX; channel++)
    {
        if (((monet_data.ble_info[channel].connect_status <= BLE_CONNECTION_STATUS_MAC_SET) &&
            (monet_data.ble_info[channel].connect_status != BLE_CONNECTION_STATUS_NOTVALID)))
        {
            memcpy(addr + len, monet_data.ble_info[channel].mac_addr, BLE_MAC_ADDRESS_LEN);
            len += BLE_MAC_ADDRESS_LEN;
        }
    }

    whitelist_count = len / BLE_GAP_ADDR_LEN;

    for (i = 0; i < whitelist_count; i++)
    {
        whitelist_addrs[i].addr_id_peer = 0;
        whitelist_addrs[i].addr_type = BLE_GAP_ADDR_TYPE_RANDOM_STATIC;
        whitelist_addrs[i].addr[0] = addr[0];
        whitelist_addrs[i].addr[1] = addr[1];
        whitelist_addrs[i].addr[2] = addr[2];
        whitelist_addrs[i].addr[3] = addr[3];
        whitelist_addrs[i].addr[4] = addr[4];
        whitelist_addrs[i].addr[5] = addr[5];
        addr += BLE_GAP_ADDR_LEN;
        addr_ptrs[i] = whitelist_addrs + i;
        pf_log_raw(atel_log_ctl.core_en, "whitelist Mac:(0x%02x:0x%02x:0x%02x:0x%02x:0x%02x:0x%02x)\r",
                         whitelist_addrs[i].addr[0],
                         whitelist_addrs[i].addr[1],
                         whitelist_addrs[i].addr[2],
                         whitelist_addrs[i].addr[3],
                         whitelist_addrs[i].addr[4],
                         whitelist_addrs[i].addr[5]
                         );
    }

    if (whitelist_count == 0)
    {
        pf_log_raw(atel_log_ctl.core_en, "sd_ble_gap_whitelist_set clear.\r");
        err_code = sd_ble_gap_whitelist_set(NULL, 0);
    }
    else
    {
        err_code = sd_ble_gap_whitelist_set(addr_ptrs, whitelist_count);
    }
    pf_log_raw(atel_log_ctl.core_en, "sd_ble_gap_whitelist_set: %d.\r", err_code);

    return err_code;
}

void ble_aus_set_adv_filter(uint8_t *p_addr)
{
    // TODO: to implement multi-channel
    uint32_t err_code = 0;
    static uint8_t filter_set = 0;

    if ((p_addr[1] | p_addr[2] | p_addr[3] | p_addr[4] | p_addr[5] | p_addr[6]) == 0)
    {
        pf_log_raw(atel_log_ctl.error_en, "aus_set_adv_filter Mac Err.\r");
        return;
    }

    if ((p_addr[0] != monet_data.ble_filter_mac_addr[0][0]) ||
        (p_addr[1] != monet_data.ble_filter_mac_addr[0][1]) ||
        (p_addr[2] != monet_data.ble_filter_mac_addr[0][2]) ||
        (p_addr[3] != monet_data.ble_filter_mac_addr[0][3]) ||
        (p_addr[4] != monet_data.ble_filter_mac_addr[0][4]) ||
        (p_addr[5] != monet_data.ble_filter_mac_addr[0][5]))
    {
        monet_data.ble_filter_mac_addr[0][0] = p_addr[0];
        monet_data.ble_filter_mac_addr[0][1] = p_addr[1];
        monet_data.ble_filter_mac_addr[0][2] = p_addr[2];
        monet_data.ble_filter_mac_addr[0][3] = p_addr[3];
        monet_data.ble_filter_mac_addr[0][4] = p_addr[4];
        monet_data.ble_filter_mac_addr[0][5] = p_addr[5];

        memcpy(monet_data.ble_info[0].mac_addr, p_addr, BLE_MAC_ADDRESS_LEN);
        // monet_data.ble_info[0].connect_status = BLE_CONNECTION_STATUS_MAC_SET;
        // monet_data.ble_info[0].handler = 0xffff;

        filter_set = 0;
        pf_log_raw(atel_log_ctl.error_en, "aus_set_adv_filter Mac(%x:%x:%x:%x:%x:%x).\r",
                   p_addr[0], p_addr[1], p_addr[2], p_addr[3], p_addr[4], p_addr[5]);
    }

    if (filter_set)
    {
        return;
    }

    err_code = ble_aus_white_list_set();

    if (err_code == 0)
    {
        filter_set = 1;
    }
}

void ble_aus_advertising_stop(void)
{
    uint32_t err_code = 0;
    uint16_t i = 0, handler = 0xffff;

    for (i = 0; i < BLE_CHANNEL_NUM_MAX; i++)
    {
        handler = ble_connected_handler_get_from_channel(i);
        if (handler != 0xffff)
        {
            err_code = sd_ble_gap_disconnect(handler, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            pf_log_raw(atel_log_ctl.core_en, "ble_aus_advertising_stop CH(%d) Handler(%d) Err(%d).\r", i, handler, err_code);
        }
    }

    // TODO: optimize advertising stop
    // if (m_advertising.adv_mode_current != BLE_ADV_MODE_IDLE)
    // {
    //     sd_ble_gap_adv_stop(m_advertising.adv_handle);
    // }

    pf_log_raw(atel_log_ctl.core_en, "ble_aus_advertising_stop called\r");
    ble_connected_channel_clear();

    // TODO: disconnect all
    monet_data.bleDisconnectEvent = 1;
}

uint32_t ble_aus_advertising_start(void)
{
    uint32_t err_code = 0;
    if (monet_data.is_advertising == 1)
    {
        pf_log_raw(atel_log_ctl.core_en, "ble_aus_advertising_start: already.\r");
        return err_code;
    }

    err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    pf_log_raw(atel_log_ctl.core_en, "ble_aus_advertising_start: %d.\r", err_code);
    CRITICAL_REGION_ENTER();
    monet_data.to_advertising_result = err_code;
    if (monet_data.to_advertising_result == NRF_SUCCESS)
    {
        monet_data.is_advertising = 1;
    }

    if (err_code)
    {
        if (err_code == NRF_ERROR_INVALID_PARAM)
        {
            monet_data.to_advertising_without_white_list = 1;
        }

        monet_data.to_advertising_recover = 1;
    }
    else
    {
        monet_data.to_advertising_recover = 0;
    }
    CRITICAL_REGION_EXIT();
    // APP_ERROR_CHECK(err_code);
    return err_code;
}

uint32_t ble_aus_change_change_conn_params(uint16_t channel, ble_aus_conn_param_t ble_conn_param)
{
    ret_code_t err_code = 0;
    ble_gap_conn_params_t new_params;

    if (monet_data.ble_info[channel].handler == 0xffff)
    {
        pf_log_raw(atel_log_ctl.error_en, "change_conn_params channel err.\r");
        return 0xfe;
    }

    err_code = ble_conn_params_stop();
    pf_log_raw(atel_log_ctl.core_en, "ble_conn_params_stop Err(0x%x:%d).\r", err_code, err_code);
    
    new_params.min_conn_interval = MSEC_TO_UNITS(ble_conn_param.min_ms, UNIT_1_25_MS);
    new_params.max_conn_interval = MSEC_TO_UNITS(ble_conn_param.max_ms, UNIT_1_25_MS);
    new_params.slave_latency = ble_conn_param.latency;
    new_params.conn_sup_timeout = MSEC_TO_UNITS(ble_conn_param.timeout_ms, UNIT_10_MS);

    err_code = ble_conn_params_change_conn_params(monet_data.ble_info[channel].handler, &new_params);

    pf_log_raw(atel_log_ctl.core_en, "change_conn_params Err(0x%x:%d).\r", err_code, err_code);

    return err_code;
}

uint32_t ble_aus_change_change_conn_params_disconnect(uint16_t channel)
{
    ret_code_t err_code = 0;

    if (channel != 0xffff)
    {
        err_code = sd_ble_gap_disconnect(monet_data.ble_info[channel].handler, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);

        monet_data.ble_info[channel].connect_status = BLE_CONNECTION_STATUS_NOT_CONNECTED;
        
        ble_aus_advertising_start();
    }

    pf_log_raw(atel_log_ctl.core_en, "conn_params_disconnect CH(%d) Err(%d).\r", channel, err_code);

    return err_code;
}
#endif /* BLE_FUNCTION_ONOFF */

static void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt)
{
    pf_log_raw(atel_log_ctl.core_en, "Zazu Version: %d.%d.%d.%d.\r\n", MNT_MAJOR, MNT_MINOR, MNT_REVISION, MNT_BUILD);
    if (p_evt->result != NRF_SUCCESS)
    {
        pf_log_raw(atel_log_ctl.core_en, "--> Event received: ERROR while executing an fstorage operation.\r");
        return;
    }

    switch (p_evt->id)
    {
        case NRF_FSTORAGE_EVT_WRITE_RESULT:
        {
            pf_log_raw(atel_log_ctl.core_en, "--> Event received: wrote %d bytes at address 0x%x.\r",
                         p_evt->len, p_evt->addr);
        } break;

        case NRF_FSTORAGE_EVT_ERASE_RESULT:
        {
            pf_log_raw(atel_log_ctl.core_en, "--> Event received: erased %d page from address 0x%x.\r",
                         p_evt->len, p_evt->addr);
        } break;

        default:
            break;
    }
}

#define FDS_AREA_START_ADDRESS (0x75000)
#define FDS_AREA_END_ADDRESS (0x77FFF)

NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) =
{
    /* Set a handler for fstorage events. */
    .evt_handler = fstorage_evt_handler,

    /* These below are the boundaries of the flash space assigned to this instance of fstorage.
     * You must set these manually, even at runtime, before nrf_fstorage_init() is called.
     * The function nrf5_flash_end_addr_get() can be used to retrieve the last address on the
     * last page of flash available to write data. */
    .start_addr = FDS_AREA_START_ADDRESS,
    .end_addr   = FDS_AREA_END_ADDRESS,
};

static void wait_for_flash_ready(nrf_fstorage_t const * p_fstorage)
{
    uint16_t i = 0;
    /* While fstorage is busy, sleep and wait for an event. */
    while (nrf_fstorage_is_busy(p_fstorage))
    {
        if (i < 100)
        {
            i++;
            pf_delay_ms(1);
        }
        else
        {
            break;
        }
    }
}

static void erase_fds_areas(void)
{
    ret_code_t rc;
    nrf_fstorage_api_t * p_fs_api;

    p_fs_api = &nrf_fstorage_sd;

    rc = nrf_fstorage_init(&fstorage, p_fs_api, NULL);
    APP_ERROR_CHECK(rc);

    nrf_fstorage_erase(&fstorage, FDS_AREA_START_ADDRESS, 3, NULL);

    wait_for_flash_ready(&fstorage);

    nrf_fstorage_erase(&fstorage, FDS_AREA_START_ADDRESS, 3, NULL);

    wait_for_flash_ready(&fstorage);

    pf_delay_ms(100);

    nrf_fstorage_uninit(&fstorage, NULL);

    if (reset_info.reset_from_dfu == 1)
    {
        DFU_FLAG_REGISTER = DFU_FLAG_VALUE_FROM_BOOT;
        DFU_FLAG_REGISTER1 = DFU_FLAG_VALUE_FROM_INVALID;
        //DFU_FLAG_REGISTER1 |= DEVICE_RESET_SOURCE_BIT_MASK(DEVICE_RESET_FROM_INVALID);
    }

    nrf_pwr_mgmt_shutdown(NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU);
}

/**
 * @}
 */
