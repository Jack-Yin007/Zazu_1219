/*
 * user.h
 *
 *  Created on: Dec 12, 2015
 *      Author: J
 */

#ifndef USER_H_
#define USER_H_

/* get compiler defined type definitions (NULL, size_t, etc) */
#include <stddef.h>

#include "version.h"
#include "platform_hal_drv.h"
#include "ring_buf.h"

/* TODO Application specific user parameters used in user.c may go here */

/******************************************************************************/
/* User Function Prototypes                                                   */
/******************************************************************************/
#define TIME_UNIT                       (1000)         //ms
#define TIME_UINT_SECOND_TIMER_PERIOD   (5000)        //ms
#define TIME_UNIT_CHANGE_WHEN_SLEEP     (0)
#define TIME_UNIT_IN_SLEEP_NORMAL       (TIME_UNIT)   //ms
#define TIME_UNIT_IN_SLEEP_HIBERNATION  (30000)       //ms

#define DEVICE_POWER_OFF_DELAY_MS (2 * 1000)
#define DEVICE_POWER_ON_DELAY_MS (45 * 1000)
#define DEVICE_FIRST_POWER_ON_DELAY_MS (5 * 1000)
#define DEVICE_POWER_ON_DELAY_SHORT_MS (8 * 1000)
#define DEVICE_BOOT_DELAY_MS (25 * 1000)
#define DEVICE_POWER_ON_PENDING_MS (2 * 1000)
#define DEVICE_BOOT_ENTER_DELAY_MS (2 * 1000)

#define MILISECOND_TO_SECONDS(x) ((x) / (1000))

#define MODULE_SN_LENGTH 8

#define IO_CMD_CHAR_BLE_RECV ('1')
#define IO_CMD_CHAR_BLE_SEND ('2')
#define IO_CMD_CHAR_BLE_CTRL ('3')

#define CMD_CHECKSUM                    (1)
#define CMD_ESCAPE                      (1)
#define USE_CHECKSUM                    (1)
#define MAX_COMMAND                     (244)

#define INT_WAKEUP                      23 // 0x00800000
#define INT_POWERUP                     22 // 0x00400000 Added power up indicator
#define INT_WDFIRED                     21 // 0x00200000 Added WD Fired indicator
#define INT_APP_WAKEUP                  20 // 0x00100000 App Wake MCU
#define INT_VIRTUAL_IGNITION            19 // 0x00080000
#define INT_TILT_TAMPER                 18 // 0x00040000
#define INT_POWER_HIGH                  17 // 0x00020000
#define INT_POWER_LOW                   16 // 0x00010000
#define INT_POWER_RESTORED              15
#define INT_MAIN_ADC_OUT_OF_RANGE       14
#define INT_BACKUP_ADC_OUT_OF_RANGE     13
#define INT_ACCELEROMETER_TRIGGER       12 // 0x00001000
#define INT_BAT_ON                      11 // 0x00000800 the device is operating on battery (no main power)
#define INT_BAT_OFF                     10 // 0x00000400 the unit is no longer using battery (is on main power)
#define INT_ACC_SK                      9
#define INT_TIMER                       8  // 0x00000100
#define INT_GPIO_D                      3  // 0x00000008
#define INT_GPIO_C                      2  // 0x00000004
#define INT_GPIO_B                      1  // 0x00000002
#define INT_GPIO_A                      0  // 0x00000001

#define MASK_FOR_BIT(n) (1UL << (n))    // Use  1UL to avoid warning:  #61-D: integer operation result is out of range (for bit 31)

#define GPIO_DIRECTION      0x80 //0:output, 1:input
#define GPIO_MODE           0x40 //input: 0: float, 1: pull-up, for output: 0: open 1:push-pull
#define GPIO_OUTPUT_HIGH    0x20 //output: 1: H active 0: Low Active
#define GPIO_SET_HIGH       0x10 //GPIO: 0: low, 1: high default value

#define DIRECTION_IN        GPIO_DIRECTION
#define DIRECTION_OUT       0

#define GPIO_INTO_MASK      0x8
#define GPIO_WD_MASK        0x4

#define GPIO_IT_MODE        0x3 //input interrupt mode mask
#define GPIO_IT_NONE        0x0
#define GPIO_IT_LOW_TO_HIGH 0x1
#define GPIO_IT_HIGH_TO_LOW 0x2
#define GPIO_IT_BOTH        0x3

#define PIN_STATUS(x,y,z,s) ((x>0?(GPIO_DIRECTION|GPIO_IT_BOTH):0)| (y>0?GPIO_MODE:0)| (z>0? GPIO_OUTPUT_HIGH:0)|(s>0?GPIO_SET_HIGH:0))

#ifdef ATEL_GPIO_ASSOC
#undef ATEL_GPIO_ASSOC
#endif /* ATEL_GPIO_ASSOC */
#define ATEL_GPIO_ASSOC(enum,x,y,z,s)    enum,

// Do not rearrarange order without synchronizing with App
typedef enum {
    GPIO_FIRST = -1,

    ATEL_ALL_GPIOS

    GPIO_LAST
} mntGpio_t;

#define MAX_EXT_GPIOS (0)

#define IS_VALID_GPIO(p)        ((p) > GPIO_FIRST && (p) < GPIO_LAST)
#define GPIO_TO_INDEX(p)        ((p) - GPIO_FIRST - 1)
#define NUM_OF_GPIO_PINS        (GPIO_LAST)
#define NUM_OF_GPIO             NUM_OF_GPIO_PINS
#define GPIO_NONE               GPIO_LAST

typedef enum {
    IO_WAIT_FOR_DOLLAR,
    IO_GET_FRAME_LENGTH,
    IO_GET_COMMAND,
    IO_GET_CARRIAGE_RETURN
} IoCmdState;

typedef enum {
    IO_GET_NORMAL,
    IO_GET_ESCAPE
} IoEspState;

typedef enum {
    MAIN_UNKNOWN,
    MAIN_NORMAL,
    MAIN_GONE
 } MainState_e;

typedef enum {
    SLEEP_OFF,
    SLEEP_NORMAL,
    SLEEP_HIBERNATE
} SleepState_e;

typedef enum {
    MONET_BB_OFF,
    MONET_BB_ISAWAKE,
    MONET_BB_ON,
    MONET_BB_TOGGLEPWRKEY,
    MONET_BB_START,
    MONET_BB_DONE,
    MONET_POWER_SWMAIN     = 16, // Power State mask (bit 4)
    MONET_POWER_SWBATT     = 32, // Power State mask (bit 5)
    MONET_POWER_LOW_THRES  = 64, // Power State mask (bit 6)
    MONET_POWER_HIGH_THRES = 128 // Power State mask (bit 7)
} Monet_BB_StartUp;

typedef enum {
    TX_LEVEL_P8,
    TX_LEVEL_P4,
    TX_LEVEL_0,
    TX_LEVEL_N4,
    TX_LEVEL_N8,
    TX_LEVEL_N12,
    TX_LEVEL_N16,
    TX_LEVEL_N20,
    TX_LEVEL_LAST
} TX_Power_Level;

typedef struct {
    uint8_t  txRssi;
}tx_power_struct;

typedef struct {
    IoCmdState      state;
    IoEspState      escape;
    uint8_t         cmd;
    uint8_t         length;
    uint8_t         remaining;
    uint8_t         checksum;
    uint8_t         data[MAX_COMMAND];
}IoRxFrameStruct;

typedef struct {
    uint8_t         Pin;
    uint32_t        Reload;  //in second
} wd_struct;

typedef struct {
    uint8_t         status;
    uint16_t        Reload;
} gpio_struct;

typedef struct{
    uint8_t         IntPin;
    wd_struct       WD;
    gpio_struct     gConf[NUM_OF_GPIO];
} gpio_conf;

typedef struct{
    uint32_t        Intstatus;
    uint32_t        WDtimer;
    uint16_t        counter[NUM_OF_GPIO];
    uint32_t        gpiolevel;
    uint8_t         WDflag;
} gpio_data;

#define RING_BUFFER_SIZE_FROM_MCU (1024 * 2)

#define BLE_CHANNEL_NUM_MAX (8)
#define BLE_MAC_ADDRESS_LEN (6)

typedef struct
{
    uint16_t min_100us;
    uint16_t max_100us;
    uint16_t latency;
    uint32_t timeout_100us;
} BLE_CONN_PARAM_t;

typedef struct {
    IoRxFrameStruct     iorxframe;
    ring_buffer_t       txQueueU1;

    uint32_t            SleepAlarm;
    uint32_t            sysTickUnit;
    uint8_t             SleepStateChange;
    SleepState_e        SleepState;

    uint32_t            deviceofftime;
    uint8_t             devicePowerOn;
    uint8_t             devicePowerOnPending;
    uint32_t            devicePowerOnTimeMs;
    uint32_t            devicePowerOffTimeMs;
    uint32_t            devicePowerOffDelay;
    uint32_t            devicePowerOnDelay;
    uint32_t            deviceBootEnterDelay;
    uint16_t            deviceWakeupDetect;
    uint16_t            deviceBootDelayMs;
    uint8_t             firstPowerUp;
    uint8_t             uartTXDEnabled;
    uint8_t             uartToBeDeinit;
    uint8_t             uartToBeInit;
    uint8_t             uartTickCount;
    uint8_t             appActive;

    uint8_t             bleConnectionStatus;
    uint32_t            bleDisconnectTimeMs;
    uint8_t             bleConnectionEvent;
    uint8_t             bleDisconnectEvent;
    uint16_t            ble_recv_channel;
    uint8_t             ble_mac_addr[BLE_MAC_ADDRESS_LEN];
    uint8_t             ble_peer_mac_addr[BLE_MAC_ADDRESS_LEN];
    uint8_t             ble_scan_mode;
    uint32_t            ble_scan_reset_ms;
    BLE_CONN_PARAM_t    ble_conn_param;
    uint8_t             bleRecvDataEvent;
    uint8_t             bleAdvertiseStatus;
} monet_struct;

typedef struct
{   // 1: enable, 0: disable
    uint8_t normal_en;
    uint8_t core_en;
    uint8_t io_protocol_en;
    uint8_t irq_handler_en;
    uint8_t platform_en;
    uint8_t warning_en;
    uint8_t error_en;
} atel_log_ctl_t;

extern monet_struct monet_data;
extern gpio_conf    monet_conf;
extern gpio_data    monet_gpio;
extern atel_log_ctl_t atel_log_ctl;

void InitApp(uint8_t boot);         /* I/O and Peripheral Initialization */

void atel_timerTickHandler(uint32_t tickUnit_ms);
void atel_timer_s(uint32_t seconds);
void atel_uart_restore(void);
void device_poweroff_delay_refresh(void);
void device_poweroff_delay_detect(uint32_t delta);
void device_bootloader_enter_dealy(uint32_t delta);

void init_config(void);
void gpio_init(void);
void setdefaultConfig(uint8_t bat_en);
void configGPIO(int index, uint8_t status);
void SetGPIOOutput(uint8_t index, bool Active);
void turnOnDevice(void);
void turnOffDeviceCancel(uint8_t once);
void turnOffDevice(void);
void MCU_TurnOn_Device(void);
void MCU_TurnOff_Device(void);
void MCU_Wakeup_Device_App(void);
void MCU_Sleep_Device_App(void);
int8_t atel_io_queue_process(void);
void GetRxCommandEsp(uint8_t RXByte);
void CheckInterrupt(void);

void BuildFrame(uint8_t cmd, uint8_t * pParameters, uint8_t nSize);
void ble_connection_status_inform(uint16_t channel, uint8_t state);
void ble_inform_device_mac(void);
void monet_Vcommand(void);
void HandleRxCommand(void);

void ble_aus_advertising_stop(void);
void ble_aus_advertising_start(void);
void ble_advertisement_status_inform(uint8_t state);
void advertising_data_update(uint32_t interval, uint16_t duration, uint8_t update);

#endif /* USER_H_ */
