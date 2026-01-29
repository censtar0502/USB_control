/* transaction_fsm.c - FIXED: Fast polling like reference log */
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
    if (!fsm || !fsm->mgr || !fsm->gkl) return;

    const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
    if (!dev) return;

    /* Keep cached values for UI (these are maintained by PumpMgr) */
    fsm->rt_volume_dL = dev->rt_volume_dL;
    fsm->rt_money     = dev->rt_money;
    fsm->totalizer_dL = dev->totalizer_dL;

    GKL_Link *link = &fsm->gkl->link;
    uint32_t now = HAL_GetTick();

    switch (fsm->state)
    {
        case TRX_IDLE:
            fsm->poll_step = 0u;
            fsm->final_data_requested = 0u;

            /* Auto-close if nozzle returned while we are idle */
            if (dev->status == 9u && link->state == GKL_STATE_IDLE)
            {
                PumpTrans_End(fsm->gkl, dev->ctrl_addr, dev->slave_addr);
                fsm->state = TRX_CLOSING;
            }
            break;

        case TRX_PRESET_SENT:
            /* Preset accepted -> go ARMED */
            if (dev->status == 3u || dev->status == 4u || dev->status == 6u)
            {
                fsm->state = TRX_ARMED;
                fsm->poll_step = 0u;
                fsm->cycle_status_ms = 0u;
            }

            /* If returned to idle -> cancel */
            if (dev->status == 1u)
            {
                fsm->state = TRX_IDLE;
            }
            break;

        case TRX_ARMED:
            /* Start dispensing */
            if (dev->status == 6u || dev->status == 4u)
            {
                fsm->state = TRX_DISPENSING;
                fsm->poll_step = 0u;
                fsm->cycle_status_ms = 0u;
            }

            if (dev->status == 1u)
            {
                fsm->state = TRX_IDLE;
            }
            break;

        case TRX_DISPENSING:
        {
            /* Completed -> request final transaction data */
            if (dev->status == 8u)
            {
                fsm->state = TRX_COMPLETE;
                fsm->poll_step = 0u;
                fsm->final_data_requested = 0u;
                break;
            }

            if (dev->status == 1u)
            {
                fsm->state = TRX_IDLE;
                break;
            }

            /* Realtime cycle only when active and link is idle */
            if ((dev->status == 3u || dev->status == 4u || dev->status == 6u) && link->state == GKL_STATE_IDLE)
            {
                /* Start cycle only after a fresh SR update - helps keep the order: SR -> L -> R */
                if (fsm->poll_step == 0u)
                {
                    if (dev->last_status_ms != 0u && dev->last_status_ms != fsm->cycle_status_ms)
                    {
                        fsm->cycle_status_ms = dev->last_status_ms;
                        fsm->wait_vol_seq = dev->rt_vol_seq;

                        if (PumpTrans_PollRealtimeVolume(fsm->gkl, dev->ctrl_addr, dev->slave_addr, dev->nozzle))
                        {
                            fsm->poll_step = 1u;
                            fsm->last_poll_ms = now;
                        }
                    }
                }
                else if (fsm->poll_step == 1u)
                {
                    /* Wait for L response */
                    if (dev->rt_vol_seq != fsm->wait_vol_seq)
                    {
                        fsm->wait_money_seq = dev->rt_money_seq;

                        if (PumpTrans_PollRealtimeMoney(fsm->gkl, dev->ctrl_addr, dev->slave_addr, dev->nozzle))
                        {
                            fsm->poll_step = 2u;
                            fsm->last_poll_ms = now;
                        }
                    }
                    else if ((now - fsm->last_poll_ms) > 300u)
                    {
                        /* Timeout -> restart cycle */
                        fsm->poll_step = 0u;
                    }
                }
                else /* poll_step == 2 */
                {
                    /* Wait for R response */
                    if (dev->rt_money_seq != fsm->wait_money_seq)
                    {
                        fsm->poll_step = 0u;
                    }
                    else if ((now - fsm->last_poll_ms) > 300u)
                    {
                        fsm->poll_step = 0u;
                    }
                }
            }
            else
            {
                fsm->poll_step = 0u;
            }
        } break;

        case TRX_PAUSED:
            /* Waiting for resume or cancel */
            if (dev->status == 6u) fsm->state = TRX_DISPENSING;
            if (dev->status == 8u) fsm->state = TRX_COMPLETE;
            if (dev->status == 1u) fsm->state = TRX_IDLE;
            break;

        case TRX_COMPLETE:
            /* Send TU command once when we see S81 (status==8) */
            if (dev->status == 8u && !fsm->final_data_requested && link->state == GKL_STATE_IDLE)
            {
                fsm->wait_trx_seq = dev->trx_final_seq;
                if (PumpTrans_ReadTransaction(fsm->gkl, dev->ctrl_addr, dev->slave_addr))
                {
                    fsm->final_data_requested = 1u;
                }
            }

            /* Auto-close on S90 (nozzle returned) */
            if (dev->status == 9u && link->state == GKL_STATE_IDLE)
            {
                PumpTrans_End(fsm->gkl, dev->ctrl_addr, dev->slave_addr);
                fsm->state = TRX_CLOSING;
            }

            /* If transaction final arrived, cached data is already stored in PumpMgr (dev->trx_...) */
            break;

        case TRX_CLOSING:
            if (dev->status == 1u)
            {
                fsm->state = TRX_IDLE;
                fsm->rt_volume_dL = 0u;
                fsm->rt_money = 0u;
                fsm->final_data_requested = 0u;
                fsm->poll_step = 0u;
            }
            break;

        default:
            fsm->state = TRX_IDLE;
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
    fsm->poll_step = 0;
    fsm->final_data_requested = 0;
    
    if (PumpTrans_PresetVolume(fsm->gkl, dev->ctrl_addr, dev->slave_addr, 1, volume_dL, dev->price)) {
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
    fsm->poll_step = 0;
    fsm->final_data_requested = 0;
    
    if (PumpTrans_PresetMoney(fsm->gkl, dev->ctrl_addr, dev->slave_addr, 1, money, dev->price)) {
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
    
    if (PumpTrans_Stop(fsm->gkl, dev->ctrl_addr, dev->slave_addr)) {
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
    
    if (PumpTrans_Resume(fsm->gkl, dev->ctrl_addr, dev->slave_addr)) {
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
    
    if (PumpTrans_End(fsm->gkl, dev->ctrl_addr, dev->slave_addr)) {
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
