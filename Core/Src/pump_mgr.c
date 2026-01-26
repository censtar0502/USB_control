/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    pump_mgr.c
  * @brief   Protocol-agnostic pump device manager (polling, cached state)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "pump_mgr.h"
#include <string.h>

void PumpMgr_Init(PumpMgr *m, uint32_t poll_period_ms)
{
    if (m == NULL) return;
    memset(m, 0, sizeof(*m));
    m->poll_period_ms = poll_period_ms;
    for (uint8_t i = 0u; i < (uint8_t)PUMP_MAX_DEVICES; i++)
    {
        m->next_poll_ms[i] = 0u;
    }
}

bool PumpMgr_Add(PumpMgr *m, uint8_t id, PumpProto *proto, uint8_t ctrl_addr, uint8_t slave_addr)
{
    if (m == NULL || proto == NULL) return false;
    if (m->count >= (uint8_t)PUMP_MAX_DEVICES) return false;

    /* Ensure unique id */
    for (uint8_t i = 0u; i < m->count; i++)
    {
        if (m->dev[i].id == id) return false;
    }

    PumpDevice *d = &m->dev[m->count];
    memset(d, 0, sizeof(*d));
    d->id = id;
    d->proto = proto;
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
        if (m->dev[i].id == id) return &m->dev[i];
    }
    return NULL;
}

const PumpDevice *PumpMgr_GetConst(const PumpMgr *m, uint8_t id)
{
    if (m == NULL) return NULL;
    for (uint8_t i = 0u; i < m->count; i++)
    {
        if (m->dev[i].id == id) return &m->dev[i];
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
        if (m->dev[i].id == id)
        {
            m->dev[i].last_error = 0u;
            m->dev[i].fail_count = 0u;
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
        if (m->dev[i].id == id)
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

static void pumpmgr_handle_event(PumpMgr *m, const PumpEvent *ev)
{
    if (m == NULL || ev == NULL) return;

    /* Find matching device by address */
    for (uint8_t i = 0u; i < m->count; i++)
    {
        PumpDevice *d = &m->dev[i];
        if (d->ctrl_addr == ev->ctrl_addr && d->slave_addr == ev->slave_addr)
        {
            if (ev->type == PUMP_EVT_STATUS)
            {
                d->status = ev->status;
                d->nozzle = ev->nozzle;
                d->last_status_ms = HAL_GetTick();
                d->last_error = 0u;
                d->fail_count = 0u;
            }
            else if (ev->type == PUMP_EVT_ERROR)
            {
                d->last_error = ev->error_code;
                d->fail_count = ev->fail_count;
            }
        }
    }
}

void PumpMgr_Task(PumpMgr *m)
{
    if (m == NULL) return;
    uint32_t now = HAL_GetTick();

    /* Build unique protocol list so we don't call Task/PopEvent multiple times
       for the same protocol instance when multiple pumps share one bus/port. */
    PumpProto *protos[PUMP_MAX_DEVICES];
    uint8_t proto_count = 0u;
    for (uint8_t i = 0u; i < m->count; i++)
    {
        PumpProto *p = m->dev[i].proto;
        if (p == NULL) continue;

        bool seen = false;
        for (uint8_t j = 0u; j < proto_count; j++)
        {
            if (protos[j] == p)
            {
                seen = true;
                break;
            }
        }

        if (!seen && proto_count < (uint8_t)PUMP_MAX_DEVICES)
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

    /* 2) Periodic polling (round-robin per device, but respecting per-proto busy) */
    for (uint8_t i = 0u; i < m->count; i++)
    {
        PumpDevice *d = &m->dev[i];
        if (d->proto == NULL) continue;

        if (now < m->next_poll_ms[i]) continue;

        if (!PumpProto_IsIdle(d->proto))
        {
            /* try later */
            continue;
        }

        (void)PumpProto_PollStatus(d->proto, d->ctrl_addr, d->slave_addr);
        m->next_poll_ms[i] = now + m->poll_period_ms;
    }
}
