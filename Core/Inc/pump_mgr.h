/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_mgr.h
  * @brief   Protocol-agnostic pump device manager (polling, cached state)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef PUMP_MGR_H
#define PUMP_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pump_proto.h"

#ifndef PUMP_MGR_MAX_PUMPS
#define PUMP_MGR_MAX_PUMPS   (4u)

/* Faster SR polling when transaction is active */
#define PUMP_MGR_ACTIVE_POLL_MS 30u
#endif

typedef struct
{
    uint8_t      id;
    PumpProto    proto;

    uint8_t      ctrl_addr;
    uint8_t      slave_addr;

    uint32_t     price;

    uint8_t      status;
    uint8_t      nozzle;

    uint32_t     last_status_ms;

    /* Cached realtime values (updated by PumpProtoGKL via events) */
    uint32_t     rt_volume_dL;     /* 0.1L units */
    uint32_t     rt_money;         /* protocol units */
    uint8_t      rt_vol_seq;
    uint8_t      rt_money_seq;

    /* Cached totalizer */
    uint32_t     totalizer_dL;     /* 0.1L units */
    uint8_t      totalizer_nozzle; /* nozzle index from C response */
    uint8_t      totalizer_seq;

    /* Cached final transaction (T response) */
    uint32_t     trx_money;
    uint32_t     trx_volume_dL;    /* 0.1L units */
    uint16_t     trx_price;
    uint8_t      trx_nozzle;
    uint8_t      trx_final_seq;

    uint8_t      last_error;
    uint8_t      fail_count;
} PumpDevice;

typedef struct
{
    PumpDevice  pumps[PUMP_MGR_MAX_PUMPS];
    uint8_t     count;

    uint32_t    poll_period_ms;
    uint32_t    next_poll_ms[PUMP_MGR_MAX_PUMPS];
} PumpMgr;

void PumpMgr_Init(PumpMgr *m, uint32_t poll_period_ms);
bool PumpMgr_Add(PumpMgr *m, uint8_t id, PumpProto *proto, uint8_t ctrl_addr, uint8_t slave_addr);

PumpDevice *PumpMgr_Get(PumpMgr *m, uint8_t id);
const PumpDevice *PumpMgr_GetConst(const PumpMgr *m, uint8_t id);

bool PumpMgr_SetPrice(PumpMgr *m, uint8_t id, uint32_t price);
uint32_t PumpMgr_GetPrice(const PumpMgr *m, uint8_t id);

bool PumpMgr_SetSlaveAddr(PumpMgr *m, uint8_t id, uint8_t slave_addr);
uint8_t PumpMgr_GetSlaveAddr(const PumpMgr *m, uint8_t id);

bool PumpMgr_SetCtrlAddr(PumpMgr *m, uint8_t id, uint8_t ctrl_addr);
uint8_t PumpMgr_GetCtrlAddr(const PumpMgr *m, uint8_t id);

void PumpMgr_ClearFail(PumpMgr *m, uint8_t id);
void PumpMgr_RequestPollNow(PumpMgr *m, uint8_t id);
void PumpMgr_RequestPollAllNow(PumpMgr *m);

PumpProtoResult PumpMgr_RequestTotalizer(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle);
bool PumpMgr_PopEvent(PumpMgr *m, PumpEvent *out);
void PumpMgr_Task(PumpMgr *m);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_MGR_H */
