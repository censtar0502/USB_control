/* app.c - ORIGINAL WORKING VERSION */
#include "app.h"
#include "cdc_logger.h"
#include <stdio.h>
#include <string.h>

static AppContext s_app;

void APP_Init(UART_HandleTypeDef *huart_trk1, UART_HandleTypeDef *huart_trk2, I2C_HandleTypeDef *hi2c)
{
    CDC_Log(">>> System Booting...");
    
    memset(&s_app, 0, sizeof(s_app));
    
    /* Initialize GKL protocol */
    PumpProtoGKL_Init(&s_app.gkl1, huart_trk1);
    PumpProtoGKL_SetTag(&s_app.gkl1, "TRK1");
    
    PumpProtoGKL_Init(&s_app.gkl2, huart_trk2);
    PumpProtoGKL_SetTag(&s_app.gkl2, "TRK2");
    
    PumpProtoGKL_Bind(&s_app.proto1, &s_app.gkl1);
    PumpProtoGKL_Bind(&s_app.proto2, &s_app.gkl2);
    
    CDC_Log(">>> GKL protocol instances ready");
    
    /* Initialize pump manager */
    PumpMgr_Init(&s_app.mgr, 1000);
    PumpMgr_Add(&s_app.mgr, 1u, &s_app.proto1, 0u, 1u);
    PumpMgr_Add(&s_app.mgr, 2u, &s_app.proto2, 0u, 2u);
    
    /* Set default prices */
    PumpDevice *d1 = PumpMgr_Get(&s_app.mgr, 1u);
    PumpDevice *d2 = PumpMgr_Get(&s_app.mgr, 2u);
    if (d1) d1->price = 1122u;
    if (d2) d2->price = 2233u;
    
    /* Load settings from EEPROM */
    Settings_Init(&s_app.settings, hi2c);
    if (Settings_Load(&s_app.settings))
    {
        CDC_Log(">>> Settings loaded from EEPROM");
        Settings_ApplyToPumpMgr(&s_app.settings, &s_app.mgr);
        
        d1 = PumpMgr_Get(&s_app.mgr, 1u);
        d2 = PumpMgr_Get(&s_app.mgr, 2u);
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
    }
    else
    {
        CDC_Log(">>> Settings not found, using defaults");
    }
    
    /* Initialize UI */
    UI_Init(&s_app.ui, &s_app.mgr, &s_app.settings);
    
    CDC_Log(">>> APP_Init complete");
}

void APP_Task(void)
{
    PumpMgr_Task(&s_app.mgr);
    
    /* Handle events from pump manager */
    PumpEvent ev;
    while (APP_PopEvent(&ev))
    {
        if (ev.type == PUMP_EVT_TOTALIZER)
        {
            UI_HandleTotalizerEvent(&s_app.ui, ev.nozzle, ev.totalizer);
        }
    }
    
    UI_Task(&s_app.ui, 0);
    Settings_Task(&s_app.settings);
}

void APP_PushEvent(PumpEvent *ev)
{
    if (!ev) return;
    
    uint8_t next = (s_app.event_queue.head + 1u) & 7u;
    if (next != s_app.event_queue.tail)
    {
        s_app.event_queue.events[s_app.event_queue.head] = *ev;
        s_app.event_queue.head = next;
    }
}

bool APP_PopEvent(PumpEvent *ev)
{
    if (s_app.event_queue.tail == s_app.event_queue.head)
        return false;
        
    if (ev)
        *ev = s_app.event_queue.events[s_app.event_queue.tail];
        
    s_app.event_queue.tail = (s_app.event_queue.tail + 1u) & 7u;
    return true;
}
