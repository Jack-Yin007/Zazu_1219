/*
 *  ring_pool_queue.c
 *
 *  Created on: Jan 08, 2021
 *      Author: Yangjie Gu
 */

#include "ring_pool_queue.h"

int8_t ring_pool_queue_in(ring_pool_queue_t* p_queue, ring_pool_cell_t* p_buf)
{
    if ((p_buf->p_data == NULL) || (p_buf->length == 0) || (p_queue == NULL))
    {
        return -1;
    }

    if (p_queue->num == p_queue->max)
    {
        return -2;
    }

    memcpy(p_queue->p_buf[p_queue->in].p_data, p_buf->p_data, p_buf->length);
    p_queue->p_buf[p_queue->in].length = p_buf->length;
    p_queue->p_buf[p_queue->in].channel = p_buf->channel;
    p_queue->in++;
    p_queue->in %= p_queue->max;

    if (p_queue->in > p_queue->out)
    {
        p_queue->num = p_queue->in - p_queue->out;
    }
    else
    {
        p_queue->num = p_queue->max + p_queue->in - p_queue->out;
    }

    return 0;
}

ring_pool_cell_t* ring_pool_queue_get_one(ring_pool_queue_t* p_queue)
{
    ring_pool_cell_t *tmp = NULL;

    if (p_queue == NULL)
    {
        return NULL;
    }

    if (p_queue->num == p_queue->max)
    {
        return NULL;
    }

    tmp = &(p_queue->p_buf[p_queue->in]);
    tmp->length = 0;
    tmp->channel = 0;

    p_queue->in++;
    p_queue->in %= p_queue->max;

    if (p_queue->in >= p_queue->out)
    {
        p_queue->num = p_queue->in - p_queue->out;
    }
    else
    {
        p_queue->num = p_queue->max + p_queue->in - p_queue->out;
    }

    return tmp;
}

int8_t ring_pool_queue_out(ring_pool_queue_t* p_queue, ring_pool_cell_t* p_buf)
{
    if ((p_buf == NULL) || (p_queue == NULL))
    {
        return -1;
    }

    if (p_queue->num == 0)
    {
        return -2;
    }

    p_buf->p_data = p_queue->p_buf[p_queue->out].p_data;
    p_buf->length = p_queue->p_buf[p_queue->out].length;
    p_buf->channel = p_queue->p_buf[p_queue->out].channel;

    return 0;
}

int8_t ring_pool_queue_delete_one(ring_pool_queue_t* p_queue)
{
    if (p_queue == NULL)
    {
        return -1;
    }

    if (p_queue->num == 0)
    {
        return -2;
    }

    p_queue->p_buf[p_queue->out].channel = 0xffff;
    p_queue->p_buf[p_queue->out].length = 0;
    p_queue->out++;
    p_queue->out %= p_queue->max;

    if (p_queue->in >= p_queue->out)
    {
        p_queue->num = p_queue->in - p_queue->out;
    }
    else
    {
        p_queue->num = p_queue->max + p_queue->in - p_queue->out;
    }

    return 0;
}

int8_t ring_pool_queue_is_empty(ring_pool_queue_t* p_queue)
{
    if (p_queue->num == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

int8_t ring_pool_queue_is_full(ring_pool_queue_t* p_queue)
{
    if (p_queue->num == p_queue->max)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

#if 0

#include "logger.h"

ring_pool_queue_t pool_queue_test = {0};

void ring_pool_queue_init_test(void)
{
    RING_POOL_QUEUE_INIT(pool_queue_test, 10, 136);
}

void ring_pool_queue_test(void)
{
    static uint32_t count = 0;

    if (!ring_pool_queue_is_full(&pool_queue_test))
    {
        uint8_t i = 0;
        uint8_t buf[136] = {0};
        ring_pool_cell_t cell = {0};

        for (i = 0; i < sizeof(buf); i++)
        {
            buf[i] = i;
        }

        count++;
        buf[0] = (count >> 24) & 0xff;
        buf[1] = (count >> 16) & 0xff;
        buf[2] = (count >> 8) & 0xff;
        buf[3] = (count >> 0) & 0xff;

        cell.length = 136;
        cell.p_data = buf;
        cell.channel = count;

        ring_pool_queue_in(&pool_queue_test, &cell);

        logger_raw("ring_pool_queue_test in: %d.\r\n", count);
        // printf_hex_and_char(cell.p_data, cell.length);
    }

    if (ring_pool_queue_is_full(&pool_queue_test))
    {
        ring_pool_cell_t cell = {0};
        ring_pool_queue_out(&pool_queue_test, &cell);
        logger_raw("ring_pool_queue_test out: %d.\r\n", cell.channel);
        // printf_hex_and_char(cell.p_data, cell.length);
        ring_pool_queue_delete_one(&pool_queue_test);
    }
}

#endif


