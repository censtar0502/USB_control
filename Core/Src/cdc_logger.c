#include "cdc_logger.h"
#include "usbd_cdc_if.h"
#include "main.h"
#include <string.h>

#ifndef CDC_LOG_RING_SIZE
#define CDC_LOG_RING_SIZE   (4096u)
#endif

/* Ring buffer */
static uint8_t  s_ring[CDC_LOG_RING_SIZE];
static volatile uint32_t s_head = 0;
static volatile uint32_t s_tail = 0;

/* TX state */
static volatile uint8_t  s_tx_busy = 0;
static volatile uint32_t s_dropped = 0;

/* Packet buffer (must remain valid until TX complete) */
static uint8_t  s_tx_pkt[CDC_DATA_FS_MAX_PACKET_SIZE] __attribute__((aligned(32)));
static uint16_t s_tx_len = 0;

static uint32_t ring_used(uint32_t head, uint32_t tail)
{
    if (head >= tail) return (head - tail);
    return (CDC_LOG_RING_SIZE - (tail - head));
}

static uint32_t ring_free(uint32_t head, uint32_t tail)
{
    /* keep 1 byte empty to distinguish full/empty */
    uint32_t used = ring_used(head, tail);
    if (used >= (CDC_LOG_RING_SIZE - 1u)) return 0u;
    return (CDC_LOG_RING_SIZE - 1u - used);
}

static void dcache_clean_txpkt(uint16_t len)
{
#if (__DCACHE_PRESENT == 1U)
    /* Clean the cache for the packet buffer before USB reads it */
    uint32_t n = (uint32_t)len;
    if (n == 0u) return;

    /* round up to 32 bytes */
    n = (n + 31u) & ~31u;
    SCB_CleanDCache_by_Addr((uint32_t*)s_tx_pkt, (int32_t)n);
#else
    (void)len;
#endif
}

void CDC_LOG_Init(void)
{
    __disable_irq();
    s_head = 0;
    s_tail = 0;
    s_tx_busy = 0;
    s_dropped = 0;
    s_tx_len = 0;
    __enable_irq();
}

uint32_t CDC_LOG_GetDroppedCount(void)
{
    return s_dropped;
}

void CDC_LOG_Push(const char *s)
{
    if (s == NULL) return;

    uint32_t len = (uint32_t)strlen(s);
    if (len == 0u) return;

    __disable_irq();
    uint32_t head = s_head;
    uint32_t tail = s_tail;

    if (ring_free(head, tail) < len)
    {
        /* no space: drop entire message */
        s_dropped++;
        __enable_irq();
        return;
    }

    for (uint32_t i = 0; i < len; i++)
    {
        s_ring[head] = (uint8_t)s[i];
        head++;
        if (head >= CDC_LOG_RING_SIZE) head = 0;
    }

    s_head = head;
    __enable_irq();
}

void CDC_Log(const char *msg)
{
    if (msg == NULL) {
        return;
    }

    CDC_LOG_Push(msg);
    CDC_LOG_Push("\r\n");
}

void CDC_LOG_Task(void)
{
    if (s_tx_busy) return;

    __disable_irq();
    uint32_t head = s_head;
    uint32_t tail = s_tail;
    __enable_irq();

    uint32_t used = ring_used(head, tail);
    if (used == 0u) return;

    uint16_t to_send = (used > (uint32_t)CDC_DATA_FS_MAX_PACKET_SIZE) ?
                       (uint16_t)CDC_DATA_FS_MAX_PACKET_SIZE :
                       (uint16_t)used;

    /* Prepare packet (do not advance tail until USBD_OK) */
    uint32_t t = tail;
    for (uint16_t i = 0; i < to_send; i++)
    {
        s_tx_pkt[i] = s_ring[t];
        t++;
        if (t >= CDC_LOG_RING_SIZE) t = 0;
    }
    s_tx_len = to_send;

    dcache_clean_txpkt(s_tx_len);

    uint8_t res = CDC_Transmit_FS(s_tx_pkt, s_tx_len);
    if (res == USBD_OK)
    {
        __disable_irq();
        s_tail = t;
        s_tx_busy = 1u;
        __enable_irq();
    }
    else
    {
        /* BUSY/FAIL: keep queue intact, try later */
    }
}

void CDC_LOG_TxCpltCallback(void)
{
    s_tx_busy = 0u;
}
