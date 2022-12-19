#ifndef _DRV_MCP25XXFD_H
#define _DRV_MCP25XXFD_H

#ifdef __cplusplus  // Provide C++ Compatibility
extern "C" {
#endif

#include "drv_canfdspi_api.h"

#define CAN_DATA_RATE_CHECK_EN (1)

#define CAN_ROLE_CLIENT         (0)
#define CAN_ROLE_SERVER         (1)
#define CAN_ROLE_SELECT         CAN_ROLE_CLIENT

#define CAN_CLIENT_ID0          0x100

#define CAN_SERVER_ID0          0x80

#define CAN_TX_SETP_BYTES_MAX           (32)
#define CAN_TX_RBUF_BYTES               (10 * 1024)
#define CAN_RX_POOL_QUEUE_NUM           (50)
#define CAN_RX_POOL_QUEUE_CELL_SIZE     (200)

#define CAN_CONTROLLER_TEST_EN  (0)

void can_controller_init(void);
void can_controller_tx(uint8_t* pParam, uint8_t Length, uint16_t channel);
void can_controller_process(uint8_t rx_ready, uint32_t sys_time_ms);
void can_rx_pool_queue_process(void);
uint8_t can_rx_pool_queue_is_full(void);
void can_controller_test(void);

#ifdef __cplusplus
}
#endif

#endif /* _DRV_MCP25XXFD_H */
