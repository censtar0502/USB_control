/* ui.h - ORIGINAL WORKING VERSION - FIXED */
#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "pump_mgr.h"
#include "settings.h"

typedef enum
{
    DISPENSE_MODE_IDLE = 0,
    DISPENSE_MODE_VOLUME,
    DISPENSE_MODE_MONEY,
    DISPENSE_MODE_FULL_TANK,
    DISPENSE_MODE_COUNT
} DispenseMode;

typedef enum
{
    UI_SCREEN_HOME = 0,
    UI_SCREEN_DISPENSE_MODE,
    UI_SCREEN_MENU,
    UI_SCREEN_EDIT,
    UI_SCREEN_EDIT_PRICE,     /* ADDED */
    UI_SCREEN_EDIT_ADDR,      /* ADDED */
    UI_SCREEN_TOTALIZER
} UI_Screen;

typedef struct
{
    PumpMgr   *mgr;
    Settings  *settings;

    uint32_t last_render_ms;

    UI_Screen screen;

    uint8_t active_pump_id;

    uint8_t menu_index;

    uint8_t edit_pump_index;
    char edit_buf[8];
    uint8_t edit_len;

    uint32_t totalizer_values[6];
    bool totalizer_data_valid;
    uint32_t totalizer_last_update_ms;
    uint8_t totalizer_current_nozzle;

    DispenseMode dispense_mode[2];

    uint32_t toast_until_ms;
    char toast_line[24];
} UI_Context;

void UI_Init(UI_Context *ui, PumpMgr *mgr, Settings *settings);

void UI_Task(UI_Context *ui, char key);

void UI_RequestTotalizerUpdate(UI_Context *ui);

void UI_HandleTotalizerEvent(UI_Context *ui, uint8_t nozzle, uint32_t value);

const char* UI_GetDispenseModeString(DispenseMode mode);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
