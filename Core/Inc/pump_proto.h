/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_proto.h
  * @brief   Protocol-agnostic interface between business FSM and a pump protocol
  *
  * The FSM/UI should not know about concrete protocol frames (GasKitLink, etc.).
  * It talks to PumpProto through this interface.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef PUMP_PROTO_H
#define PUMP_PROTO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h> /* NULL */

typedef enum
{
    PUMP_PROTO_OK = 0,
    PUMP_PROTO_BUSY,
    PUMP_PROTO_ERR
} PumpProtoResult;

typedef enum
{
    PUMP_EVT_NONE = 0,
    PUMP_EVT_STATUS,
    PUMP_EVT_ERROR,
    PUMP_EVT_TOTALIZER  /* Новое событие - данные тоталайзера */
} PumpEventType;

typedef struct
{
    PumpEventType type;

    /* Addressing (protocol-agnostic fields) */
    uint8_t ctrl_addr;
    uint8_t slave_addr;

    /* Status (when type == PUMP_EVT_STATUS) */
    uint8_t status;      /* 0..9 (normalized numeric), meaning is protocol-specific mapping */
    uint8_t nozzle;      /* 0..n */

    /* Error (when type == PUMP_EVT_ERROR) */
    uint8_t error_code;  /* protocol-specific numeric error */
    uint8_t fail_count;  /* consecutive failures seen by link */

    /* Totalizer data (when type == PUMP_EVT_TOTALIZER) */
    uint8_t nozzle_idx;  /* Номер форсунки 1-6 */
    uint32_t totalizer;  /* Значение тоталайзера в сантилитрах */
} PumpEvent;

typedef struct PumpProtoVTable
{
    void (*task)(void *ctx);
    bool (*is_idle)(void *ctx);

    PumpProtoResult (*send_poll_status)(void *ctx, uint8_t ctrl_addr, uint8_t slave_addr);
    PumpProtoResult (*request_totalizer)(void *ctx, uint8_t ctrl_addr, uint8_t slave_addr, uint8_t nozzle);

    bool (*pop_event)(void *ctx, PumpEvent *out);
} PumpProtoVTable;

typedef struct
{
    const PumpProtoVTable *vt;
    void *ctx;
} PumpProto;

static inline void PumpProto_Task(PumpProto *p)
{
    if (p && p->vt && p->vt->task) p->vt->task(p->ctx);
}

static inline bool PumpProto_IsIdle(PumpProto *p)
{
    if (p && p->vt && p->vt->is_idle) return p->vt->is_idle(p->ctx);
    return false;
}

static inline PumpProtoResult PumpProto_PollStatus(PumpProto *p, uint8_t ctrl, uint8_t slave)
{
    if (p == NULL || p->vt == NULL || p->vt->send_poll_status == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_poll_status(p->ctx, ctrl, slave);
}

static inline PumpProtoResult PumpProto_RequestTotalizer(PumpProto *p, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (p == NULL || p->vt == NULL || p->vt->request_totalizer == NULL) return PUMP_PROTO_ERR;
    return p->vt->request_totalizer(p->ctx, ctrl, slave, nozzle);
}

static inline bool PumpProto_PopEvent(PumpProto *p, PumpEvent *out)
{
    if (p && p->vt && p->vt->pop_event) return p->vt->pop_event(p->ctx, out);
    return false;
}

#ifdef __cplusplus
}
#endif

#endif /* PUMP_PROTO_H */
