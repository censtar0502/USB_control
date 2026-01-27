#include "app.h"
#include "cdc_logger.h"
#include "keyboard.h"
#include <string.h>
#include <stdio.h>

/* Static application context */
static AppContext s_app;

/* Event queue helpers */
static uint8_t event_queue_next(uint8_t idx) {
    return (idx + 1) % 8;
}

static bool event_queue_is_full(void) {
    return event_queue_next(s_app.event_queue.head) == s_app.event_queue.tail;
}

static bool event_queue_is_empty(void) {
    return s_app.event_queue.head == s_app.event_queue.tail;
}

void APP_PushEvent(PumpEvent *ev)
{
    if (ev == NULL) return;

    if (event_queue_is_full()) {
        /* Drop oldest event */
        s_app.event_queue.tail = event_queue_next(s_app.event_queue.tail);
    }

    s_app.event_queue.events[s_app.event_queue.head] = *ev;
    s_app.event_queue.head = event_queue_next(s_app.event_queue.head);
}

bool APP_PopEvent(PumpEvent *ev)
{
    if (ev == NULL) return false;
    if (event_queue_is_empty()) return false;

    *ev = s_app.event_queue.events[s_app.event_queue.tail];
    s_app.event_queue.tail = event_queue_next(s_app.event_queue.tail);
    return true;
}

/* Callback function to handle events from PumpMgr */
static void pump_event_handler(PumpEvent *ev)
{
    if (ev == NULL) return;

    /* Push event to application queue */
    APP_PushEvent(ev);

    /* Log some events for debugging */
    if (ev->type == PUMP_EVT_TOTALIZER) {
        CDC_Log("APP: Totalizer event");
    }
}

void APP_Init(UART_HandleTypeDef *huart_trk1, UART_HandleTypeDef *huart_trk2, I2C_HandleTypeDef *hi2c)
{
    CDC_Log(">>> APP_Init start");

    /* Initialize protocol instances */
    PumpProtoGKL_Init(&s_app.gkl1, huart_trk1);
    PumpProtoGKL_SetTag(&s_app.gkl1, "TRK1");
    PumpProtoGKL_Bind(&s_app.proto1, &s_app.gkl1);

    PumpProtoGKL_Init(&s_app.gkl2, huart_trk2);
    PumpProtoGKL_SetTag(&s_app.gkl2, "TRK2");
    PumpProtoGKL_Bind(&s_app.proto2, &s_app.gkl2);

    CDC_Log(">>> GKL protocol instances ready");

    /* Initialize pump manager with 1 second polling */
    PumpMgr_Init(&s_app.mgr, 1000);

    /* Add pumps (IDs 1 and 2) with default addresses - MATCH YOUR SIGNATURE */
    if (!PumpMgr_Add(&s_app.mgr, 1, &s_app.proto1, 0, 1)) {
        CDC_Log(">>> ERROR: Failed to add TRK1 to manager");
    }
    if (!PumpMgr_Add(&s_app.mgr, 2, &s_app.proto2, 0, 2)) {
        CDC_Log(">>> ERROR: Failed to add TRK2 to manager");
    }

    /* Initialize settings (EEPROM) */
    Settings_Init(&s_app.settings, hi2c);

    /* Try to load settings from EEPROM */
    if (Settings_Load(&s_app.settings)) {
        CDC_Log(">>> Settings loaded from EEPROM");

        /* Apply loaded settings to pump manager */
        for (uint8_t i = 0; i < 2; i++) {
            /* Direct access to settings data */
            uint8_t addr = s_app.settings.data.pump[i].slave_addr;
            uint16_t price = s_app.settings.data.pump[i].price;

            PumpMgr_SetSlaveAddr(&s_app.mgr, i + 1, addr);
            PumpMgr_SetPrice(&s_app.mgr, i + 1, price);

            /* Log with manual formatting */
            char msg[64];
            snprintf(msg, sizeof(msg), ">>> TRK%u: addr=%u price=%u",
                    (unsigned)(i + 1), (unsigned)addr, (unsigned)price);
            CDC_Log(msg);
        }
    } else {
        CDC_Log(">>> No valid settings in EEPROM, using defaults");
    }

    /* Initialize UI */
    UI_Init(&s_app.ui, &s_app.mgr, &s_app.settings);

    /* Initialize event queue */
    s_app.event_queue.head = 0;
    s_app.event_queue.tail = 0;

    CDC_Log(">>> APP_Init complete");
}

void APP_Task(void)
{
    char key = 0;

    /* 1. Get keyboard input */
    key = KEYBOARD_GetKey();

    /* 2. Process pump manager (protocol polling, etc.) */
    PumpMgr_Task(&s_app.mgr);

    /* 3. Process events from pump manager */
    PumpEvent ev;

    /* Get events from manager */
    while (PumpMgr_PopEvent(&s_app.mgr, &ev)) {
        pump_event_handler(&ev);
    }

    /* 4. Process application event queue */
    while (APP_PopEvent(&ev)) {
        if (ev.type == PUMP_EVT_TOTALIZER) {
            /* Handle totalizer events if needed */
        }
    }

    /* 5. Process UI */
    UI_Task(&s_app.ui, key);

    /* 6. Process settings (EEPROM saves) */
    Settings_Task(&s_app.settings);

    /* 7. Process USB CDC logging */
    CDC_LOG_Task();
}
