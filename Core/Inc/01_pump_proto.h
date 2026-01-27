/* Полный файл pump_proto.h - ГОТОВ К COPY-PASTE */
/* Замените весь файл Core/Inc/pump_proto.h этим содержимым */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_proto.h
  * @brief   Protocol-agnostic interface with transaction support
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
#include <stddef.h>

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
    PUMP_EVT_REALTIME_VOLUME,
    PUMP_EVT_REALTIME_MONEY,
    PUMP_EVT_TOTALIZER,
    PUMP_EVT_TRANSACTION
} PumpEventType;

typedef struct
{
    PumpEventType type;
    uint8_t ctrl_addr;
    uint8_t slave_addr;

    union
    {
        struct { uint8_t status; uint8_t nozzle; } st;
        struct { uint8_t error_code; uint8_t fail_count; } err;
        struct { uint32_t value; uint8_t nozzle; } rt_volume;
        struct { uint32_t value; uint8_t nozzle; } rt_money;
        struct { uint8_t index; uint32_t value; } totalizer;
        struct { uint32_t volume; uint32_t money; uint16_t price; } transaction;
    } u;
} PumpEvent;

typedef struct PumpProtoVTable
{
    void (*task)(void *ctx);
    bool (*is_idle)(void *ctx);
    PumpProtoResult (*send_poll_status)(void *ctx, uint8_t ctrl_addr, uint8_t slave_addr);
    PumpProtoResult (*send_preset_volume)(void *ctx, uint8_t ctrl, uint8_t slave, uint8_t nozzle, uint32_t volume_dL, uint16_t price);
    PumpProtoResult (*send_preset_money)(void *ctx, uint8_t ctrl, uint8_t slave, uint8_t nozzle, uint32_t money, uint16_t price);
    PumpProtoResult (*send_stop)(void *ctx, uint8_t ctrl, uint8_t slave);
    PumpProtoResult (*send_resume)(void *ctx, uint8_t ctrl, uint8_t slave);
    PumpProtoResult (*send_end)(void *ctx, uint8_t ctrl, uint8_t slave);
    PumpProtoResult (*send_poll_realtime_volume)(void *ctx, uint8_t ctrl, uint8_t slave, uint8_t nozzle);
    PumpProtoResult (*send_poll_realtime_money)(void *ctx, uint8_t ctrl, uint8_t slave, uint8_t nozzle);
    PumpProtoResult (*send_read_totalizer)(void *ctx, uint8_t ctrl, uint8_t slave, uint8_t index);
    PumpProtoResult (*send_read_transaction)(void *ctx, uint8_t ctrl, uint8_t slave);
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

static inline bool PumpProto_PopEvent(PumpProto *p, PumpEvent *out)
{
    if (p && p->vt && p->vt->pop_event) return p->vt->pop_event(p->ctx, out);
    return false;
}

static inline PumpProtoResult PumpProto_PresetVolume(PumpProto *p, uint8_t ctrl, uint8_t slave, uint8_t nozzle, uint32_t volume_dL, uint16_t price)
{
    if (p == NULL || p->vt == NULL || p->vt->send_preset_volume == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_preset_volume(p->ctx, ctrl, slave, nozzle, volume_dL, price);
}

static inline PumpProtoResult PumpProto_PresetMoney(PumpProto *p, uint8_t ctrl, uint8_t slave, uint8_t nozzle, uint32_t money, uint16_t price)
{
    if (p == NULL || p->vt == NULL || p->vt->send_preset_money == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_preset_money(p->ctx, ctrl, slave, nozzle, money, price);
}

static inline PumpProtoResult PumpProto_Stop(PumpProto *p, uint8_t ctrl, uint8_t slave)
{
    if (p == NULL || p->vt == NULL || p->vt->send_stop == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_stop(p->ctx, ctrl, slave);
}

static inline PumpProtoResult PumpProto_Resume(PumpProto *p, uint8_t ctrl, uint8_t slave)
{
    if (p == NULL || p->vt == NULL || p->vt->send_resume == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_resume(p->ctx, ctrl, slave);
}

static inline PumpProtoResult PumpProto_End(PumpProto *p, uint8_t ctrl, uint8_t slave)
{
    if (p == NULL || p->vt == NULL || p->vt->send_end == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_end(p->ctx, ctrl, slave);
}

static inline PumpProtoResult PumpProto_PollRealtimeVolume(PumpProto *p, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (p == NULL || p->vt == NULL || p->vt->send_poll_realtime_volume == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_poll_realtime_volume(p->ctx, ctrl, slave, nozzle);
}

static inline PumpProtoResult PumpProto_PollRealtimeMoney(PumpProto *p, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (p == NULL || p->vt == NULL || p->vt->send_poll_realtime_money == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_poll_realtime_money(p->ctx, ctrl, slave, nozzle);
}

static inline PumpProtoResult PumpProto_ReadTotalizer(PumpProto *p, uint8_t ctrl, uint8_t slave, uint8_t index)
{
    if (p == NULL || p->vt == NULL || p->vt->send_read_totalizer == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_read_totalizer(p->ctx, ctrl, slave, index);
}

static inline PumpProtoResult PumpProto_ReadTransaction(PumpProto *p, uint8_t ctrl, uint8_t slave)
{
    if (p == NULL || p->vt == NULL || p->vt->send_read_transaction == NULL) return PUMP_PROTO_ERR;
    return p->vt->send_read_transaction(p->ctx, ctrl, slave);
}

#ifdef __cplusplus
}
#endif

#endif /* PUMP_PROTO_H */
