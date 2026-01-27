/* pump_transactions.h - Direct GKL transaction support */
#ifndef PUMP_TRANSACTIONS_H
#define PUMP_TRANSACTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "gkl_link.h"

/* Transaction commands - Direct GKL access */
bool PumpTrans_PresetVolume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, 
                            uint8_t nozzle, uint32_t volume_dL, uint16_t price);

bool PumpTrans_PresetMoney(GKL_Link *gkl, uint8_t ctrl, uint8_t slave,
                           uint8_t nozzle, uint32_t money, uint16_t price);

bool PumpTrans_Stop(GKL_Link *gkl, uint8_t ctrl, uint8_t slave);

bool PumpTrans_Resume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave);

bool PumpTrans_End(GKL_Link *gkl, uint8_t ctrl, uint8_t slave);

bool PumpTrans_PollRealtimeVolume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

bool PumpTrans_PollRealtimeMoney(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

bool PumpTrans_ReadTotalizer(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

bool PumpTrans_ReadTransaction(GKL_Link *gkl, uint8_t ctrl, uint8_t slave);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_TRANSACTIONS_H */
