#include <math.h>
#include "drv_spi.h"
#include "drv_canfdspi_register.h"
#include "user.h"
#include "serial_frame.h"
#include "ring_pool_queue.h"

// Transmit Channels
#define APP_TX_FIFO CAN_FIFO_CH2

// Receive Channels
#define APP_RX_FIFO CAN_FIFO_CH1

bool ramInitialized;

CAN_CONFIG config;
CAN_OPERATION_MODE opMode;

// Transmit objects
CAN_TX_FIFO_CONFIG txConfig;
CAN_TX_FIFO_EVENT txFlags;
CAN_TX_MSGOBJ txObj;
uint8_t txd[MAX_DATA_BYTES];

// Receive objects
CAN_RX_FIFO_CONFIG rxConfig;
REG_CiFLTOBJ fObj;
REG_CiMASK mObj;
CAN_RX_FIFO_EVENT rxFlags;
CAN_RX_MSGOBJ rxObj;
uint8_t rxd[MAX_DATA_BYTES];

CAN_BITTIME_SETUP selectedBitTime = CAN_500K_1M;

uint8_t tec;
uint8_t rec;
CAN_ERROR_STATE errorFlags;

#if CAN_DATA_RATE_CHECK_EN
uint32_t can_rx_data_count = 0;
uint32_t can_tx_data_count = 0;
#endif /* CAN_DATA_RATE_CHECK_EN */

#define MAX_TXQUEUE_ATTEMPTS 50

void APP_CANFDSPI_Init(void)
{
    // Reset device
    DRV_CANFDSPI_Reset(DRV_CANFDSPI_INDEX_0);

    // Enable ECC and initialize RAM
    DRV_CANFDSPI_EccEnable(DRV_CANFDSPI_INDEX_0);

    if (!ramInitialized) {
        DRV_CANFDSPI_RamInit(DRV_CANFDSPI_INDEX_0, 0xff);
        ramInitialized = true;
    }

    // Configure device
    DRV_CANFDSPI_ConfigureObjectReset(&config);
    config.IsoCrcEnable = 1;
    config.StoreInTEF = 0;

    DRV_CANFDSPI_Configure(DRV_CANFDSPI_INDEX_0, &config);

    // Setup TX FIFO
    DRV_CANFDSPI_TransmitChannelConfigureObjectReset(&txConfig);
    txConfig.FifoSize = 18;
    txConfig.PayLoadSize = CAN_PLSIZE_32;
    txConfig.TxPriority = 1;

    DRV_CANFDSPI_TransmitChannelConfigure(DRV_CANFDSPI_INDEX_0, APP_TX_FIFO, &txConfig);

    // Setup RX FIFO
    DRV_CANFDSPI_ReceiveChannelConfigureObjectReset(&rxConfig);
    rxConfig.FifoSize = 31;
    rxConfig.PayLoadSize = CAN_PLSIZE_32;

    DRV_CANFDSPI_ReceiveChannelConfigure(DRV_CANFDSPI_INDEX_0, APP_RX_FIFO, &rxConfig);

    // Setup RX Filter
    fObj.word = 0;
    #if (CAN_ROLE_SELECT == CAN_ROLE_SERVER)
    fObj.bF.SID = CAN_SERVER_ID0;
    #else
    fObj.bF.SID = CAN_CLIENT_ID0;
    #endif /* CAN_ROLE_SELECT */
    fObj.bF.EXIDE = 0;
    fObj.bF.EID = 0x00;

    DRV_CANFDSPI_FilterObjectConfigure(DRV_CANFDSPI_INDEX_0, CAN_FILTER0, &fObj.bF);

    // Setup RX Mask
    mObj.word = 0;
    #if (CAN_ROLE_SELECT == CAN_ROLE_SERVER)
    mObj.bF.MSID = 0x0;
    #else
    mObj.bF.MSID = 0x7FF;
    #endif /* CAN_ROLE_SELECT */
    mObj.bF.MIDE = 1; // Only allow standard IDs
    mObj.bF.MEID = 0x0;
    DRV_CANFDSPI_FilterMaskConfigure(DRV_CANFDSPI_INDEX_0, CAN_FILTER0, &mObj.bF);

    // Link FIFO and Filter
    DRV_CANFDSPI_FilterToFifoLink(DRV_CANFDSPI_INDEX_0, CAN_FILTER0, APP_RX_FIFO, true);

    // Setup Bit Time
    DRV_CANFDSPI_BitTimeConfigure(DRV_CANFDSPI_INDEX_0, selectedBitTime, CAN_SSP_MODE_AUTO, CAN_SYSCLK_20M);

    // Setup Transmit and Receive Interrupts
    DRV_CANFDSPI_GpioModeConfigure(DRV_CANFDSPI_INDEX_0, CAN_GPIO_MODE_GPIO, CAN_GPIO_MODE_GPIO);
    DRV_CANFDSPI_TransmitChannelEventEnable(DRV_CANFDSPI_INDEX_0, APP_TX_FIFO, CAN_TX_FIFO_NOT_FULL_EVENT);
    DRV_CANFDSPI_ReceiveChannelEventEnable(DRV_CANFDSPI_INDEX_0, APP_RX_FIFO, CAN_RX_FIFO_NOT_EMPTY_EVENT);
    DRV_CANFDSPI_ModuleEventEnable(DRV_CANFDSPI_INDEX_0, CAN_TX_EVENT | CAN_RX_EVENT);

    // Select Normal Mode
    DRV_CANFDSPI_OperationModeSelect(DRV_CANFDSPI_INDEX_0, CAN_NORMAL_MODE);
}

CAN_DLC APP_CANFDSPI_DataBytesToDlc(uint8_t n)
{
    CAN_DLC dlc = CAN_DLC_0;

    if (n <= 8) {
        dlc = (CAN_DLC)n;
    } else if (n == 12) {
        dlc = CAN_DLC_12;
    } else if (n == 16) {
        dlc = CAN_DLC_16;
    } else if (n == 20) {
        dlc = CAN_DLC_20;
    } else if (n == 24) {
        dlc = CAN_DLC_24;
    } else if (n == 32) {
        dlc = CAN_DLC_32;
    } else if (n == 48) {
        dlc = CAN_DLC_48;
    } else if (n == 64) {
        dlc = CAN_DLC_64;
    }

    return dlc;
}

uint8_t APP_CANFDSPI_StepDataBytes(uint8_t n)
{
    uint8_t step = 0;

    if (n < 8) {
        step = n;
    } else if (n < 12) {
        step = 8;
    } else if (n < 16) {
        step = 12;
    } else if (n < 20) {
        step = 16;
    } else if (n < 24) {
        step = 20;
    } else if (n < 32) {
        step = 24;
    } else if (n < 48) {
        step = 32;
    } else if (n < 64) {
        step = 48;
    } else {
        step = 64;
    }

    return step;
}

void APP_CANFDSPI_TransmitMessageQueue(void)
{
    uint8_t attempts = MAX_TXQUEUE_ATTEMPTS;

    // Check if FIFO is not full
    do {
        DRV_CANFDSPI_TransmitChannelEventGet(DRV_CANFDSPI_INDEX_0, APP_TX_FIFO, &txFlags);

        if (attempts == 0) {
            __NOP();
            __NOP();
            DRV_CANFDSPI_ErrorCountStateGet(DRV_CANFDSPI_INDEX_0, &tec, &rec, &errorFlags);
            return;
        }
        attempts--;
    }
    while (!(txFlags & CAN_TX_FIFO_NOT_FULL_EVENT));

    // Load message and transmit
    uint8_t n = DRV_CANFDSPI_DlcToDataBytes(txObj.bF.ctrl.DLC);

    #if CAN_DATA_RATE_CHECK_EN
    can_tx_data_count += n;
    #endif /* CAN_DATA_RATE_CHECK_EN */

    DRV_CANFDSPI_TransmitChannelLoad(DRV_CANFDSPI_INDEX_0, APP_TX_FIFO, &txObj, txd, n, true);
}

int8_t APP_CANFDSPI_TransmitMessage(uint32_t id, uint8_t *p_data, uint16_t len)
{
    if ((len > MAX_DATA_BYTES) || (len == 0))
    {
        return -1;
    }

    uint8_t step = 0;
    uint16_t left = len;
    uint8_t* p_tmp = p_data;

SEND_AGAIN:

    step = APP_CANFDSPI_StepDataBytes(left);

    if (step > CAN_TX_SETP_BYTES_MAX)
    {
        step = CAN_TX_SETP_BYTES_MAX;
    }

    __NOP();
    __NOP();

    // Configure transmit message
    txObj.word[0] = 0;
    txObj.word[1] = 0;

    txObj.bF.id.SID = id;
    txObj.bF.id.EID = 0;

    txObj.bF.ctrl.DLC = APP_CANFDSPI_DataBytesToDlc(step);
    txObj.bF.ctrl.IDE = 0;
    txObj.bF.ctrl.BRS = 0;
    txObj.bF.ctrl.FDF = 1;

    memcpy(txd, p_tmp, step);

    APP_CANFDSPI_TransmitMessageQueue();

    p_tmp += step;
    left -= step;

    if (left)
    {
        goto SEND_AGAIN;
    }

    return 0;
}

void APP_CANFDSPI_ReceiveMessage(uint32_t *p_id, uint8_t *p_buf, uint8_t *p_len)
{
    __NOP();
    __NOP();

    *p_len = 0;

    // Check if FIFO is not empty
    DRV_CANFDSPI_ReceiveChannelEventGet(DRV_CANFDSPI_INDEX_0, APP_RX_FIFO, &rxFlags);
    if (rxFlags & CAN_RX_FIFO_NOT_EMPTY_EVENT) {

        // Get message
        DRV_CANFDSPI_ReceiveMessageGet(DRV_CANFDSPI_INDEX_0, APP_RX_FIFO, &rxObj, rxd, MAX_DATA_BYTES);

        *p_id = rxObj.bF.id.SID;
        *p_len = DRV_CANFDSPI_DlcToDataBytes((CAN_DLC)rxObj.bF.ctrl.DLC);
        memcpy(p_buf, rxd, *p_len);
    }
}

typedef struct {
    uint8_t buf[MAX_DATA_BYTES];
    uint32_t len;
} can_data_buf_t;

uint8_t can0_tx_rbuf[CAN_TX_RBUF_BYTES] = {0};
can_data_buf_t can0_rxdata_buf = {0};
sf_control_block_t can0_sf_cb = {0};

ring_pool_queue_t can_pool_queue = {0};

int32_t can0_rx_data(uint8_t* p_data, uint32_t* p_len)
{
    memcpy(p_data, can0_rxdata_buf.buf, can0_rxdata_buf.len);
    *p_len = can0_rxdata_buf.len;
    memset(&can0_rxdata_buf, 0, sizeof(can_data_buf_t));

    return 0;
}

int32_t can0_rx_process(uint8_t cmd, uint8_t* p_data, uint32_t len)
{
    int8_t ret = 0;
    ring_pool_cell_t cell = {0};

    // pf_log_raw(atel_log_ctl.normal_en, "can0_rx_process cmd: 0x%x len: %d.\r\n", cmd, len);
    // printf_hex_and_char(p_data, len);

    cell.length = len;
    cell.p_data = p_data;
    cell.channel = 0;

    ret = ring_pool_queue_in(&can_pool_queue, &cell);

    if (ret)
    {
        pf_log_raw(atel_log_ctl.error_en, "can0_rx_process err(%d) pool(M %d: N %d).\r\n", ret, can_pool_queue.max, can_pool_queue.num);
    }

    return ret;
}

int32_t can0_tx_handle(uint8_t* p_data, uint32_t len)
{
    #if (CAN_ROLE_SELECT == CAN_ROLE_SERVER)
    APP_CANFDSPI_TransmitMessage(CAN_CLIENT_ID0, p_data, len);
    #else
    APP_CANFDSPI_TransmitMessage(CAN_SERVER_ID0, p_data, len);
    #endif /* CAN_ROLE_SELECT */
    // logger_raw("can0_tx_handle:\r\n");
    // printf_hex_and_char(p_data, len);
    return 0;
}

void can_controller_init(void)
{
    sf_control_block_init_t can_sf_cb_init = {0};

    RING_POOL_QUEUE_INIT(can_pool_queue, CAN_RX_POOL_QUEUE_NUM, CAN_RX_POOL_QUEUE_CELL_SIZE);

    APP_CANFDSPI_Init();

    can_sf_cb_init.p_tx_buf = can0_tx_rbuf;
    can_sf_cb_init.tx_buf_size = sizeof(can0_tx_rbuf);
    can_sf_cb_init.rx_data = can0_rx_data;
    can_sf_cb_init.rx_process = can0_rx_process;
    can_sf_cb_init.tx_handle = can0_tx_handle;
    sf_data_init(&can0_sf_cb, &can_sf_cb_init);
}

void can_controller_rx(uint32_t sys_time_ms)
{
    uint8_t data_r[MAX_DATA_BYTES] = {0};
    uint32_t id_r = 0;
    uint8_t len_r = 0;
    uint16_t c_r = 0;

RECEIVE_AGAIN:
    APP_CANFDSPI_ReceiveMessage(&id_r, data_r, &len_r);

    if (len_r)
    {
        #if CAN_DATA_RATE_CHECK_EN
        can_rx_data_count += len_r;
        #endif /* CAN_DATA_RATE_CHECK_EN */

        // TODO: Camera power off refresh, rx data power on camera
        monet_data.RecvDataEvent = 1;
        camera_poweroff_delay_refresh();

        // logger_raw("can_controller_rx ID: 0x%x len: %d.\r\n", id_r, len_r);
        // printf_hex_and_char(data_r, len_r);

        #if (CAN_ROLE_SELECT == CAN_ROLE_SERVER)
        if (id_r == CAN_SERVER_ID0)
        #else
        if (id_r == CAN_CLIENT_ID0)
        #endif /* CAN_ROLE_SELECT */
        {
            can0_rxdata_buf.len = len_r;
            memcpy(can0_rxdata_buf.buf, data_r, len_r);
            sf_data_process(&can0_sf_cb);
        }

        c_r++;

        if (c_r < 31)
        {
            goto RECEIVE_AGAIN;
        }
    }
}

void can_controller_tx(uint8_t* p_data, uint8_t len, uint16_t channel)
{
    int32_t ret = 0;

    if (len > CAN_RX_POOL_QUEUE_CELL_SIZE)
    {
        pf_log_raw(atel_log_ctl.error_en, "can_controller_tx channel(%d) len(%d) err.\r\n", channel, len);
        return;
    }

    // TODO: Camera power off refresh
    camera_poweroff_delay_refresh();

    if (channel == 0)
    {
        #if (CAN_ROLE_SELECT == CAN_ROLE_SERVER)
        ret = sf_data_send(&can0_sf_cb, 'S', p_data, len);
        #else
        ret = sf_data_send(&can0_sf_cb, 'C', p_data, len);
        #endif /* CAN_ROLE_SELECT */

        if (ret)
        {
            pf_log_raw(atel_log_ctl.error_en, "can_controller_tx channel(%d) err(%d).\r\n", channel, ret);
        }
    }
    else
    {
        pf_log_raw(atel_log_ctl.error_en, "can_controller_tx channel(%d) invalid.\r\n", channel);
    }
}

void can_controller_process(uint8_t rx_ready, uint32_t sys_time_ms)
{
    #if CAN_DATA_RATE_CHECK_EN
    static uint32_t last_sys_time = 0;
    #endif /* CAN_DATA_RATE_CHECK_EN */

    if (rx_ready)
    {
        can_controller_rx(sys_time_ms);
    }

    sf_data_process(&can0_sf_cb);

    #if CAN_DATA_RATE_CHECK_EN
    if ((sys_time_ms - last_sys_time) > 5000)
    {
        float data_rate = (1000.0 * can_rx_data_count) / (sys_time_ms - last_sys_time);
        float data_rate1 = (1000.0 * can_tx_data_count) / (sys_time_ms - last_sys_time);
        last_sys_time = sys_time_ms;
        can_rx_data_count = 0;
        can_tx_data_count = 0;
        pf_log_raw(atel_log_ctl.platform_en, "can_controller (R: %d, S:%d, B:%d)", rx_ready, CAN_ROLE_SELECT, selectedBitTime);
        pf_log_raw(atel_log_ctl.platform_en, " RX: "NRF_LOG_FLOAT_MARKER"B/S", NRF_LOG_FLOAT(data_rate));
        pf_log_raw(atel_log_ctl.platform_en, " TX: "NRF_LOG_FLOAT_MARKER"B/S.\r", NRF_LOG_FLOAT(data_rate1));
    }
    #endif /* CAN_DATA_RATE_CHECK_EN */

    #if (SPI_CAN_CONTROLLER_EN && (CAN_ROLE_SELECT == CAN_ROLE_CLIENT) && (CAN_CONTROLLER_TEST_EN))
    can_controller_test();
    #endif /* CAN_ROLE_SELECT && CAN_CONTROLLER_TEST_EN */
}

void can_rx_pool_queue_process(void)
{
    ring_pool_cell_t cell = {0};

    if (ring_pool_queue_is_empty(&can_pool_queue))
    {
        return;
    }

    monet_data.data_recv_channel = 0xffff;
    ring_pool_queue_out(&can_pool_queue, &cell);
    monet_data.data_recv_channel = cell.channel;

    if ((cell.p_data[0] == 0x66) && (cell.p_data[1] == 0x00))
    {
        pf_gpio_pattern_set(cell.p_data + 2, cell.length - 2);
        pf_log_raw(atel_log_ctl.error_en, "CAN(%d) GPIO Control(%d:%d).", cell.channel, cell.p_data[2], cell.p_data[3]);
    }
    else if (cell.p_data[0] == 0xAA)
    {
        // BuildFrame(cell.p_data[0], cell.p_data + 1, cell.length - 1);
        pf_log_raw(atel_log_ctl.core_en, "atel_io_queue_process 0xAA CAN\r");
        monet_xAAcommand(cell.p_data + 1, cell.length - 1);
    }
    else
    {
        if (cell.length)
        {
            // WARNING: Convert from IO_CMD_CHAR_CAN_RECV to BLE cmd
            BuildFrame(IO_CMD_CHAR_CAN_RECV - 3, cell.p_data, cell.length);
        }
    }

    // logger_raw("can_rx_pool_queue_process out: %d:%d.\r\n", cell.channel, cell.length);
    // printf_hex_and_char(cell.p_data, cell.length);

    ring_pool_queue_delete_one(&can_pool_queue);
}

uint8_t can_rx_pool_queue_is_full(void)
{
    return ring_pool_queue_is_full(&can_pool_queue);
}

void can_controller_test(void)
{
    #if 1
    static uint32_t count = 0;
    uint8_t buf[128] = {'1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
                        '1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
                        '1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
                        '1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
                        '1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
                        '1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
                        '1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
                        '1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4','1', '2', '3', '4',
    };

    count++;

    if (count > 180)
    {
        count = 0;
        can_controller_tx(buf, 128, 0);
    }
    #else
    uint8_t buf[128] = {0};
    uint32_t i = 0;
    static uint32_t count = 0;

    for (i = 0; i < sizeof(buf); i++)
    {
        buf[i] = i;
    }

    count++;
    buf[0] = (count >> 24) & 0xff;
    buf[1] = (count >> 16) & 0xff;
    buf[2] = (count >> 8) & 0xff;
    buf[3] = (count >> 0) & 0xff;

    sf_data_send(&can0_sf_cb, 0x01, buf, 128);
    #endif
}

#if 0
void APP_CANFDSPI_Test(void)
{
    uint8_t data_r[MAX_DATA_BYTES] = {0};
    uint32_t id_r = 0;
    uint8_t len_r = 0;
    uint16_t c_r = 0;
    static uint32_t total_r = 0;
    static uint32_t old_total_r = 0;
    uint8_t data_t[MAX_DATA_BYTES] = "12345678";
    static uint32_t count = 0;
    count++;
    memcpy(data_t, &count, sizeof(count));
    APP_CANFDSPI_TransmitMessage(CAN_CLIENT_ID0, data_t, 8);

RECEIVE_AGAIN:
    APP_CANFDSPI_ReceiveMessage(&id_r, data_r, &len_r);
    if (len_r)
    {
        c_r++;
        total_r++;
        // printf_hex_and_char(data_r, len_r);

        if (c_r < 31)
        {
            goto RECEIVE_AGAIN;
        }
    }

    if (old_total_r != total_r)
    {
        old_total_r = total_r;
        pf_log_raw(atel_log_ctl.platform_en, "APP_CANFDSPI_Test ID(0x%x) L(%d) C(%d:%d) T(%d).\r\n",
               id_r, len_r, c_r, *((uint32_t *)data_r), total_r);
    }
}
#endif /* 0 */
