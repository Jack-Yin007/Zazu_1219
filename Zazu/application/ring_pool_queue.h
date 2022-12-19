/*
 *  ring_pool_queue.h
 *
 *  Created on: Jan 08, 2021
 *      Author: Yangjie Gu
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __RING_POOL_QUEUE_H_
#define __RING_POOL_QUEUE_H_
#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include <string.h>

typedef struct {
    uint8_t*            p_data;
    uint32_t            length;
    uint32_t            channel;
} ring_pool_cell_t;

typedef struct
{
  ring_pool_cell_t*     p_buf;
  uint32_t              in;
  uint32_t              out;
  uint32_t              num;
  uint32_t              max;
} ring_pool_queue_t;

#define RING_POOL_QUEUE_INIT(POOL_QUEUE, CELL_NUM, CELL_LEN)                  \
    do                                                                        \
    {                                                                         \
        uint32_t                i                       = 0;                  \
        static ring_pool_cell_t cell[CELL_NUM]          = {0};                \
        static uint8_t          buf[CELL_NUM][CELL_LEN] = {0};                \
                                                                              \
        POOL_QUEUE.p_buf    = cell;                                           \
        POOL_QUEUE.in       = 0;                                              \
        POOL_QUEUE.out      = 0;                                              \
        POOL_QUEUE.num      = 0;                                              \
        POOL_QUEUE.max      = CELL_NUM;                                       \
                                                                              \
        for (i = 0; i < CELL_NUM; i++)                                        \
        {                                                                     \
            cell[i].p_data  = buf[i];                                         \
            cell[i].length  = 0;                                              \
            cell[i].channel = 0xffff;                                         \
        }                                                                     \
    } while (0)

int8_t ring_pool_queue_in           (ring_pool_queue_t* p_queue, ring_pool_cell_t* p_buf);
int8_t ring_pool_queue_out          (ring_pool_queue_t* p_queue, ring_pool_cell_t* p_buf);
int8_t ring_pool_queue_delete_one   (ring_pool_queue_t* p_queue);
int8_t ring_pool_queue_is_empty     (ring_pool_queue_t* p_queue);
int8_t ring_pool_queue_is_full      (ring_pool_queue_t* p_queue);

#ifdef __cplusplus
}
#endif
#endif /* __SERIAL_FRAME_H_ */
