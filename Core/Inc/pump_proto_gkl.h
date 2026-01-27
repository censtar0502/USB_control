/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_proto_gkl.h
  * @brief   GasKitLink (CENSTAR) implementation of PumpProto interface
  *****************************************************************************
  */
/* USER CODE END Header */

#ifndef PUMP_PROTO_GKL_H
#define PUMP_PROTO_GKL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pump_proto.h"
#include "gkl_link.h"

/* Event queue length per UART link */
#ifndef PUMP_GKL_EVTQ_LEN
#define PUMP_GKL_EVTQ_LEN   (8u)
#endif

/* After how many consecutive failed exchanges we consider link disconnected */
#ifndef PUMP_GKL_NO_CONNECT_THRESHOLD
#define PUMP_GKL_NO_CONNECT_THRESHOLD   (10u)
#endif

/* Raw TX/RX frame trace into USB CDC log.
   WARNING: enabling this increases log traffic. */
#ifndef PUMP_GKL_TRACE_FRAMES
#define PUMP_GKL_TRACE_FRAMES   (0u)  /* Отключим verbose логирование по умолчанию */
#endif

/* Select which TRK to log (0=both, 1=TRK1 only, 2=TRK2 only) */
#ifndef PUMP_GKL_LOG_TARGET
#define PUMP_GKL_LOG_TARGET   1  /* Default: log both */
#endif

/* Enable/disable compact log format (like reference log) */
#ifndef PUMP_GKL_COMPACT_LOG
#define PUMP_GKL_COMPACT_LOG   1  /* Default: use compact format */
#endif

typedef struct
{
    /* Underlying non-blocking GasKitLink datalink */
    GKL_Link link;

    /* Optional human-readable tag for logs (e.g. "TRK1") */
    char tag[8];

    /* A small event queue so upper layers can be decoupled */
    PumpEvent q[PUMP_GKL_EVTQ_LEN];
    uint8_t q_head;
    uint8_t q_tail;

    /* In-flight request tracking */
    uint8_t pending;
    uint8_t pending_ctrl;
    uint8_t pending_slave;

    /* Prevent spamming identical error events */
    GKL_Result last_reported_err;
    uint8_t last_reported_failcnt;    /* No-connect log latch */
    uint8_t no_connect_latched;

    /* RX trace bookkeeping (per link) */
    uint32_t last_rx_total_frames;
    uint32_t last_rx_crc_errors;

    /* RX bytes counter at last TX (for timeout diagnostics) */
    uint32_t pending_rx_bytes_start;
} PumpProtoGKL;

/**
 * @brief Initialize GasKitLink PumpProto implementation on given UART.
 */
void PumpProtoGKL_Init(PumpProtoGKL *gkl, UART_HandleTypeDef *huart);

/**
 * @brief Set tag to be printed in logs (optional).
 */
void PumpProtoGKL_SetTag(PumpProtoGKL *gkl, const char *tag);

/**
 * @brief Bind implementation to generic PumpProto handle.
 */
void PumpProtoGKL_Bind(PumpProto *out, PumpProtoGKL *gkl);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_PROTO_GKL_H */
