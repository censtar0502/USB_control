#ifndef PUMP_MGR_H
#define PUMP_MGR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*
 * Pump manager (high-level polling sequencer) for GasKitLink.
 *
 * Required behavior (matches эталонный лог):
 *  - Always poll status with "SR" (cmd='S', data="R")
 *  - When status indicates transaction is running (e.g. "S61U"):
 *      after SR response, poll progress:
 *        1) "LM" (cmd='L', data="M")  -> response "L..."
 *        2) "RS" (cmd='R', data="S")  -> response "R..."
 *      then back to SR again.
 *
 * Integration:
 *  - Call GKL_Task() frequently (main loop or timer).
 *  - Call PumpMgr_Task() frequently after GKL_Task().
 */

#include "gkl_link.h"
#include "pump_response_parser.h"

typedef enum
{
    PUMPMGR_STATE_UNKNOWN = 0,
    PUMPMGR_STATE_IDLE,        /* S10S etc: no transaction */
    PUMPMGR_STATE_PREPARED,    /* after V1... until start */
    PUMPMGR_STATE_PAUSED,      /* S31P */
    PUMPMGR_STATE_WAIT,        /* S41W */
    PUMPMGR_STATE_RUNNING,     /* S61U */
    PUMPMGR_STATE_FINISHING,   /* S81[ */
    PUMPMGR_STATE_DONE,        /* S90[ */
    PUMPMGR_STATE_ERROR
} PumpMgrState;

typedef struct
{
    /* link */
    GKL_Link *link;

    /* timing */
    uint32_t sr_period_ms;       /* base status poll period */
    uint32_t lr_period_ms;       /* period between LM/RS cycles when running */
    uint32_t next_send_ms;

    /* internal sequencer */
    uint8_t  step;               /* 0:SR, 1:LM, 2:RS */
    bool     waiting_resp;

    /* decoded status */
    uint16_t last_status_code;   /* numeric status (e.g. 10,31,41,61,81,90) */
    char     last_status_char;   /* letter state (S,P,W,U,'[', etc) */
    PumpMgrState state;

    /* latest progress */
    uint8_t  last_nozzle;
    uint32_t last_volume_dL;     /* deciliters (0.1 L) */
    uint32_t last_money;         /* currency minor units (as in protocol) */

    /* flags for UI */
    bool     has_volume;
    bool     has_money;
} PumpMgr;

void PumpMgr_Init(PumpMgr *mgr, GKL_Link *link);

/* optional tuning (call after Init) */
void PumpMgr_SetPeriods(PumpMgr *mgr, uint32_t sr_period_ms, uint32_t lr_period_ms);

/* call often from main loop (after GKL_Task()) */
void PumpMgr_Task(PumpMgr *mgr);

/* getters */
PumpMgrState PumpMgr_GetState(const PumpMgr *mgr);
uint16_t     PumpMgr_GetLastStatusCode(const PumpMgr *mgr);
char         PumpMgr_GetLastStatusChar(const PumpMgr *mgr);

bool         PumpMgr_GetLastVolume_dL(const PumpMgr *mgr, uint8_t *nozzle, uint32_t *volume_dL);
bool         PumpMgr_GetLastMoney(const PumpMgr *mgr, uint8_t *nozzle, uint32_t *money);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_MGR_H */
