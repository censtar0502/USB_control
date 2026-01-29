/* transaction_fsm.h */
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

    /* Cached values for UI */
    uint32_t rt_volume_dL;
    uint32_t rt_money;
    uint32_t totalizer_dL;

    uint32_t last_poll_ms;

    /*
     * Realtime polling state machine:
     *  poll_step:
     *   0 - wait for next SR update, then send LM
     *   1 - waiting L response (volume), then send RS
     *   2 - waiting R response (money), then finish cycle
     */
    uint8_t  poll_step;

    uint32_t cycle_status_ms;
    uint8_t  wait_vol_seq;
    uint8_t  wait_money_seq;

    /* 1 if TU (T request) command sent; we wait for TRX_FINAL event */
    uint8_t  final_data_requested;
    uint8_t  wait_trx_seq;

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
