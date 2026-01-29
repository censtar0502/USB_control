/* transaction_fsm.c - Transaction State Machine Implementation */
#include "transaction_fsm.h"
#include "pump_transactions.h"
#include "pump_response_parser.h"
#include "gkl_link.h"
#include "stm32h7xx_hal.h"
#include <string.h>

void TrxFSM_Init(TransactionFSM *fsm, uint8_t pump_id, PumpMgr *mgr, PumpProtoGKL *gkl)
{
    if (!fsm) return;
    memset(fsm, 0, sizeof(*fsm));
    fsm->pump_id = pump_id;
    fsm->mgr = mgr;
    fsm->gkl = gkl;
    fsm->state = TRX_IDLE;
}

void TrxFSM_Task(TransactionFSM *fsm)
{
    if (!fsm || !fsm->gkl || !fsm->mgr) return;
    
    uint32_t now = HAL_GetTick();
    GKL_Link *gkl = &fsm->gkl->link;
    const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
    if (!dev) return;
    
    /* Process GKL responses */
    if (GKL_HasResponse(gkl)) {
        GKL_Frame resp;
        if (GKL_GetResponse(gkl, &resp)) {
            if (resp.cmd == 'L') {
                uint8_t nozzle;
                uint32_t volume_dL;
                if (PumpResp_ParseRealtimeVolume(&resp, &nozzle, &volume_dL)) {
                    fsm->rt_volume_dL = volume_dL;
                }
            }
            else if (resp.cmd == 'R') {
                uint8_t nozzle;
                uint32_t money;
                if (PumpResp_ParseRealtimeMoney(&resp, &nozzle, &money)) {
                    fsm->rt_money = money;
                }
            }
            else if (resp.cmd == 'C') {
                uint8_t nozzle;
                uint32_t totalizer_dL;
                if (PumpResp_ParseTotalizer(&resp, &nozzle, &totalizer_dL)) {
                    fsm->totalizer_dL = totalizer_dL;
                }
            }
        }
    }
    
    /* State machine transitions */
    switch (fsm->state) {
        case TRX_IDLE:
            /* Auto-cleanup: if pump shows S90 but we're idle, send N to close */
            if (dev->status == 9 && gkl->state == GKL_STATE_IDLE) {
                PumpTrans_End(gkl, dev->ctrl_addr, dev->slave_addr);
            }
            break;
            
        case TRX_PRESET_SENT:
            if (dev->status == 3) {
                fsm->state = TRX_ARMED;
            }
            break;
            
        case TRX_ARMED:
            if (dev->status == 4 || dev->status == 6) {
                fsm->state = TRX_DISPENSING;
            }
            break;
            
        case TRX_DISPENSING:
            /* Poll realtime data every 500ms */
            if ((now - fsm->last_poll_ms) > 500 && gkl->state == GKL_STATE_IDLE) {
                fsm->last_poll_ms = now;
                PumpTrans_PollRealtimeVolume(gkl, dev->ctrl_addr, dev->slave_addr, 1);
            }
            
            if (dev->status == 8) {
                fsm->state = TRX_COMPLETE;
            }
            break;
            
        case TRX_PAUSED:
            /* Waiting for resume or cancel */
            break;
            
        case TRX_COMPLETE:
            /* Auto-close on S90 (nozzle returned) */
            if (dev->status == 9 && gkl->state == GKL_STATE_IDLE) {
                PumpTrans_End(gkl, dev->ctrl_addr, dev->slave_addr);
                fsm->state = TRX_CLOSING;
            }
            break;
            
        case TRX_CLOSING:
            if (dev->status == 1) {
                fsm->state = TRX_IDLE;
                fsm->rt_volume_dL = 0;
                fsm->rt_money = 0;
            }
            break;
    }
}

bool TrxFSM_StartVolume(TransactionFSM *fsm, uint32_t volume_dL)
{
    if (!fsm || !fsm->gkl || fsm->state != TRX_IDLE) return false;
    
    const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
    if (!dev) return false;
    
    GKL_Link *gkl = &fsm->gkl->link;
    if (gkl->state != GKL_STATE_IDLE) return false;
    
    fsm->preset_volume_dL = volume_dL;
    fsm->rt_volume_dL = 0;
    fsm->rt_money = 0;
    
    if (PumpTrans_PresetVolume(gkl, dev->ctrl_addr, dev->slave_addr, 1, volume_dL, dev->price)) {
        fsm->state = TRX_PRESET_SENT;
        fsm->last_poll_ms = HAL_GetTick();
        return true;
    }
    return false;
}

bool TrxFSM_StartMoney(TransactionFSM *fsm, uint32_t money)
{
    if (!fsm || !fsm->gkl || fsm->state != TRX_IDLE) return false;
    
    const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
    if (!dev) return false;
    
    GKL_Link *gkl = &fsm->gkl->link;
    if (gkl->state != GKL_STATE_IDLE) return false;
    
    fsm->preset_money = money;
    fsm->rt_volume_dL = 0;
    fsm->rt_money = 0;
    
    if (PumpTrans_PresetMoney(gkl, dev->ctrl_addr, dev->slave_addr, 1, money, dev->price)) {
        fsm->state = TRX_PRESET_SENT;
        fsm->last_poll_ms = HAL_GetTick();
        return true;
    }
    return false;
}

bool TrxFSM_Pause(TransactionFSM *fsm)
{
    if (!fsm || !fsm->gkl || fsm->state != TRX_DISPENSING) return false;
    
    const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
    if (!dev) return false;
    
    GKL_Link *gkl = &fsm->gkl->link;
    if (gkl->state != GKL_STATE_IDLE) return false;
    
    if (PumpTrans_Stop(gkl, dev->ctrl_addr, dev->slave_addr)) {
        fsm->state = TRX_PAUSED;
        return true;
    }
    return false;
}

bool TrxFSM_Resume(TransactionFSM *fsm)
{
    if (!fsm || !fsm->gkl || fsm->state != TRX_PAUSED) return false;
    
    const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
    if (!dev) return false;
    
    GKL_Link *gkl = &fsm->gkl->link;
    if (gkl->state != GKL_STATE_IDLE) return false;
    
    if (PumpTrans_Resume(gkl, dev->ctrl_addr, dev->slave_addr)) {
        fsm->state = TRX_DISPENSING;
        return true;
    }
    return false;
}

bool TrxFSM_Cancel(TransactionFSM *fsm)
{
    if (!fsm || !fsm->gkl) return false;
    
    if (fsm->state == TRX_IDLE || fsm->state == TRX_CLOSING) return false;
    
    const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
    if (!dev) return false;
    
    GKL_Link *gkl = &fsm->gkl->link;
    if (gkl->state != GKL_STATE_IDLE) return false;
    
    if (fsm->state == TRX_PRESET_SENT || fsm->state == TRX_ARMED) {
        fsm->state = TRX_IDLE;
        fsm->rt_volume_dL = 0;
        fsm->rt_money = 0;
        return true;
    }
    
    if (PumpTrans_End(gkl, dev->ctrl_addr, dev->slave_addr)) {
        fsm->state = TRX_CLOSING;
        return true;
    }
    return false;
}

TrxState TrxFSM_GetState(TransactionFSM *fsm)
{
    return fsm ? fsm->state : TRX_IDLE;
}

uint32_t TrxFSM_GetRealtimeVolume(TransactionFSM *fsm)
{
    return fsm ? fsm->rt_volume_dL : 0;
}

uint32_t TrxFSM_GetRealtimeMoney(TransactionFSM *fsm)
{
    return fsm ? fsm->rt_money : 0;
}
