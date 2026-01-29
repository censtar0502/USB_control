#include "pump_mgr.h"
#include "stm32h7xx_hal.h"  /* for HAL_GetTick; change if other MCU family */

/* steps */
#define STEP_SR   0u
#define STEP_LM   1u
#define STEP_RS   2u

static PumpMgrState prv_map_state(uint16_t code, char st)
{
    (void)st;
    switch (code)
    {
        case 10: return PUMPMGR_STATE_IDLE;       /* S10S */
        case 31: return PUMPMGR_STATE_PAUSED;     /* S31P */
        case 41: return PUMPMGR_STATE_WAIT;       /* S41W */
        case 61: return PUMPMGR_STATE_RUNNING;    /* S61U */
        case 81: return PUMPMGR_STATE_FINISHING;  /* S81[ */
        case 90: return PUMPMGR_STATE_DONE;       /* S90[ */
        default: return PUMPMGR_STATE_UNKNOWN;
    }
}

void PumpMgr_Init(PumpMgr *mgr, GKL_Link *link)
{
    if (mgr == NULL) { return; }

    mgr->link = link;

    mgr->sr_period_ms = 200u;
    mgr->lr_period_ms = 200u;

    mgr->next_send_ms = HAL_GetTick();
    mgr->step = STEP_SR;
    mgr->waiting_resp = false;

    mgr->last_status_code = 0u;
    mgr->last_status_char = 0;
    mgr->state = PUMPMGR_STATE_UNKNOWN;

    mgr->last_nozzle = 0u;
    mgr->last_volume_dL = 0u;
    mgr->last_money = 0u;
    mgr->has_volume = false;
    mgr->has_money = false;
}

void PumpMgr_SetPeriods(PumpMgr *mgr, uint32_t sr_period_ms, uint32_t lr_period_ms)
{
    if (mgr == NULL) { return; }
    if (sr_period_ms == 0u) { sr_period_ms = 200u; }
    if (lr_period_ms == 0u) { lr_period_ms = 200u; }
    mgr->sr_period_ms = sr_period_ms;
    mgr->lr_period_ms = lr_period_ms;
}

/* send one request frame according to current step */
static bool prv_send_step(PumpMgr *mgr)
{
    const uint8_t *data = NULL;
    uint8_t len = 0u;
    char cmd = 0;
    char expected = 0;

    if (mgr->link == NULL) { return false; }

    if (mgr->step == STEP_SR)
    {
        static const uint8_t sr_data[1] = { (uint8_t)'R' };
        cmd = 'S';
        expected = 'S';
        data = sr_data;
        len = 1u;
    }
    else if (mgr->step == STEP_LM)
    {
        static const uint8_t lm_data[1] = { (uint8_t)'M' };
        cmd = 'L';
        expected = 'L';
        data = lm_data;
        len = 1u;
    }
    else /* STEP_RS */
    {
        static const uint8_t rs_data[1] = { (uint8_t)'S' };
        cmd = 'R';
        expected = 'R';
        data = rs_data;
        len = 1u;
    }

    if (GKL_IsBusy(mgr->link))
    {
        return false;
    }

    if (!GKL_Send(mgr->link, cmd, data, len, expected))
    {
        return false;
    }

    mgr->waiting_resp = true;
    return true;
}

static void prv_handle_response(PumpMgr *mgr, const GKL_Frame *resp)
{
    /* SR response */
    if (resp->cmd == 'S')
    {
        uint16_t code = 0u;
        char st = 0;

        if (PumpResp_ParseStatus(resp, &code, &st))
        {
            mgr->last_status_code = code;
            mgr->last_status_char = st;
            mgr->state = prv_map_state(code, st);
        }
        else
        {
            mgr->state = PUMPMGR_STATE_ERROR;
        }
        return;
    }

    /* LM response -> volume */
    if (resp->cmd == 'L')
    {
        uint8_t noz = 0u;
        uint32_t vol_dL = 0u;

        if (PumpResp_ParseRealtimeVolume(resp, &noz, &vol_dL))
        {
            mgr->last_nozzle = noz;
            mgr->last_volume_dL = vol_dL;
            mgr->has_volume = true;
        }
        return;
    }

    /* RS response -> money */
    if (resp->cmd == 'R')
    {
        uint8_t noz = 0u;
        uint32_t money = 0u;

        if (PumpResp_ParseRealtimeMoney(resp, &noz, &money))
        {
            mgr->last_nozzle = noz;
            mgr->last_money = money;
            mgr->has_money = true;
        }
        return;
    }

    /* other responses ignored here (V/C/etc are handled elsewhere if needed) */
}

void PumpMgr_Task(PumpMgr *mgr)
{
    uint32_t now;

    if (mgr == NULL) { return; }
    now = HAL_GetTick();

    /* If response is ready, consume it */
    if (mgr->waiting_resp && GKL_HasResponse(mgr->link))
    {
        GKL_Frame resp;
        if (GKL_GetResponse(mgr->link, &resp))
        {
            prv_handle_response(mgr, &resp);
        }
        mgr->waiting_resp = false;

        /* decide next step */
        if (mgr->step == STEP_SR)
        {
            if (mgr->state == PUMPMGR_STATE_RUNNING)
            {
                /* IMPORTANT: this is the missing part in your log:
                 * after S61U we must poll LM then RS like эталон */
                mgr->step = STEP_LM;
                mgr->next_send_ms = now; /* immediately */
            }
            else
            {
                mgr->step = STEP_SR;
                mgr->next_send_ms = now + mgr->sr_period_ms;
            }
        }
        else if (mgr->step == STEP_LM)
        {
            mgr->step = STEP_RS;
            mgr->next_send_ms = now; /* immediately */
        }
        else /* STEP_RS */
        {
            mgr->step = STEP_SR;
            /* when running, keep a stable LR cycle rate; otherwise SR rate */
            if (mgr->state == PUMPMGR_STATE_RUNNING)
            {
                mgr->next_send_ms = now + mgr->lr_period_ms;
            }
            else
            {
                mgr->next_send_ms = now + mgr->sr_period_ms;
            }
        }
    }

    /* If not waiting, try to send according to schedule */
    if (!mgr->waiting_resp)
    {
        if ((int32_t)(now - mgr->next_send_ms) >= 0)
        {
            (void)prv_send_step(mgr);
            /* if send failed (busy), we'll retry next tick without changing next_send_ms */
        }
    }
}

PumpMgrState PumpMgr_GetState(const PumpMgr *mgr)
{
    if (mgr == NULL) { return PUMPMGR_STATE_UNKNOWN; }
    return mgr->state;
}

uint16_t PumpMgr_GetLastStatusCode(const PumpMgr *mgr)
{
    if (mgr == NULL) { return 0u; }
    return mgr->last_status_code;
}

char PumpMgr_GetLastStatusChar(const PumpMgr *mgr)
{
    if (mgr == NULL) { return 0; }
    return mgr->last_status_char;
}

bool PumpMgr_GetLastVolume_dL(const PumpMgr *mgr, uint8_t *nozzle, uint32_t *volume_dL)
{
    if (mgr == NULL || nozzle == NULL || volume_dL == NULL) { return false; }
    if (!mgr->has_volume) { return false; }
    *nozzle = mgr->last_nozzle;
    *volume_dL = mgr->last_volume_dL;
    return true;
}

bool PumpMgr_GetLastMoney(const PumpMgr *mgr, uint8_t *nozzle, uint32_t *money)
{
    if (mgr == NULL || nozzle == NULL || money == NULL) { return false; }
    if (!mgr->has_money) { return false; }
    *nozzle = mgr->last_nozzle;
    *money = mgr->last_money;
    return true;
}
