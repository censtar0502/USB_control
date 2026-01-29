/* transaction_fsm.h - Transaction State Machine */
#ifndef TRANSACTION_FSM_H
#define TRANSACTION_FSM_H

#include <stdint.h>
#include <stdbool.h>
#include "pump_mgr.h"
#include "pump_proto_gkl.h"

typedef enum {
    TRX_IDLE = 0,
    TRX_PRESET_SENT,
    TRX_ARMED,
    TRX_DISPENSING,
    TRX_PAUSED,
    TRX_COMPLETE,
    TRX_CLOSING
} TrxState;

typedef struct {
    uint8_t pump_id;
    PumpMgr *mgr;
    PumpProtoGKL *gkl;
    
    TrxState state;
    uint32_t preset_volume_dL;
    uint32_t preset_money;
    uint32_t rt_volume_dL;
    uint32_t rt_money;
    uint32_t totalizer_dL;
    uint32_t last_poll_ms;
    
} TransactionFSM;

void TrxFSM_Init(TransactionFSM *fsm, uint8_t pump_id, PumpMgr *mgr, PumpProtoGKL *gkl);
void TrxFSM_Task(TransactionFSM *fsm);
bool TrxFSM_StartVolume(TransactionFSM *fsm, uint32_t volume_dL);
bool TrxFSM_StartMoney(TransactionFSM *fsm, uint32_t money);
bool TrxFSM_Pause(TransactionFSM *fsm);
bool TrxFSM_Resume(TransactionFSM *fsm);
bool TrxFSM_Cancel(TransactionFSM *fsm);
TrxState TrxFSM_GetState(TransactionFSM *fsm);
uint32_t TrxFSM_GetRealtimeVolume(TransactionFSM *fsm);
uint32_t TrxFSM_GetRealtimeMoney(TransactionFSM *fsm);

#endif
