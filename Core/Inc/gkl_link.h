/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gkl_link.h
  * @brief   Non-blocking GasKitLink (CENSTAR) master-side datalink/application framing
  *
  * Key goals:
  * - No CPU blocking for protocol traffic
  * - Multiple UART instances supported (USART2/USART3/...)
  * - One request in-flight per UART link
  *
  * Frame format (per spec):
  *   <0x02><ctrl><slave><cmd><data...><xor_checksum>
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef GKL_LINK_H
#define GKL_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#define GKL_STX                        (0x02u)

/* Datalink limits from spec: data words up to 22 bytes */
#define GKL_MAX_DATA_LEN               (22u)
#define GKL_MAX_FRAME_LEN              (1u + 2u + 1u + GKL_MAX_DATA_LEN + 1u) /* 27 bytes */

#define GKL_INTERBYTE_TIMEOUT_MS       (10u)   /* tif */
#define GKL_RESP_TIMEOUT_MS            (200u)  /* ts */
#define GKL_RESP_DELAY_MIN_MS          (3u)   /* td (slave delays response) */

/* How many UART links we can register in the global callbacks dispatcher */
#ifndef GKL_MAX_LINKS
#define GKL_MAX_LINKS                  (4u)
#endif

/* Raw RX logging buffer size (debug).
 * Stores EVERY received byte (even garbage/out-of-frame), drained from main loop.
 */
#ifndef GKL_RAW_RX_LOG_SIZE
#define GKL_RAW_RX_LOG_SIZE            (512u)
#endif

typedef enum
{
    GKL_OK = 0,
    GKL_BUSY,              /* Added for transaction API */
    GKL_ERR,               /* Added for transaction API */
    GKL_ERR_BUSY,
    GKL_ERR_PARAM,
    GKL_ERR_TIMEOUT,
    GKL_ERR_CRC,
    GKL_ERR_FORMAT,
    GKL_ERR_UART
} GKL_Result;

typedef enum
{
    GKL_STATE_IDLE = 0,
    GKL_STATE_TX_DMA,
    GKL_STATE_WAIT_RESP,
    GKL_STATE_GOT_RESP,
    GKL_STATE_ERROR
} GKL_State;

typedef struct
{
    uint8_t ctrl;
    uint8_t slave;
    char    cmd;                         /* command word (ASCII) */
    uint8_t data[GKL_MAX_DATA_LEN];
    uint8_t data_len;                    /* 0..22 */
    uint8_t checksum;                    /* XOR checksum */
} GKL_Frame;

typedef struct
{
    /* Connection health: consecutive failed exchanges (timeout/crc/format/uart) */
    uint8_t consecutive_fail;
    /* Last error */
    GKL_Result last_error;
    /* Driver state */
    GKL_State state;

    /* RX diagnostics (helps distinguish wiring/noise vs parser issues) */
    uint8_t  rx_seen_since_tx;          /* 0 = no bytes received after last request */
    uint8_t  last_rx_byte;              /* last received byte value (raw) */
    uint8_t  rx_len;                    /* current rx buffer length */
    uint32_t rx_total_bytes;            /* lifetime counter */
    uint32_t rx_total_frames;           /* lifetime parsed frames */
} GKL_Stats;

typedef struct
{
    UART_HandleTypeDef *huart;

    volatile GKL_State  state;
    volatile GKL_Result last_error;
    volatile uint8_t    consecutive_fail;

    /* TX */
    uint8_t  tx_buf[GKL_MAX_FRAME_LEN] __attribute__((aligned(32)));
    uint8_t  tx_len;

    /* RX byte-by-byte */
    uint8_t  rx_byte;
    uint8_t  rx_buf[GKL_MAX_FRAME_LEN];
    uint8_t  rx_expected_len;            /* 0 = unknown */
    volatile uint8_t rx_len;

    /* RX diagnostics */
    volatile uint8_t rx_seen_since_tx;   /* set to 1 when any byte arrives after TX */
    volatile uint8_t last_rx_byte;       /* last raw byte received */
    volatile uint32_t rx_total_bytes;
    volatile uint32_t rx_total_frames;

    /* Response */
    volatile uint8_t resp_ready;
    GKL_Frame last_resp;

    /* Timing */
    volatile uint32_t tx_done_ms;
    volatile uint32_t last_rx_byte_ms;

    /* Expected response command for current request */
    volatile char expected_resp_cmd;

    /* Raw RX log ring (debug): every received byte is pushed here from IRQ */
    uint8_t raw_rx_log[GKL_RAW_RX_LOG_SIZE];
    volatile uint16_t raw_rx_head;
    volatile uint16_t raw_rx_tail;
    volatile uint8_t raw_rx_overflow;

    /* UART error diagnostics (set in IRQ, reported from main loop) */
    volatile uint32_t last_uart_error;
    volatile uint8_t uart_error_pending;
} GKL_Link;

/* ===================== Public API ===================== */

/**
 * @brief  Bind module to a UART and start RX.
 * @note   Call once after MX_USARTx_UART_Init().
 */
void GKL_Init(GKL_Link *link, UART_HandleTypeDef *huart);

/**
 * @brief  Must be called often from main while(1) (no delays inside).
 *         Handles response timeout and inter-byte timeout.
 */
void GKL_Task(GKL_Link *link);

/**
 * @brief  Send a command (master->slave) and arm ожидание ответа (slave->master).
 */
GKL_Result GKL_Send(GKL_Link *link,
                    uint8_t ctrl,
                    uint8_t slave,
                    char cmd,
                    const uint8_t *data,
                    uint8_t data_len,
                    char expected_resp_cmd);

/**
 * @brief  True if a response frame is ready to be consumed via GKL_GetResponse().
 */
bool GKL_HasResponse(GKL_Link *link);

/**
 * @brief  Get last received response (valid only if GKL_HasResponse()).
 */
bool GKL_GetResponse(GKL_Link *link, GKL_Frame *out);

/**
 * @brief  Get stats (connection, last error, state).
 */
GKL_Stats GKL_GetStats(GKL_Link *link);

/**
 * @brief  Drain raw RX bytes collected from IRQ.
 * @return Number of bytes copied to out[] (0 if none).
 */
uint16_t GKL_RawRxDrain(GKL_Link *link, uint8_t *out, uint16_t max_len);

/**
 * @brief  Get last UART error code (HAL UART ErrorCode) and clear pending flag.
 * @return true if a new error was pending.
 */
bool GKL_GetAndClearUartError(GKL_Link *link, uint32_t *out_error);

/**
 * @brief  Convenience: build frame bytes (mostly for debug).
 */
GKL_Result GKL_BuildFrame(uint8_t ctrl,
                          uint8_t slave,
                          char cmd,
                          const uint8_t *data,
                          uint8_t data_len,
                          uint8_t *out_bytes,
                          uint8_t *out_len);

/* ===================== HAL callbacks bridging (global dispatcher) ===================== */
/* Call these from your HAL_* callbacks in main.c */

void GKL_Global_UART_TxCpltCallback(UART_HandleTypeDef *huart);
void GKL_Global_UART_RxCpltCallback(UART_HandleTypeDef *huart);
void GKL_Global_UART_ErrorCallback(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* GKL_LINK_H */
//test for github
