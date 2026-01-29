/* pump_transactions.h - Refactored with proper logging */
#ifndef PUMP_TRANSACTIONS_H
#define PUMP_TRANSACTIONS_H

#include <stdint.h>
#include <stdbool.h>
#include "pump_proto_gkl.h"

/* Transaction commands with proper logging */
bool PumpTrans_PresetVolume(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave,
                            uint8_t nozzle, uint32_t volume_dL, uint16_t price);

bool PumpTrans_PresetMoney(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave,
                           uint8_t nozzle, uint32_t money, uint16_t price);

bool PumpTrans_Stop(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave);

bool PumpTrans_Resume(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave);

bool PumpTrans_End(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave);

bool PumpTrans_PollRealtimeVolume(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

bool PumpTrans_PollRealtimeMoney(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

bool PumpTrans_ReadTotalizer(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

bool PumpTrans_ReadTransaction(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave);

#endif
