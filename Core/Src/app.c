/* app.c - With FSM and KEYBOARD */
#include "app.h"
#include "keyboard.h"
#include "cdc_logger.h"
#include <stdio.h>
#include <string.h>

static AppContext s_app;

void APP_Init(UART_HandleTypeDef *huart_trk1, UART_HandleTypeDef *huart_trk2, I2C_HandleTypeDef *hi2c)
{
    CDC_Log(">>> System Booting...");
    
    memset(&s_app, 0, sizeof(s_app));
    
    /* Init protocol */
    PumpProtoGKL_Init(&s_app.gkl1, huart_trk1);
    PumpProtoGKL_SetTag(&s_app.gkl1, "TRK1");
    PumpProtoGKL_Bind(&s_app.proto1, &s_app.gkl1);
    
    PumpProtoGKL_Init(&s_app.gkl2, huart_trk2);
    PumpProtoGKL_SetTag(&s_app.gkl2, "TRK2");
    PumpProtoGKL_Bind(&s_app.proto2, &s_app.gkl2);
    
    CDC_Log(">>> GKL protocol ready");
    
    /* Init pump manager */
    PumpMgr_Init(&s_app.mgr, 1000);
    PumpMgr_Add(&s_app.mgr, 1, &s_app.proto1, 0, 1);
    PumpMgr_Add(&s_app.mgr, 2, &s_app.proto2, 0, 2);
    
    /* Set default prices */
    PumpDevice *d1 = PumpMgr_Get(&s_app.mgr, 1);
    PumpDevice *d2 = PumpMgr_Get(&s_app.mgr, 2);
    if (d1) d1->price = 1122;
    if (d2) d2->price = 2233;
    
    /* Load settings */
    Settings_Init(&s_app.settings, hi2c);
    if (Settings_Load(&s_app.settings)) {
        CDC_Log(">>> Settings loaded from EEPROM");
        Settings_ApplyToPumpMgr(&s_app.settings, &s_app.mgr);
        
        d1 = PumpMgr_Get(&s_app.mgr, 1);
        d2 = PumpMgr_Get(&s_app.mgr, 2);
        if (d1) {
            char msg[64];
            snprintf(msg, sizeof(msg), ">>> TRK1: addr=%u price=%lu",
                    (unsigned)d1->slave_addr, (unsigned long)d1->price);
            CDC_Log(msg);
        }
        if (d2) {
            char msg[64];
            snprintf(msg, sizeof(msg), ">>> TRK2: addr=%u price=%lu",
                    (unsigned)d2->slave_addr, (unsigned long)d2->price);
            CDC_Log(msg);
        }
    } else {
        CDC_Log(">>> Settings not found, using defaults");
    }
    
    /* Init FSM */
    TrxFSM_Init(&s_app.trk1_fsm, 1, &s_app.mgr, &s_app.gkl1);
    TrxFSM_Init(&s_app.trk2_fsm, 2, &s_app.mgr, &s_app.gkl2);
    
    CDC_Log(">>> FSM initialized");
    
    /* Init UI */
    UI_Init(&s_app.ui, &s_app.trk1_fsm, &s_app.trk2_fsm, &s_app.settings);
    
    /* Init keyboard */
    KEYBOARD_Init();

    CDC_Log(">>> APP_Init complete");
}

void APP_Task(void)
{
    /* Run managers */
    PumpMgr_Task(&s_app.mgr);
    TrxFSM_Task(&s_app.trk1_fsm);
    TrxFSM_Task(&s_app.trk2_fsm);
    Settings_Task(&s_app.settings);

    /* Read keyboard */
    char key = KEYBOARD_GetKey();
    if (key != 0) {
        /* Log key press */
        char msg[16];
        snprintf(msg, sizeof(msg), "KEY: %c", key);
        CDC_Log(msg);

        /* Pass to UI */
        UI_Task(&s_app.ui, key);
    } else {
        /* Periodic UI update */
        UI_Task(&s_app.ui, 0);
    }
}

void APP_OnKeyPress(char key)
{
    /* Direct key injection (if needed) */
    UI_Task(&s_app.ui, key);
}
