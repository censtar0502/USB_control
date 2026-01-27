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
#include "stm32h7xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

#ifndef PUMP_MAX_DEVICES
#define PUMP_MAX_DEVICES   (8u)
#endif

typedef struct
{
    uint8_t id;

    /* Which protocol implementation serves this pump (UART/driver etc.) */
    PumpProto *proto;

    /* Addressing */
    uint8_t ctrl_addr;
    uint8_t slave_addr;

    /* User-configurable */
    uint32_t price; /* application units (define later), stored per pump */

    /* Cached live state */
    uint8_t status;         /* normalized numeric status */
    uint8_t nozzle;         /* selected nozzle */
    uint32_t last_status_ms;

    /* Cached errors */
    uint8_t last_error;
    uint8_t fail_count;
} PumpDevice;

typedef struct
{
    PumpDevice dev[PUMP_MAX_DEVICES];
    uint8_t count;

    uint32_t poll_period_ms;
    uint32_t next_poll_ms[PUMP_MAX_DEVICES];
} PumpMgr;

void PumpMgr_Init(PumpMgr *m, uint32_t poll_period_ms);
bool PumpMgr_Add(PumpMgr *m, uint8_t id, PumpProto *proto, uint8_t ctrl_addr, uint8_t slave_addr);

void PumpMgr_Task(PumpMgr *m);

PumpDevice *PumpMgr_Get(PumpMgr *m, uint8_t id);
const PumpDevice *PumpMgr_GetConst(const PumpMgr *m, uint8_t id);

bool PumpMgr_SetPrice(PumpMgr *m, uint8_t id, uint32_t price);
uint32_t PumpMgr_GetPrice(const PumpMgr *m, uint8_t id);

bool PumpMgr_SetSlaveAddr(PumpMgr *m, uint8_t id, uint8_t slave_addr);
uint8_t PumpMgr_GetSlaveAddr(const PumpMgr *m, uint8_t id);

bool PumpMgr_SetCtrlAddr(PumpMgr *m, uint8_t id, uint8_t ctrl_addr);
uint8_t PumpMgr_GetCtrlAddr(const PumpMgr *m, uint8_t id);

/* --------- Utility helpers (still protocol-agnostic) ---------
   These are intended for UI/service actions.
   They do NOT touch the underlying protocol state, only manager cached fields
   and poll scheduling.
*/

/**
 * @brief Clear cached error and fail counter for a pump.
 */
void PumpMgr_ClearFail(PumpMgr *m, uint8_t id);

/**
 * @brief Request an immediate poll of the given pump (on next PumpMgr_Task).
 */
void PumpMgr_RequestPollNow(PumpMgr *m, uint8_t id);

/**
 * @brief Request an immediate poll of all pumps (on next PumpMgr_Task).
 */
void PumpMgr_RequestPollAllNow(PumpMgr *m);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_MGR_H */
