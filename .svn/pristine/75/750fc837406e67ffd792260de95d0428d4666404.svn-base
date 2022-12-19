/*
 *  serial_frame.h
 *
 *  Created on: Dec 29, 2020
 *      Author: Yangjie Gu
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SERIAL_FRAME_H_
#define __SERIAL_FRAME_H_
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "include.h"
#include "ring_buf.h"

#define SF_CMD_CHECKSUM                     (1)
#define SF_CMD_ESCAPE                       (1)
#define SF_USE_CHECKSUM                     (1)
#define SF_MAX_COMMAND                      (244)

#define SF_STEP_SIZE_BYTES                  (64)

typedef enum {
    SF_WAIT_FOR_DOLLAR,
    SF_GET_FRAME_LENGTH,
    SF_GET_COMMAND,
    SF_GET_CARRIAGE_RETURN
} sf_state_t;

typedef enum {
    SF_GET_NORMAL,
    SF_GET_ESCAPE
} sf_escape_state_t;

typedef struct {
    sf_state_t              state;
    sf_escape_state_t       escape;
    uint8_t                 cmd;
    uint8_t                 length;
    uint8_t                 remaining;
    uint8_t                 checksum;
    uint8_t                 parmCount;
    uint8_t                 data[SF_MAX_COMMAND];
} sf_rx_frame_t;

typedef struct {
    uint8_t*                p_rx_buf;
    uint32_t                rx_buf_size;
    uint8_t*                p_tx_buf;
    uint32_t                tx_buf_size;
    int32_t                 (* rx_data)(uint8_t* p_data, uint32_t* p_len);
    int32_t                 (* rx_process)(uint8_t cmd, uint8_t* p_data, uint32_t len);
    int32_t                 (* tx_handle)(uint8_t* p_data, uint32_t len);
} sf_control_block_init_t;

typedef struct {
    sf_rx_frame_t           rx_frame;
    uint8_t                 rx_tmp_buf[SF_STEP_SIZE_BYTES];
    uint32_t                rx_tmp_len;
    ring_buffer_t           tx_buf;
    uint8_t                 tx_tmp_buf[SF_STEP_SIZE_BYTES];
    uint32_t                tx_tmp_len;
    uint32_t                tx_wait_count;
    int32_t                 (* rx_data)(uint8_t* p_data, uint32_t* p_len);
    int32_t                 (* rx_process)(uint8_t cmd, uint8_t* p_data, uint32_t len);
    int32_t                 (* tx_handle)(uint8_t* p_data, uint32_t len);
} sf_control_block_t;

int32_t     sf_data_init            (sf_control_block_t* p_sf_cb, sf_control_block_init_t* p_sf_cb_init);
int32_t     sf_data_process         (sf_control_block_t* p_sf_cb);
int32_t     sf_data_send            (sf_control_block_t* p_sf_cb, uint8_t cmd, uint8_t* p_data, uint32_t len);

#define SF_TEST_FUNCTIONALITIES_EN 0
#if SF_TEST_FUNCTIONALITIES_EN
void sf_frame_test_init(void);
void sf_frame_test_process(void);
#endif /* SF_TEST_FUNCTIONALITIES_EN */

#ifdef __cplusplus
}
#endif
#endif /* __SERIAL_FRAME_H_ */
