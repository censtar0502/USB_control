/* ui.h - Complete with transaction support */
#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "pump_mgr.h"
#include "settings.h"
#include "pump_proto_gkl.h"  /* For GKL access */

typedef enum
{
    DISPENSE_MODE_IDLE = 0,
    DISPENSE_MODE_VOLUME,
    DISPENSE_MODE_MONEY,
    DISPENSE_MODE_FULL
} DispenseMode;

typedef enum
{
    TRK_STATE_IDLE = 0,
    TRK_STATE_ARMED,
    TRK_STATE_FUELLING,
    TRK_STATE_PAUSED,
    TRK_STATE_NOZZLE_UP
} TrkDisplayState;

typedef enum
{
    UI_SCREEN_HOME = 0,
    UI_SCREEN_SELECT_MODE,
    UI_SCREEN_PRESET_VOLUME,
    UI_SCREEN_PRESET_MONEY,
    UI_SCREEN_TOTALIZER
} UI_Screen;

typedef struct
{
    PumpMgr   *mgr;
    Settings  *settings;
    
    /* Direct GKL access for transactions */
    PumpProtoGKL *gkl1;
    PumpProtoGKL *gkl2;

    uint32_t last_render_ms;
    uint32_t blink_timer_ms;
    bool blink_state;

    UI_Screen screen;
    uint8_t active_pump_id;

    char edit_buf[8];
    uint8_t edit_len;

    uint32_t toast_until_ms;
    char toast_line[24];
    
    struct {
        TrkDisplayState state;
        DispenseMode mode;
        uint32_t preset_value;
        uint8_t preset_nozzle;
        uint32_t rt_volume_dL;
        uint32_t rt_money;
        uint32_t last_poll_ms;
    } trk[2];
} UI_Context;

void UI_Init(UI_Context *ui, PumpMgr *mgr, Settings *settings, 
             PumpProtoGKL *gkl1, PumpProtoGKL *gkl2);
void UI_Task(UI_Context *ui, char key);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
