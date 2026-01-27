/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_mgr_transactions.c
  * @brief   Pump Manager Transaction API implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "pump_mgr_transactions.h"
#include "pump_proto.h"

/* ============================================================================
 * Transaction Preset API
 * ========================================================================= */

/**
 * @brief Preset volume for dispensing
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @param nozzle: Nozzle number (1-9)
 * @param volume_dL: Volume in deciliters (1L = 10dL)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_PresetVolume(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle, uint32_t volume_dL)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    /* Store preset parameters */
    dev->preset_volume_dL = volume_dL;
    dev->preset_nozzle = nozzle;
    
    /* Send preset command with current price */
    return PumpProto_PresetVolume(dev->proto, dev->ctrl_addr, dev->slave_addr,
                                  nozzle, volume_dL, (uint16_t)dev->price);
}

/**
 * @brief Preset money for dispensing
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @param nozzle: Nozzle number (1-9)
 * @param money: Money amount in smallest units
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_PresetMoney(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle, uint32_t money)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    /* Store preset parameters */
    dev->preset_money = money;
    dev->preset_nozzle = nozzle;
    
    /* Send preset command with current price */
    return PumpProto_PresetMoney(dev->proto, dev->ctrl_addr, dev->slave_addr,
                                 nozzle, money, (uint16_t)dev->price);
}

/* ============================================================================
 * Transaction Control API
 * ========================================================================= */

/**
 * @brief Stop (pause) current dispensing
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_Stop(PumpMgr *mgr, uint8_t pump_id)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    return PumpProto_Stop(dev->proto, dev->ctrl_addr, dev->slave_addr);
}

/**
 * @brief Resume paused dispensing
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_Resume(PumpMgr *mgr, uint8_t pump_id)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    return PumpProto_Resume(dev->proto, dev->ctrl_addr, dev->slave_addr);
}

/**
 * @brief End current transaction
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_End(PumpMgr *mgr, uint8_t pump_id)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    return PumpProto_End(dev->proto, dev->ctrl_addr, dev->slave_addr);
}

/* ============================================================================
 * Realtime Polling API
 * ========================================================================= */

/**
 * @brief Poll realtime volume during dispensing
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @param nozzle: Nozzle number (1-9)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_PollRealtimeVolume(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    return PumpProto_PollRealtimeVolume(dev->proto, dev->ctrl_addr, dev->slave_addr, nozzle);
}

/**
 * @brief Poll realtime money during dispensing
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @param nozzle: Nozzle number (1-9)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_PollRealtimeMoney(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    return PumpProto_PollRealtimeMoney(dev->proto, dev->ctrl_addr, dev->slave_addr, nozzle);
}

/* ============================================================================
 * Data Reading API
 * ========================================================================= */

/**
 * @brief Read totalizer value
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @param index: Totalizer index (0-7)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_ReadTotalizer(PumpMgr *mgr, uint8_t pump_id, uint8_t index)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    return PumpProto_ReadTotalizer(dev->proto, dev->ctrl_addr, dev->slave_addr, index);
}

/**
 * @brief Read last transaction data
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @return PumpProtoResult
 */
PumpProtoResult PumpMgr_ReadTransaction(PumpMgr *mgr, uint8_t pump_id)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return PUMP_PROTO_ERR;
    
    PumpDevice *dev = &mgr->dev[pump_id - 1u];
    if (dev->proto == NULL) return PUMP_PROTO_ERR;
    
    return PumpProto_ReadTransaction(dev->proto, dev->ctrl_addr, dev->slave_addr);
}

/* ============================================================================
 * Getters for Cached Data
 * ========================================================================= */

/**
 * @brief Get cached realtime volume
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @return Volume in dL, or 0 if no data
 */
uint32_t PumpMgr_GetRealtimeVolume(const PumpMgr *mgr, uint8_t pump_id)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return 0;
    return mgr->dev[pump_id - 1u].last_rt_volume_dL;
}

/**
 * @brief Get cached realtime money
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @return Money amount, or 0 if no data
 */
uint32_t PumpMgr_GetRealtimeMoney(const PumpMgr *mgr, uint8_t pump_id)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return 0;
    return mgr->dev[pump_id - 1u].last_rt_money;
}

/**
 * @brief Get cached totalizer value
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @param index: Totalizer index (0-7)
 * @return Totalizer value in dL, or 0 if no data
 */
uint32_t PumpMgr_GetTotalizer(const PumpMgr *mgr, uint8_t pump_id, uint8_t index)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES || index >= 8) return 0;
    return mgr->dev[pump_id - 1u].totalizer[index];
}

/**
 * @brief Get last completed transaction data
 * @param mgr: Pump manager context
 * @param pump_id: Pump ID (1..N)
 * @param volume_dL: Output pointer for volume (can be NULL)
 * @param money: Output pointer for money (can be NULL)
 * @param price: Output pointer for price (can be NULL)
 */
void PumpMgr_GetLastTransaction(const PumpMgr *mgr, uint8_t pump_id,
                                uint32_t *volume_dL, uint32_t *money, uint16_t *price)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MAX_DEVICES) return;
    
    const PumpDevice *dev = &mgr->dev[pump_id - 1u];
    
    if (volume_dL) *volume_dL = dev->last_trx_volume_dL;
    if (money) *money = dev->last_trx_money;
    if (price) *price = dev->last_trx_price;
}
