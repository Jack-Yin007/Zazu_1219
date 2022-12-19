
#include "ring_buf.h"

#ifndef NULL
#define NULL 0
#endif

int8_t ring_buffer_init(ring_buffer_t *p, uint8_t *p_buffer, uint32_t size)
{
    if ((p_buffer == NULL) || (size == 0))
    {
        return -1;
    }

    p->p_buf = p_buffer;
    p->size = size;
    p->in = 0;
    p->out = 0;
    p->num = 0;

    return 0;
}

int8_t ring_buffer_in(ring_buffer_t *p_buf, uint8_t data)
{
    if (p_buf->num == p_buf->size)
    {
        return -1;
    }

    p_buf->p_buf[p_buf->in] = data;
    p_buf->in++;
    p_buf->in %= p_buf->size;

    if (p_buf->out < p_buf->in)
    {
        p_buf->num = (p_buf->in - p_buf->out);
    }
    else
    {
        p_buf->num = (p_buf->size + p_buf->in - p_buf->out);
    }

    return 0;
}

uint32_t ring_buffer_in_len(ring_buffer_t *p_buf, uint8_t *p_data, uint32_t length)
{
    uint32_t i = 0;

    for (i = 0; i < length; i++)
    {
        if (ring_buffer_in(p_buf, *p_data))
        {
            break;
        }
        p_data++;
    }

    return i;
}

void ring_buffer_in_escape(ring_buffer_t *pQueue, uint8_t value)
{
    if(value=='$')
    {
        ring_buffer_in(pQueue, '!');
        ring_buffer_in(pQueue, '0');
    }
    else if(value == '!')
    {
        ring_buffer_in(pQueue, '!');
        ring_buffer_in(pQueue, '1');
    }
    else
    {
        ring_buffer_in(pQueue, value);
    }
}

int8_t ring_buffer_out(ring_buffer_t *p_buf, uint8_t *p_data)
{
    if (p_buf->num == 0)
    {
        return -1;
    }

    *p_data = p_buf->p_buf[p_buf->out];
    p_buf->out++;
    p_buf->out %= p_buf->size;
    
    if (p_buf->out <= p_buf->in)
    {
        p_buf->num = (p_buf->in - p_buf->out);
    }
    else
    {
        p_buf->num = (p_buf->size + p_buf->in - p_buf->out);
    }

    return 0;
}

uint32_t ring_buffer_out_len(ring_buffer_t *p_buf, uint8_t *p_data, uint32_t length)
{
    uint32_t i = 0;

    for (i = 0; i < length; i++)
    {
        if (ring_buffer_out(p_buf, p_data))
        {
            break;
        }
        p_data++;
    }

    return i;
}

uint8_t ring_buffer_is_empty(ring_buffer_t *p_buf)
{
    if (p_buf->num == 0)
    {
        return 1;
    }
    else
    {
        return 0;
    }
    
}

uint32_t ring_buffer_num_get(ring_buffer_t *p_buf)
{
    return p_buf->num;
}

uint32_t ring_buffer_left_get(ring_buffer_t *p_buf)
{
    return (p_buf->size - p_buf->num);
}
