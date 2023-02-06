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
#define TIME_UNIT                       (1000)        //ms
#define TIME_UINT_SECOND_TIMER_PERIOD   (5000)        //ms
#define TIME_UNIT_CHANGE_WHEN_SLEEP     (0)
#define TIME_UNIT_IN_SLEEP_NORMAL       (TIME_UNIT)   //ms
#define TIME_UNIT_IN_SLEEP_HIBERNATION  (30000)       //ms
#define TIME_UNIT_IN_SHIPPING_MODE      (10000)       //ms

#define BLE_DISCONNECT_DELAY_MAX_MS    (5 * 60 * 1000)
#define BLE_DISCONNECT_DATA_RATE_THRESHOLD (100)

#define CAMERA_HEARTBEAT_INTERVAL_MS    (10000)       //ms
#define CAMERA_POWER_OFF_DELAY_EN (0)
#define CAMERA_POWER_OFF_DELAY_MS (3 * 1000)
#define CAMERA_POWER_ON_DELAY_MS (20 * 1000)
#define CAMERA_POWER_ON_FACTORY_DELAY_MS (10 * 60 * 1000)
#define CAMERA_POWER_ON_FACTORY_DELAY_MIN_MAX (6 * 60)
#define CAMERA_POWER_ON_PENDING_MS (5 * 1000)
#define CAMERA_POWER_UP_ABNORMAL_DEBOUNCE (15000)     //ms
#define CAMERA_POWER_UP_ABNORMAL_RETRY_MAX (5)
#define CAMERA_FIRST_POWER_OFF_DELAY_MS (10 * 60 * 1000)
#define CAMERA_POWER_UP_MINIMUM_INTERVAL_MS (30 * 1000)
#define CAMERA_UPGRADING_MODE_TIMEOUT_MS (20 * 60 * 1000)

#define WATCHDOG_DEFAULT_RELOAD_VALUE (25 * 60 * 60)            // 25hr
#define WATCHDOG_DEFAULT_RELOAD_VALUE_MIN (1 * 60)              // 1min
#define WATCHDOG_DEFAULT_RELOAD_VALUE_MAX (30 * 24 * 60 * 60)   // 1mon

#define BLE_IN_PAIRING_MODE_TIMEOUT_MS (5 * 60 * 1000)

#define DEVICE_ADC_SAMPLE_PERIOD_MS         (2 * 1000) //ms

#define MCU_DATA_HANDLE_TIMEOUT_MS (10 * 1000)

#define MCU_APP_TO_BOOT_DELAY_MS (5000)

#define MILISECOND_TO_SECONDS(x) ((x) / (1000))

#define IO_CMD_CHAR_BLE_RECV ('1')
#define IO_CMD_CHAR_BLE_SEND ('2')
#define IO_CMD_CHAR_BLE_CTRL ('3')
#define IO_CMD_CHAR_CAN_RECV ('4')
#define IO_CMD_CHAR_CAN_SEND ('5')

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

// Readjust voltage trigger thresholds to avoid zombie mode
#define CHARGING_MODE2_DISABLE_LOW_THRESHOLD    (8300ul)
#define CHARGING_MODE2_DISABLE_HIGH_THRESHOLD   (8400ul)
#define BUB_CRITICAL_THRESHOLD_DELTA            (200u)        // mv
#define BUB_CRITICAL_THRESHOLD_FIRST_DELTA      (100u)        // mv
#define BUB_THRESHOLD_CHANGE_TEMP               (7400)        // mV
//#define BUB_CRITICAL_THRESHOLD                  (6600)        // mV
#define BUB_CRITICAL_THRESHOLD                  (6700)        // mV
#define BUB_LOW_THRESHOLD                       (7000)        // mV
//#define BUB_LOW_THRESHOLD                       (7400)        // mV
#define BUB_HIGH_THRESHOLD                      (8400)        // mV
#define MAIN_ADC_MAIN_OFF                       (5500)        // mV
#define MAIN_ADC_MAIN_VALID                     (6000)        // mV
#define SOLAR_ADC_VALID                         (2900)           // mV WARNING: Solar >2.8v
#define SOLAR_CHARGE_POWER_VALID                (1000*1000)   // uw
#define SOLAR_GET_POWER_COUNT                   (5)         
#define CHARGE_RESTART_CHECK_COUNT              (5)

#define BUB_CRITICAL_DEBOUNCE_MS                (10 * 1000)
#define BUB_VALID_DEBOUNCE_MS                   (10 * 1000)
#define MAIN_VALID_DEBOUNCE_MS                  (10 * 1000)

#define CHARGER_OFF 0
#define CHARGER_ON 1

// 10 minutes delay
#define CHARGER_DELAY (60 * 10)

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

#define MAX_EXT_GPIOS (8)

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

#define PWR_EVENT_MAIN_PWR_PLUG_IN 1
#define PWR_EVENT_MAIN_PWR_GONE 2

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

#define BLE_CHANNEL_NUM_MAX (1)
#define BLE_MAC_ADDRESS_LEN (6)
#define BLE_CONNECTION_STATUS_NOTVALID (0)
#define BLE_CONNECTION_STATUS_NOT_CONNECTED (1)
#define BLE_CONNECTION_STATUS_CONNECTED (2)
#define BLE_CONNECTION_STATUS_MAC_SET (3)
#define BLE_CONN_PARAM_UPDATE_RETRY_MAX (3)
#define BLE_CONN_PARM_CHECK_IN_HEARTBEAT_COUNT (2)

#define CAMERA_FIRMWARE_VERSION_SIZE (4)
#define CAMERA_SERIAL_NUMBER_SIZE_MAX (32)
#define CAMERA_CONFIGURE_FILE_VERSION_SIZE (2)

typedef struct
{
    uint8_t connect_status;
    uint16_t handler;
    uint8_t mac_addr[BLE_MAC_ADDRESS_LEN];
} BLE_INFORMATION_t;

typedef struct
{
    uint16_t channel;
    uint8_t status;
    uint8_t error;
} ble_send_result_t;


typedef struct
{
    uint16_t min_100us;
    uint16_t max_100us;
    uint16_t latency;
    uint32_t timeout_100us;
} CAMERA_CONN_PARAM_t;

typedef struct {
    IoRxFrameStruct     iorxframe;
    ring_buffer_t       txQueueU1;

    uint16_t            AdcBackup;
    uint16_t            AdcBackupRaw;
    uint16_t            AdcMain;
    uint16_t            AdcSolar;
    uint16_t            debounceBatValidMs;
    uint16_t            debounceBatCriticalMs;
      uint16_t          debounceBatActiveMs;
    uint16_t            debounceMainValidMs;
    uint8_t             batPowerValid;
    uint8_t             batPowerCritical;
    uint8_t             batPowerActive;
    uint8_t             mainPowerValid;
    uint16_t            batCriticalThresh;          // Threshold MV
    uint16_t            batLowThresh;               // Threshold MV

    uint32_t            SleepAlarm;
    uint32_t            sysTickUnit;
    uint8_t             SleepStateChange;
    SleepState_e        SleepState;
    MainState_e         MainState;
    uint8_t             batPowerSave;

    uint8_t             cameraFwVersion[CAMERA_FIRMWARE_VERSION_SIZE];
    uint8_t             cameraSerialNumber[CAMERA_SERIAL_NUMBER_SIZE_MAX + 1]; // Byte 0: Serial Number Length
    uint32_t            cameraofftime;
    uint8_t             cameraPowerOn;
    uint8_t             cameraPowerOff;
    uint8_t             cameraPowerOnPending;
    uint32_t            cameraPowerOnTimeMs;
    uint32_t            cameraPowerOffTimeMs;
    uint32_t            cameraPowerOffDelay;
    uint32_t            cameraPowerOnDelay;
    uint8_t             cameraPowerAbnormal;
    uint16_t            cameraPowerAbnormalRetry;
    uint32_t            cameraBootEnterDelay;
    uint16_t            cameraHID;
    uint8_t             cameraRev;
    uint8_t             cameraConfVer[CAMERA_CONFIGURE_FILE_VERSION_SIZE];
    uint32_t            cameraTurnOnTimes;
    uint32_t            cameraTurnOffTimes;

    uint8_t             firstPowerUp;
    uint8_t             uartTXDEnabled;
    uint8_t             uartToBeDeinit;
    uint8_t             uartToBeInit;
    uint8_t             uartTickCount;
    uint8_t             appActive;

    uint16_t            data_recv_channel;
    uint8_t             RecvDataEvent;
    uint32_t            mcuDataHandleMs;

    uint8_t             bleConnectionStatus;
    uint32_t            bleDisconnectDelayTimeMs;

    uint8_t             bleConnectionEvent;
    uint8_t             bleDisconnectEvent;
    uint8_t             ble_mac_addr[BLE_MAC_ADDRESS_LEN];
    uint8_t             ble_peer_mac_addr[BLE_MAC_ADDRESS_LEN];
    uint8_t             ble_filter_mac_addr[BLE_CHANNEL_NUM_MAX][BLE_MAC_ADDRESS_LEN];
    BLE_INFORMATION_t   ble_info[BLE_CHANNEL_NUM_MAX];
    CAMERA_CONN_PARAM_t ble_conn_param[BLE_CHANNEL_NUM_MAX];
    uint16_t            ble_conn_param_update_ok[BLE_CHANNEL_NUM_MAX];
    uint16_t            ble_conn_param_update_retry[BLE_CHANNEL_NUM_MAX];
    uint8_t             ble_conn_param_check_in_heartbeat[BLE_CHANNEL_NUM_MAX];
    int16_t             ble_rx_power;
    int16_t             ble_tx_power;

    ble_send_result_t   ble_send_result;

    uint8_t             ldr_int_status;
    uint8_t             ldr_enable_status;
    uint8_t             led_flash_enable;

    uint8_t             mux_can;

    uint16_t            ChargerDelay;
    uint8_t             ChargerStatus;
    uint8_t             PrevChargerStatus;
    uint16_t            ChargerRestartValue;

    uint8_t             CANExistChargerNoExist;
    uint8_t             bleShouldAdvertise;

    uint8_t             in_pairing_mode;
    uint8_t             pairing_success;
    uint32_t            in_pairing_mode_time;
    uint8_t             to_advertising;
    uint8_t             is_advertising;
    uint8_t             to_advertising_result;
    uint8_t             to_advertising_recover;
    uint8_t             to_advertising_without_white_list;

    uint8_t             upgrading_mode_enter;
    uint8_t             in_upgrading_mode;
    uint32_t            in_upgrading_mode_time;

    uint8_t             charge_mode2_disable; // mode2 solar panel connect to battery directly
    uint8_t             charge_mode1_disable; // mode1 solar panel charge the battery by mp2762a
    uint8_t             charge_mode_disable;  // mode1 mode2 solar panel disable.
    uint8_t             charge_mode_disable_nrf;  //
    uint8_t             charge_status;   //charge status    NalaMcu-194
    bool                charge_state_report_vbat_AA;
    bool                charge_state_report_vbat_A;
    bool                solar_mode_select_delay;
    uint8_t             solar_chg_mode_choose;   //remain what's mode next choose. 
    uint32_t            AdcBackupAccuracy;

    // uint8_t             in_extern_power_charging_process;
    // uint8_t             led_state_change_in_button; // for indicate the led state.
    // uint8_t             led_state_change_in_pairing_button;
    // uint8_t             led_state_resume;           //back to sleep or shipping mode after botton process
    uint8_t             device_in_shipping_mode;  //the flag whether in shipping mode. 1:enter shipping mode. 0:exit shipping mode. 

    //uint32_t             restartReason;
    //uint32_t             restartTimes;
    uint32_t            camera_commun_fail_times;
    uint32_t            camera_files_read_fail_times;
    int16_t             temp;
	int16_t             temp_ntc;
	int32_t             temp_nordic;
    int32_t             temp_set;

    uint8_t              restart_indicate;    // for power up,the device should flash the green led for 5 seconds    
    uint32_t             ble_disconnnet_delayTimeMs;   
    uint8_t              boot_major;
    uint8_t              boot_minor;
    uint8_t              boot_revision;
    uint8_t              boot_build; 
} monet_struct;

//1226 21 add charge status
typedef enum
{
    CHARGE_STATUS_SOLAR_MODE_1_PROCESS = 0, // changer status when in solar mode 1
    CHARGE_STATUS_SOLAR_MODE_1_FULL,        // changer status when battery fully charged in solar mode 1
    CHARGE_STATUS_SOLAR_MODE_2,             // changer status when in solar mode 2
    CHARGE_STATUS_SOLAR_LOWPOWER_MODE_2,    // changer status when in solar low power
    CHARGE_STATUS_SOLAR_LOWPOWER_OFF,
    CHARGE_STATUS_EXTERNAL_POWER_PROCESS,   // changer status when in external power
    CHARGE_STATUS_EXTERNAL_POWER_FULL,      // changer status  when battery fully charged in external power
    CHARGE_STATUS_NO_SOURCES,
    CHARGE_STATUS_NO_SOURCES_MODE_2,
} charge_status_e;

typedef struct
{
    uint32_t restartReason;
    uint32_t restartTimes;
    uint32_t recordTimes;
   // uint32_t recordData;
    uint32_t resumeTimes;

    uint16_t batCriticalThresh;          // Here store the value from app. Now the 2 paramter don't need report to nala.
    uint16_t batLowThresh;               // Threshold MV
} debug_tlv_info_t;

typedef enum
{
    NO_POWER_CHARGING_PROCESS = 0 , // reserved
    EXTERN_POWER_CHARGING_PROCESS, //only extern power charging
    EXTERN_POWER_CHARGING_FULL,    //the charging status is full in only extern power
    SOLAR_POWER_CHARGING_PROCESS_MODE1,
    SOLAR_POWER_CHARGING_PROCESS_MODE2,  
    SOLAR_POWER_CHARGING_FULL,
    SOLAR_LOW_POWER_CHARGING_OFF,
    EXTERN_SOLAR_POWER_CHARGING_PROCESS,
    EXTERN_SOLAR_POWER_CHARGING_FULL,
    POWER_STATE_MAX_COUNT,

} power_state_e;

typedef struct 
{
    uint8_t charging_process;
    uint8_t led_state_change_in_button;   // for indicate the led state when button was pushed
    uint8_t led_state_change_in_pairing_button; 
    uint8_t led_state_resume;            //back to sleep or shipping mode after botton process
    //uint8_t charger_resume_count;

} power_led_state_t;

typedef struct
{
    bool  isBusy; 
    uint8_t  status;
    uint32_t duration_time;
    power_led_state_t  led_state[POWER_STATE_MAX_COUNT];

} power_state_t;





typedef struct {
    uint8_t             button_power_on;
    uint8_t             reset_from_dfu;
    uint8_t             reset_from_shipping_mode;
    uint8_t             reset_from_shipping_mode_donot_adv_untill_pair;
    uint8_t             ble_peripheral_paired;
} device_reset_info_t;

#define DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_POS        (0)
#define DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_MSK        (0x3 << DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_POS)
#define DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_INVALID    (0x0 << DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_POS)
#define DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_VALID      (0x3 << DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_POS)
#define DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_WAKE       (0x2 << DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_POS)
#define DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_PREWAKE    (0x1 << DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_POS)

#define DEVICE_SHIPPING_MODE_WAKE_KEY_POS               (2)
#define DEVICE_SHIPPING_MODE_WAKE_KEY_MSK               (0x1 << DEVICE_SHIPPING_MODE_WAKE_KEY_POS)
#define DEVICE_SHIPPING_MODE_WAKE_KEY_VALID             (0x1 << DEVICE_SHIPPING_MODE_WAKE_KEY_POS)

#define DEV_SHIP_WAKE(x) DEVICE_SHIPPING_MODE_WAKE_ ## x
#define DEV_SHIP_WAKE_POWER_SRC_GET(x) ((x) & DEVICE_SHIPPING_MODE_WAKE_MAIN_POWER_MSK)
#define DEV_SHIP_WAKE_KEY_SRC_GET(x) ((x) & DEVICE_SHIPPING_MODE_WAKE_KEY_MSK)

typedef struct {
    uint8_t             mode;
    uint8_t             pending;
    uint16_t            wakesrc;
    uint32_t            count;
} device_shipping_mode_t;

extern device_shipping_mode_t device_shipping_mode;

typedef enum
{
  HOLD = 0,
  RELEASE = 1
}FunCtrl;

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




#define DEVICE_RESET_FROM_SHIPPING                     (25)
#define DEVICE_RESET_FROM_POWEROFF                     (26)
#define DEVICE_RESET_FROM_BOOT                         (27)

#define DEVICE_RESET_FROM_DTMEX                        (28)
#define DEVICE_RESET_FROM_DTME                         (29)
#define DEVICE_RESET_FROM_INVALID                      (30)  //0321 22 wfy.add


#define DEVICE_RESET_SOURCE_BIT_INVALID                (0)
#define DEVICE_RESET_SOURCE_BIT_VALID                  (1)
#define DEVICE_RESET_SOURCE_BIT_MASK(P)                (0x0001 << P)
#define DEVICE_RESET_SOURCE_BIT_GET(S, P)              ((S >> P) & 0x0001)
#define DEVICE_RESET_SOURCE_BIT_IS_VALID(S, P)         (DEVICE_RESET_SOURCE_BIT_GET(S, P) == DEVICE_RESET_SOURCE_BIT_VALID)
#define DEVICE_RESET_SOURCE_BIT_IS_INVALID(S, P)       (DEVICE_RESET_SOURCE_BIT_GET(S, P) == DEVICE_RESET_SOURCE_BIT_INVALID)





extern monet_struct monet_data;
extern gpio_conf    monet_conf;
extern gpio_data    monet_gpio;
extern atel_log_ctl_t atel_log_ctl;
extern device_reset_info_t reset_info;
extern debug_tlv_info_t  debug_tlv_info;
extern power_state_t     power_state;

void InitApp(uint8_t alreay_init);         /* I/O and Peripheral Initialization */
void io_tx_queue_init(void);

void atel_timerTickHandler(uint32_t tickUnit_ms);
void atel_adc_converion(uint32_t delta, uint8_t force);
void atel_adc_conv_force(void);
void atel_timer_s(uint32_t seconds);
void atel_uart_restore(void);

void device_shipping_mode_check(void);
void device_shipping_auto_check(void);
void device_low_power_mode_check(uint8_t first_run);
void device_sleeping_recovery_botton_check(void);
void clock_hfclk_release(void); //ZAZUMCU-149  //the code from slp-01.
void clock_hfclk_request(void); //ZAZUMCU-149
uint16_t get_ble_conn_handle(void);

void init_config(void);
void gpio_init(void);
void setdefaultConfig(uint8_t bat_en);
void configGPIO(int index, uint8_t status);
void SetGPIOOutput(uint8_t index, bool Active);
void turnOnCamera(void);
void turnOffCameraDelay(uint32_t delay_ms);
void turnOffCamera(void);
void MCU_TurnOn_Camera(void);
void MCU_TurnOff_Camera(void);
void MCU_Wakeup_Camera(void);
void MCU_Sleep_Camera(void);
void MCU_Wakeup_Camera_App(void);
void MCU_Sleep_Camera_App(void);
int8_t atel_io_queue_process(void);
void GetRxCommandEsp(uint8_t RXByte);
void ldr_int_process(void);
void ble_connection_status_refresh(void);
void ble_connection_status_monitor(uint32_t delta);
void camera_poweroff_delay_refresh(void);
void camera_poweroff_delay_detect(uint32_t delta);
void camera_power_abnormal_detect(uint32_t delta);
void device_bootloader_enter_dealy(uint32_t delta);

#define DEVICE_BUTTON_IDLE_STATE (1)
#define DEVICE_BUTTON_CHECK_INTERVAL_MS (1000)
#define DEVICE_BUTTON_CHECK_INTERVAL_COUNT (10)
#define DEVICE_BUTTON_ACTION_THRESHOLD_POWER_ON_OFF (3)
#define DEVICE_BUTTON_ACTION_THRESHOLD_TURNON_CAMERA (5)
#define DEVICE_BUTTON_ACTION_THRESHOLD_UNPAIRING (8)
#define DEVICE_BUTTON_ACTION_THRESHOLD_PAIRING_BONDING (15)
typedef enum
{
    DEVICE_BUTTON_STATE_IDLE,
    DEVICE_BUTTON_STATE_DEBOUCE,
    DEVICE_BUTTON_STATE_PROCESS,

    DEVICE_BUTTON_STATE_MAX
} device_button_state_t;
void device_button_pattern_init(void);
int32_t device_button_state_get(void);
void device_button_pattern_process(void);

void CheckInterrupt(void);
void check_valid_power_state(uint32_t ms);
void BatteryPowerHoldEn(FunCtrl Status);
void BuildFrame(uint8_t cmd, uint8_t * pParameters, uint8_t nSize);
uint16_t ble_channel_get_from_mac(uint8_t *p_addr);
uint16_t ble_channel_get_from_handler(uint16_t handler);
uint16_t ble_connected_handler_get_from_channel(uint16_t channel);
uint16_t ble_information_set(uint16_t handler, uint8_t status, uint8_t *p_addr);
void ble_connection_channel_init(void);
void ble_connected_channel_clear(void);
uint16_t ble_connected_channel_num_get(void);
void ble_connection_status_inform(uint16_t channel, uint8_t state);
void monet_requestAdc(uint8_t adc);
void ble_inform_camera_mac(void);
void ble_data_rate_inform(uint32_t tx, uint32_t rx);
void monet_xAAcommand(uint8_t* pParam, uint8_t Length);
void HandleRxCommand(void);

#endif /* USER_H_ */
