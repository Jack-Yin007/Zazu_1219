/*
 * ring_buf.h
 *
 *  Created on: May 26, 2020
 *      Author: Yangjie Gu
 */

#ifndef __SELF_RING_BUFFER_
#define __SELF_RING_BUFFER_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct
{
    uint8_t *p_buf;
    uint32_t size;
    uint32_t in;
    uint32_t out;
    uint32_t num;
} ring_buffer_t;

extern int8_t ring_buffer_init(ring_buffer_t *p, uint8_t *p_buffer, uint32_t size);
extern int8_t ring_buffer_in(ring_buffer_t *p_buf, uint8_t data);
extern void ring_buffer_in_escape(ring_buffer_t *pQueue, uint8_t value);
extern int8_t ring_buffer_out(ring_buffer_t *p_buf, uint8_t *p_data);
extern uint32_t ring_buffer_out_len(ring_buffer_t *p_buf, uint8_t *p_data, uint32_t length);
extern uint8_t ring_buffer_is_empty(ring_buffer_t *p_buf);
extern uint32_t ring_buffer_num_get(ring_buffer_t *p_buf);
extern uint32_t ring_buffer_left_get(ring_buffer_t *p_buf);

#ifdef __cplusplus
}
#endif

#endif /* __SELF_RING_BUFFER_ */
