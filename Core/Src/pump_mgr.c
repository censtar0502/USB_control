/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_mgr.c
  * @brief   Protocol-agnostic pump device manager (polling, cached state)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "pump_mgr.h"
#include "cdc_logger.h"
#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdio.h>

/*
 * Find a pump device by its link addressing (ctrl/slave).
 * We use it to map protocol events back to PumpDevice.
 */
static PumpDevice *pumpmgr_find(PumpMgr *m, uint8_t ctrl_addr, uint8_t slave_addr)
{
    if (m == NULL) return NULL;
    for (uint8_t i = 0u; i < m->count; i++)
    {
        PumpDevice *d = &m->pumps[i];
        if ((d->ctrl_addr == ctrl_addr) && (d->slave_addr == slave_addr))
        {
            return d;
        }
    }
    return NULL;
}

void PumpMgr_Init(PumpMgr *m, uint32_t poll_period_ms)
{
    if (m == NULL) return;
    memset(m, 0, sizeof(*m));
    m->poll_period_ms = poll_period_ms;
    for (uint8_t i = 0u; i < (uint8_t)PUMP_MGR_MAX_PUMPS; i++)
    {
        m->next_poll_ms[i] = 0u;
    }
}

bool PumpMgr_Add(PumpMgr *m, uint8_t id, PumpProto *proto, uint8_t ctrl_addr, uint8_t slave_addr)
{
    if (m == NULL || proto == NULL) return false;
    if (m->count >= (uint8_t)PUMP_MGR_MAX_PUMPS) return false;

    /* Ensure unique id */
    for (uint8_t i = 0u; i < m->count; i++)
    {
        if (m->pumps[i].id == id) return false;
    }

    PumpDevice *d = &m->pumps[m->count];
    memset(d, 0, sizeof(*d));
    d->id = id;
    d->proto = *proto;  /* Copy struct */
    d->ctrl_addr = ctrl_addr;
    d->slave_addr = slave_addr;
    d->price = 0u;
    d->status = 0u;
    d->nozzle = 0u;
    d->last_status_ms = 0u;
    d->last_error = 0u;
    d->fail_count = 0u;

    m->next_poll_ms[m->count] = 0u;
    m->count++;
    return true;
}

PumpDevice *PumpMgr_Get(PumpMgr *m, uint8_t id)
{
    if (m == NULL) return NULL;
    for (uint8_t i = 0u; i < m->count; i++)
    {
        if (m->pumps[i].id == id) return &m->pumps[i];
    }
    return NULL;
}

const PumpDevice *PumpMgr_GetConst(const PumpMgr *m, uint8_t id)
{
    if (m == NULL) return NULL;
    for (uint8_t i = 0u; i < m->count; i++)
    {
        if (m->pumps[i].id == id) return &m->pumps[i];
    }
    return NULL;
}

bool PumpMgr_SetPrice(PumpMgr *m, uint8_t id, uint32_t price)
{
    PumpDevice *d = PumpMgr_Get(m, id);
    if (d == NULL) return false;
    d->price = price;
    return true;
}

uint32_t PumpMgr_GetPrice(const PumpMgr *m, uint8_t id)
{
    const PumpDevice *d = PumpMgr_GetConst(m, id);
    if (d == NULL) return 0u;
    return d->price;
}

bool PumpMgr_SetSlaveAddr(PumpMgr *m, uint8_t id, uint8_t slave_addr)
{
    PumpDevice *d = PumpMgr_Get(m, id);
    if (d == NULL) return false;
    d->slave_addr = slave_addr;
    return true;
}

uint8_t PumpMgr_GetSlaveAddr(const PumpMgr *m, uint8_t id)
{
    const PumpDevice *d = PumpMgr_GetConst(m, id);
    if (d == NULL) return 0u;
    return d->slave_addr;
}

bool PumpMgr_SetCtrlAddr(PumpMgr *m, uint8_t id, uint8_t ctrl_addr)
{
    PumpDevice *d = PumpMgr_Get(m, id);
    if (d == NULL) return false;
    d->ctrl_addr = ctrl_addr;
    return true;
}

uint8_t PumpMgr_GetCtrlAddr(const PumpMgr *m, uint8_t id)
{
    const PumpDevice *d = PumpMgr_GetConst(m, id);
    if (d == NULL) return 0u;
    return d->ctrl_addr;
}

void PumpMgr_ClearFail(PumpMgr *m, uint8_t id)
{
    if (m == NULL) return;
    for (uint8_t i = 0u; i < m->count; i++)
    {
        if (m->pumps[i].id == id)
        {
            m->pumps[i].last_error = 0u;
            m->pumps[i].fail_count = 0u;
            return;
        }
    }
}

void PumpMgr_RequestPollNow(PumpMgr *m, uint8_t id)
{
    if (m == NULL) return;
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0u; i < m->count; i++)
    {
        if (m->pumps[i].id == id)
        {
            m->next_poll_ms[i] = now;
            return;
        }
    }
}

void PumpMgr_RequestPollAllNow(PumpMgr *m)
{
    if (m == NULL) return;
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0u; i < m->count; i++)
    {
        m->next_poll_ms[i] = now;
    }
}

PumpProtoResult PumpMgr_RequestTotalizer(PumpMgr *mgr, uint8_t pump_id, uint8_t nozzle)
{
    if (mgr == NULL || pump_id == 0 || pump_id > PUMP_MGR_MAX_PUMPS) return PUMP_PROTO_ERR;

    PumpDevice *dev = &mgr->pumps[pump_id - 1u];

    /* Call request_totalizer if available */
    if (dev->proto.vt && dev->proto.vt->request_totalizer)
    {
        return dev->proto.vt->request_totalizer(dev->proto.ctx, dev->ctrl_addr, dev->slave_addr, nozzle);
    }

    return PUMP_PROTO_ERR;
}

static void pumpmgr_handle_event(PumpMgr *m, const PumpEvent *ev)
{
    if (m == NULL || ev == NULL) return;

    PumpDevice *d = pumpmgr_find(m, ev->ctrl_addr, ev->slave_addr);
    if (d == NULL) return;

    switch (ev->type)
    {
        case PUMP_EVT_STATUS:
            d->status = ev->status;
            d->nozzle = ev->nozzle;
            d->last_status_ms = HAL_GetTick();

            /* Status received -> link is alive */
            d->last_error = 0u;
            d->fail_count = 0u;
            break;

        case PUMP_EVT_ERROR:
            d->last_error = ev->error_code;
            d->fail_count = ev->fail_count;
            break;

        case PUMP_EVT_TOTALIZER:
            d->totalizer_nozzle = ev->nozzle_idx;
            d->totalizer_dL = ev->totalizer;
            d->totalizer_seq++;
            break;

        case PUMP_EVT_RT_VOLUME:
            d->nozzle = ev->rt_nozzle;
            d->rt_volume_dL = ev->rt_volume_dL;
            d->rt_vol_seq++;
            break;

        case PUMP_EVT_RT_MONEY:
            d->nozzle = ev->rt_nozzle;
            d->rt_money = ev->rt_money;
            d->rt_money_seq++;
            break;

        case PUMP_EVT_TRX_FINAL:
            d->trx_nozzle = ev->trx_nozzle;
            d->trx_volume_dL = ev->trx_volume_dL;
            d->trx_money = ev->trx_money;
            d->trx_price = ev->trx_price;
            d->trx_final_seq++;
            break;

        default:
            break;
    }
}

bool PumpMgr_PopEvent(PumpMgr *m, PumpEvent *out)
{
    if (m == NULL || out == NULL) return false;

    /* Try each pump's protocol */
    for (uint8_t i = 0u; i < m->count; i++)
    {
        PumpDevice *d = &m->pumps[i];
        if (PumpProto_PopEvent(&d->proto, out))
        {
            return true;
        }
    }

    return false;
}

void PumpMgr_Task(PumpMgr *m)
{
    if (m == NULL) return;
    uint32_t now = HAL_GetTick();

    /* Build unique protocol list */
    PumpProto *protos[PUMP_MGR_MAX_PUMPS];
    uint8_t proto_count = 0u;
    for (uint8_t i = 0u; i < m->count; i++)
    {
        PumpProto *p = &m->pumps[i].proto;

        bool seen = false;
        for (uint8_t j = 0u; j < proto_count; j++)
        {
            if (protos[j] == p)
            {
                seen = true;
                break;
            }
        }

        if (!seen && proto_count < (uint8_t)PUMP_MGR_MAX_PUMPS)
        {
            protos[proto_count++] = p;
        }
    }

    /* 1) Drive protocols and consume their events */
    for (uint8_t i = 0u; i < proto_count; i++)
    {
        PumpProto *p = protos[i];
        PumpProto_Task(p);

        PumpEvent ev;
        while (PumpProto_PopEvent(p, &ev))
        {
            pumpmgr_handle_event(m, &ev);
        }
    }

    /* 2) Periodic polling */
    for (uint8_t i = 0u; i < m->count; i++)
    {
        PumpDevice *d = &m->pumps[i];

        if (now < m->next_poll_ms[i]) continue;

        if (!PumpProto_IsIdle(&d->proto))
        {
            continue;
        }

        (void)PumpProto_PollStatus(&d->proto, d->ctrl_addr, d->slave_addr);

        /* When a transaction is active, we poll SR faster to keep pause minimal */
        uint32_t period = m->poll_period_ms;
        if (d->status == 3u || d->status == 4u || d->status == 6u || d->status == 8u || d->status == 9u)
        {
            period = (uint32_t)PUMP_MGR_ACTIVE_POLL_MS;
        }

        m->next_poll_ms[i] = now + period;
    }
}
