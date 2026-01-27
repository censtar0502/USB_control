/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_mgr_transactions.h
  * @brief   Pump Manager Transaction API module
  * 
  * High-level API for transaction management (preset, start, stop, resume)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef PUMP_MGR_TRANSACTIONS_H
#define PUMP_MGR_TRANSACTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "pump_mgr.h"

/**
 * @brief Transaction Control API
 * 
 * These functions provide high-level transaction control for pumps.
 * All functions return PumpProtoResult (OK/BUSY/ERR).
 */

/* Transaction preset */
PumpProtoResult PumpMgr_PresetVolume(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle, uint32_t volume_dL);
PumpProtoResult PumpMgr_PresetMoney(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle, uint32_t money);

/* Transaction control */
PumpProtoResult PumpMgr_Stop(PumpMgr *mgr, uint8_t pump_id);
PumpProtoResult PumpMgr_Resume(PumpMgr *mgr, uint8_t pump_id);
PumpProtoResult PumpMgr_End(PumpMgr *mgr, uint8_t pump_id);

/* Realtime polling */
PumpProtoResult PumpMgr_PollRealtimeVolume(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle);
PumpProtoResult PumpMgr_PollRealtimeMoney(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle);

/* Data reading */
PumpProtoResult PumpMgr_ReadTotalizer(PumpMgr *mgr, uint8_t pump_id, uint8_t index);
PumpProtoResult PumpMgr_ReadTransaction(PumpMgr *mgr, uint8_t pump_id);

/**
 * @brief Getters for realtime data
 * 
 * These functions return cached realtime data from last poll.
 * Returns 0 if no data available or invalid pump_id.
 */
uint32_t PumpMgr_GetRealtimeVolume(const PumpMgr *mgr, uint8_t pump_id);
uint32_t PumpMgr_GetRealtimeMoney(const PumpMgr *mgr, uint8_t pump_id);
uint32_t PumpMgr_GetTotalizer(const PumpMgr *mgr, uint8_t pump_id, uint8_t index);

/**
 * @brief Get last completed transaction data
 */
void PumpMgr_GetLastTransaction(const PumpMgr *mgr, uint8_t pump_id,
                                uint32_t *volume_dL, uint32_t *money, uint16_t *price);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_MGR_TRANSACTIONS_H */
