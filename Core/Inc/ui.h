/* ui.h - Refactored with FSM */
#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <stdbool.h>
#include "transaction_fsm.h"
#include "settings.h"

typedef enum {
    UI_SCREEN_HOME = 0,
    UI_SCREEN_SELECT_MODE,
    UI_SCREEN_PRESET_VOLUME,
    UI_SCREEN_PRESET_MONEY,
    UI_SCREEN_TOTALIZER
} UI_Screen;

typedef struct {
    /* FSM instances */
    TransactionFSM *trk1_fsm;
    TransactionFSM *trk2_fsm;
    
    Settings *settings;
    
    /* UI state */
    UI_Screen screen;
    uint8_t active_pump_id;
    
    /* Input */
    char edit_buf[8];
    uint8_t edit_len;
    
    /* Mode selection: 0=Volume, 1=Money, 2=Full */
    uint8_t selected_mode;
    
    /* Display */
    uint32_t last_render_ms;
    uint32_t blink_timer_ms;
    bool blink_state;
    
} UI_Context;

void UI_Init(UI_Context *ui, TransactionFSM *trk1_fsm, TransactionFSM *trk2_fsm, Settings *settings);
void UI_Task(UI_Context *ui, char key);

#endif
