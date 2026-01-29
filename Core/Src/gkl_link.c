/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gkl_link.c
  * @brief   Non-blocking GasKitLink (CENSTAR) master-side datalink/application framing
  *
  * Supports multiple independent UART links by keeping state in GKL_Link.
  * A global dispatcher maps HAL UART callbacks back to the right GKL_Link.
  ******************************************************************************
  */
/* USER CODE END Header */

#include "gkl_link.h"
#include <string.h>

/* ===================== Global registry (UART -> GKL_Link) ===================== */

static GKL_Link *s_links[GKL_MAX_LINKS];
static uint8_t   s_links_count = 0;

static void gkl_register_link(GKL_Link *link)
{
    if (link == NULL || link->huart == NULL) return;

    /* Already registered? */
    for (uint8_t i = 0; i < s_links_count; i++)
    {
        if (s_links[i] == link) return;
        if (s_links[i] && s_links[i]->huart == link->huart) return;
    }

    if (s_links_count < (uint8_t)GKL_MAX_LINKS)
    {
        s_links[s_links_count++] = link;
    }
}

static GKL_Link *gkl_find_by_huart(UART_HandleTypeDef *huart)
{
    if (huart == NULL) return NULL;
    for (uint8_t i = 0; i < s_links_count; i++)
    {
        if (s_links[i] && s_links[i]->huart == huart)
        {
            return s_links[i];
        }
    }
    return NULL;
}

/* ===================== Internal helpers ===================== */

/* ===================== Raw RX logging helpers (IRQ-safe) ===================== */

static inline void gkl_raw_rx_push(GKL_Link *link, uint8_t b)
{
    /* Single-producer (IRQ) / single-consumer (main loop) ring buffer */
    uint16_t next = (uint16_t)(link->raw_rx_head + 1u);
    if (next >= (uint16_t)GKL_RAW_RX_LOG_SIZE) next = 0u;

    if (next == link->raw_rx_tail)
    {
        link->raw_rx_overflow = 1u;
        /* Drop byte on overflow */
        return;
    }

    link->raw_rx_log[link->raw_rx_head] = b;
    link->raw_rx_head = next;
}

/* Cache line size on Cortex-M7 is 32 bytes */
#define DCACHE_LINE_SIZE  (32u)

static void dcache_clean_by_addr(void *addr, uint32_t len)
{
#if (__DCACHE_PRESENT == 1U)
    if (addr == NULL || len == 0u) return;
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end   = start + len;

    uintptr_t start_aligned = start & ~(uintptr_t)(DCACHE_LINE_SIZE - 1u);
    uintptr_t end_aligned   = (end + (DCACHE_LINE_SIZE - 1u)) & ~(uintptr_t)(DCACHE_LINE_SIZE - 1u);

    SCB_CleanDCache_by_Addr((uint32_t*)start_aligned, (int32_t)(end_aligned - start_aligned));
#else
    (void)addr; (void)len;
#endif
}

static uint8_t gkl_checksum_xor(const uint8_t *frame, uint8_t len)
{
    /* XOR from 2nd byte (index 1) to (n-1) byte (index len-2) */
    if (frame == NULL || len < 5u) return 0u;
    uint8_t x = 0u;
    for (uint8_t i = 1u; i < (uint8_t)(len - 1u); i++)
    {
        x ^= frame[i];
    }
    return x;
}

static uint8_t gkl_resp_data_len_for_cmd(char resp_cmd)
{
    /* Data length in Application Layer (not counting STX/addr/cmd/checksum) */
    switch (resp_cmd)
    {
        case 'S': return 2u;  /* Status response: 2 data bytes (e.g., "10" in "S10S") */
        case 'L': return 10u;
        case 'R': return 10u;
        case 'T': return 22u;
        case 'C': return 11u;
        case 'Z': return 6u;
        case 'D': return 2u;
        default:  return 0xFFu; /* unknown/variable */
    }
}

static void gkl_rx_reset(GKL_Link *link)
{
    if (link == NULL) return;
    link->rx_len = 0u;
    link->rx_expected_len = 0u;
    link->last_rx_byte_ms = 0u;

    /* CRITICAL FIX: Clear RX buffer to prevent old data interference */
    memset(link->rx_buf, 0, sizeof(link->rx_buf));
}

static void gkl_fail(GKL_Link *link, GKL_Result err)
{
    if (link == NULL) return;
    link->last_error = err;
    if (link->consecutive_fail < 255u) link->consecutive_fail++;
    link->state = GKL_STATE_ERROR;
    gkl_rx_reset(link);
}

static void gkl_success(GKL_Link *link)
{
    if (link == NULL) return;
    link->last_error = GKL_OK;
    link->consecutive_fail = 0u;
}

static void gkl_try_finalize_frame_if_complete(GKL_Link *link)
{
    if (link == NULL) return;

    /* If expected length unknown -> can't finalize here */
    if (link->rx_expected_len == 0u) return;
    if (link->rx_len < link->rx_expected_len) return;

    uint8_t len = link->rx_expected_len;

    if (link->rx_buf[0] != GKL_STX)
    {
        gkl_fail(link, GKL_ERR_FORMAT);
        return;
    }

    /* Validate checksum */
    uint8_t calc = gkl_checksum_xor(link->rx_buf, len);
    uint8_t recv = link->rx_buf[len - 1u];
    if (calc != recv)
    {
        gkl_fail(link, GKL_ERR_CRC);
        return;
    }

    /* Validate response command (if set) */
    if (link->expected_resp_cmd != 0 && (char)link->rx_buf[3] != link->expected_resp_cmd)
    {
        gkl_fail(link, GKL_ERR_FORMAT);
        return;
    }

    /* Fill response */
    link->last_resp.ctrl = link->rx_buf[1];
    link->last_resp.slave = link->rx_buf[2];
    link->last_resp.cmd = (char)link->rx_buf[3];
    link->last_resp.checksum = recv;

    uint8_t data_len = (uint8_t)(len - (1u + 2u + 1u + 1u));
    link->last_resp.data_len = data_len;
    if (data_len > 0u)
    {
        memcpy(link->last_resp.data, &link->rx_buf[4], data_len);
    }

    link->resp_ready = 1u;
    link->state = GKL_STATE_GOT_RESP;

    link->rx_total_frames++;

    gkl_success(link);
    gkl_rx_reset(link);
}

/* ===================== Public API ===================== */

void GKL_Init(GKL_Link *link, UART_HandleTypeDef *huart)
{
    if (link == NULL) return;

    memset(link, 0, sizeof(*link));
    link->huart = huart;
    link->state = GKL_STATE_IDLE;
    link->last_error = GKL_OK;
    link->consecutive_fail = 0u;
    link->resp_ready = 0u;
    link->expected_resp_cmd = 0;

    /* Raw RX log ring init */
    link->raw_rx_head = 0u;
    link->raw_rx_tail = 0u;
    link->raw_rx_overflow = 0u;

    /* UART error diagnostics */
    link->last_uart_error = 0u;
    link->uart_error_pending = 0u;

    /* RX diagnostics */
    link->rx_seen_since_tx = 0u;
    link->last_rx_byte = 0u;
    link->rx_total_bytes = 0u;
    link->rx_total_frames = 0u;

    gkl_rx_reset(link);

    gkl_register_link(link);

    if (link->huart != NULL)
    {
        /* Start 1-byte RX interrupt stream (non-blocking, no DMA ring) */
        (void)HAL_UART_Receive_IT(link->huart, (uint8_t*)&link->rx_byte, 1u);
    }
}

GKL_Result GKL_BuildFrame(uint8_t ctrl,
                          uint8_t slave,
                          char cmd,
                          const uint8_t *data,
                          uint8_t data_len,
                          uint8_t *out_bytes,
                          uint8_t *out_len)
{
    if (out_bytes == NULL || out_len == NULL) return GKL_ERR_PARAM;
    if (data_len > GKL_MAX_DATA_LEN) return GKL_ERR_PARAM;

    uint8_t idx = 0u;
    out_bytes[idx++] = GKL_STX;
    out_bytes[idx++] = ctrl;
    out_bytes[idx++] = slave;
    out_bytes[idx++] = (uint8_t)cmd;

    if (data_len > 0u)
    {
        if (data == NULL) return GKL_ERR_PARAM;
        memcpy(&out_bytes[idx], data, data_len);
        idx = (uint8_t)(idx + data_len);
    }

    /* checksum placeholder */
    out_bytes[idx++] = 0u;
    out_bytes[idx - 1u] = gkl_checksum_xor(out_bytes, idx);

    *out_len = idx;
    return GKL_OK;
}

GKL_Result GKL_Send(GKL_Link *link,
                    uint8_t ctrl,
                    uint8_t slave,
                    char cmd,
                    const uint8_t *data,
                    uint8_t data_len,
                    char expected_resp_cmd)
{
    if (link == NULL || link->huart == NULL) return GKL_ERR_PARAM;
    if (data_len > GKL_MAX_DATA_LEN) return GKL_ERR_PARAM;

    /* Only one in-flight request per link */
    if (link->state != GKL_STATE_IDLE && link->state != GKL_STATE_GOT_RESP && link->state != GKL_STATE_ERROR)
    {
        return GKL_ERR_BUSY;
    }

    /* Clear previous response */
    link->resp_ready = 0u;
    memset(&link->last_resp, 0, sizeof(link->last_resp));

    /* Reset RX buffer/timestamps for this exchange (IMPORTANT: do it BEFORE setting rx_expected_len) */
    gkl_rx_reset(link);

    /* Reset RX diagnostics for this exchange */
    link->rx_seen_since_tx = 0u;
    link->last_rx_byte = 0u;
    link->last_uart_error = 0u;

    /* Store expected response command and pre-calc expected response length if known */
    link->expected_resp_cmd = expected_resp_cmd;

    uint8_t resp_data_len = gkl_resp_data_len_for_cmd(expected_resp_cmd);
    if (resp_data_len != 0xFFu)
    {
        link->rx_expected_len = (uint8_t)(1u + 2u + 1u + resp_data_len + 1u);
    }
    else
    {
        link->rx_expected_len = 0u;
    }

    /* Build TX frame */
    uint8_t out_len = 0u;
    GKL_Result br = GKL_BuildFrame(ctrl, slave, cmd, data, data_len, link->tx_buf, &out_len);
    if (br != GKL_OK) return br;
    link->tx_len = out_len;

    /* Clean DCache before DMA reads tx_buf */
    dcache_clean_by_addr(link->tx_buf, link->tx_len);

    if (HAL_UART_Transmit_DMA(link->huart, (uint8_t*)link->tx_buf, link->tx_len) != HAL_OK)
    {
        link->last_error = GKL_ERR_UART;
        if (link->consecutive_fail < 255u) link->consecutive_fail++;
        return GKL_ERR_UART;
    }

    link->state = GKL_STATE_TX_DMA;
    return GKL_OK;
}


bool GKL_HasResponse(GKL_Link *link)
{
    if (link == NULL) return false;
    return (link->resp_ready != 0u);
}

bool GKL_GetResponse(GKL_Link *link, GKL_Frame *out)
{
    if (link == NULL || out == NULL) return false;
    if (link->resp_ready == 0u) return false;

    __disable_irq();
    *out = link->last_resp;
    link->resp_ready = 0u;
    link->state = GKL_STATE_IDLE;
    __enable_irq();

    return true;
}

GKL_Stats GKL_GetStats(GKL_Link *link)
{
    GKL_Stats st;
    st.consecutive_fail = 0u;
    st.last_error = GKL_ERR_PARAM;
    st.state = GKL_STATE_ERROR;

    st.rx_seen_since_tx = 0u;
    st.last_rx_byte = 0u;
    st.rx_len = 0u;
    st.rx_total_bytes = 0u;
    st.rx_total_frames = 0u;

    if (link == NULL)
    {
        return st;
    }

    st.consecutive_fail = link->consecutive_fail;
    st.last_error = link->last_error;
    st.state = link->state;

    st.rx_seen_since_tx = link->rx_seen_since_tx;
    st.last_rx_byte = link->last_rx_byte;
    st.rx_len = link->rx_len;
    st.rx_total_bytes = link->rx_total_bytes;
    st.rx_total_frames = link->rx_total_frames;
    return st;
}

void GKL_Task(GKL_Link *link)
{
    if (link == NULL || link->huart == NULL) return;
    uint32_t now = HAL_GetTick();

    /* Inter-byte timeout (tif) */
    if (link->rx_len > 0u)
    {
        if ((now - link->last_rx_byte_ms) > (uint32_t)GKL_INTERBYTE_TIMEOUT_MS)
        {
            gkl_rx_reset(link);
        }
    }

    /* Response timeout (ts) */
    if (link->state == GKL_STATE_WAIT_RESP)
    {
        if ((now - link->tx_done_ms) > (uint32_t)GKL_RESP_TIMEOUT_MS)
        {
            gkl_fail(link, GKL_ERR_TIMEOUT);
        }
    }

    /* Auto-clear error to avoid blocking application */
    if (link->state == GKL_STATE_ERROR)
    {
        link->state = GKL_STATE_IDLE;
    }
}

/* ===================== HAL callback dispatcher ===================== */

void GKL_Global_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    GKL_Link *link = gkl_find_by_huart(huart);
    if (link == NULL) return;

    link->tx_done_ms = HAL_GetTick();
    link->state = GKL_STATE_WAIT_RESP;
}

void GKL_Global_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    GKL_Link *link = gkl_find_by_huart(huart);
    if (link == NULL) return;

    uint32_t now = HAL_GetTick();
    uint8_t b = link->rx_byte;

    /* Log EVERY received byte (even garbage/out-of-frame) */
    gkl_raw_rx_push(link, b);

    link->last_rx_byte_ms = now;

    /* RX diagnostics */
    link->rx_seen_since_tx = 1u;
    link->last_rx_byte = b;
    link->rx_total_bytes++;

    if (link->rx_len == 0u)
    {
        /* Wait for STX */
        if (b != GKL_STX)
        {
            goto rearm;
        }
    }

    if (link->rx_len < GKL_MAX_FRAME_LEN)
    {
        link->rx_buf[link->rx_len++] = b;
    }
    else
    {
        gkl_fail(link, GKL_ERR_FORMAT);
        goto rearm;
    }

    gkl_try_finalize_frame_if_complete(link);

rearm:
    (void)HAL_UART_Receive_IT(link->huart, (uint8_t*)&link->rx_byte, 1u);
}

void GKL_Global_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    GKL_Link *link = gkl_find_by_huart(huart);
    if (link == NULL) return;

    /* Save error code for diagnostics (printed from main loop) */
    link->last_uart_error = huart->ErrorCode;
    link->uart_error_pending = 1u;

    /* Try to grab a byte if it is sitting in RDR (framing/parity error cases) */
    if ((huart->Instance->ISR & USART_ISR_RXNE_RXFNE) != 0u)
    {
        uint8_t b = (uint8_t)(huart->Instance->RDR & 0xFFu);
        gkl_raw_rx_push(link, b);
    }

    /* Clear UART error flags and restart RX */
    __HAL_UART_CLEAR_OREFLAG(huart);
    __HAL_UART_CLEAR_FEFLAG(huart);
    __HAL_UART_CLEAR_NEFLAG(huart);
    __HAL_UART_CLEAR_PEFLAG(huart);
    huart->ErrorCode = HAL_UART_ERROR_NONE;

    link->last_error = GKL_ERR_UART;
    if (link->consecutive_fail < 255u) link->consecutive_fail++;
    gkl_rx_reset(link);

    (void)HAL_UART_AbortReceive_IT(huart);
    (void)HAL_UART_Receive_IT(link->huart, (uint8_t*)&link->rx_byte, 1u);
}

/* ===================== Debug/diagnostics helpers ===================== */

uint16_t GKL_RawRxDrain(GKL_Link *link, uint8_t *out, uint16_t max_len)
{
    if (link == NULL || out == NULL || max_len == 0u) return 0u;
    uint16_t n = 0u;
    uint16_t tail = (uint16_t)link->raw_rx_tail;
    uint16_t head = (uint16_t)link->raw_rx_head;

    while ((tail != head) && (n < max_len))
    {
        out[n++] = link->raw_rx_log[tail];
        tail++;
        if (tail >= (uint16_t)GKL_RAW_RX_LOG_SIZE) tail = 0u;
    }

    link->raw_rx_tail = tail;
    return n;
}

bool GKL_GetAndClearUartError(GKL_Link *link, uint32_t *out_error)
{
    if (link == NULL || out_error == NULL) return false;
    if (link->uart_error_pending == 0u) return false;

    *out_error = link->last_uart_error;
    link->uart_error_pending = 0u;
    return true;
}
