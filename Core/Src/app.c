/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app.c
  * @brief   Application top-level (UI + Pump Manager + Protocol adapters)
  ******************************************************************************
  */
/* USER CODE END Header */

#include "app.h"

#include "pump_mgr.h"
#include "pump_proto_gkl.h"
#include "ui.h"
#include "settings.h"
#include "keyboard.h"
#include "cdc_logger.h"

#include <stdio.h>
#include <string.h>

/* ===================== Polling timing ===================== */
/* Pause between requests per channel (ms). Default: 200ms */
#ifndef APP_POLL_PERIOD_MS
#define APP_POLL_PERIOD_MS   (200u)
#endif


/* ---- Static singletons (no dynamic allocation) ---- */
static PumpMgr s_mgr;
static UI_Context s_ui;
static Settings s_settings;

/* Concrete protocol implementations (one per UART/link) */
static PumpProtoGKL s_proto1;
static PumpProtoGKL s_proto2;

/* Generic protocol handles exposed to PumpMgr/FSM (protocol-agnostic) */
static PumpProto s_p1;
static PumpProto s_p2;


/* Optional: application boot banner */
static void app_log_boot(const char *line)
{
    if (line == NULL) return;
    CDC_LOG_Push(line);
}

static const char *uart_name(const UART_HandleTypeDef *huart)
{
    if (huart == NULL) return "UART?";

    /* We only need USART2/USART3 today, but keep it safe */
#ifdef USART1
    if (huart->Instance == USART1) return "USART1";
#endif
#ifdef USART2
    if (huart->Instance == USART2) return "USART2";
#endif
#ifdef USART3
    if (huart->Instance == USART3) return "USART3";
#endif
#ifdef UART4
    if (huart->Instance == UART4)  return "UART4";
#endif
#ifdef UART5
    if (huart->Instance == UART5)  return "UART5";
#endif
#ifdef USART6
    if (huart->Instance == USART6) return "USART6";
#endif
#ifdef UART7
    if (huart->Instance == UART7)  return "UART7";
#endif
#ifdef UART8
    if (huart->Instance == UART8)  return "UART8";
#endif

    return "UART?";
}

void APP_Init(UART_HandleTypeDef *huart_pump1,
              UART_HandleTypeDef *huart_pump2,
              I2C_HandleTypeDef  *hi2c_eeprom)
{
    char buf[96];

    /* Settings first (read EEPROM once at boot) */
    Settings_Init(&s_settings, hi2c_eeprom);
    bool loaded = Settings_Load(&s_settings);

    /* Protocol adapters (GasKitLink today; can be swapped later) */
    PumpProtoGKL_Init(&s_proto1, huart_pump1);
    PumpProtoGKL_SetTag(&s_proto1, "TRK1");
    PumpProtoGKL_Bind(&s_p1, &s_proto1);
    snprintf(buf, sizeof(buf), ">>> GasKitLink Initialized on %s (non-blocking).\r\n", uart_name(huart_pump1));
    app_log_boot(buf);

    PumpProtoGKL_Init(&s_proto2, huart_pump2);
    PumpProtoGKL_SetTag(&s_proto2, "TRK2");
    PumpProtoGKL_Bind(&s_p2, &s_proto2);
    snprintf(buf, sizeof(buf), ">>> GasKitLink Initialized on %s (non-blocking).\r\n", uart_name(huart_pump2));
    app_log_boot(buf);

    /* Manager (poll period is protocol-agnostic). 200ms is safe for now. */
    PumpMgr_Init(&s_mgr, APP_POLL_PERIOD_MS);

    /* Ensure at least two entries exist in settings */
    if (s_settings.data.pump_count < 2u)
    {
        s_settings.data.pump_count = 2u;
        /* Defaults already clamped inside Settings_Defaults/Load */
    }

    /* Create devices (IDs 1..N) */
    (void)PumpMgr_Add(&s_mgr,
                     1u,
                     &s_p1,
                     s_settings.data.pump[0].ctrl_addr,
                     s_settings.data.pump[0].slave_addr);

    (void)PumpMgr_Add(&s_mgr,
                     2u,
                     &s_p2,
                     s_settings.data.pump[1].ctrl_addr,
                     s_settings.data.pump[1].slave_addr);

    (void)PumpMgr_SetPrice(&s_mgr, 1u, (uint32_t)s_settings.data.pump[0].price);
    (void)PumpMgr_SetPrice(&s_mgr, 2u, (uint32_t)s_settings.data.pump[1].price);

    /* UI */
    UI_Init(&s_ui, &s_mgr, &s_settings);

    /* Logs */
    app_log_boot(">>> APP: Control Panel started\r\n");
    snprintf(buf, sizeof(buf), ">>> APP: Settings %s (seq=%lu, slot=%u)\r\n",
             loaded ? "loaded" : "defaults",
             (unsigned long)s_settings.seq,
             (unsigned)s_settings.last_slot);
    app_log_boot(buf);

    snprintf(buf, sizeof(buf), ">>> APP: TRK1 addr=%02u price=%04u\r\n",
             (unsigned)s_settings.data.pump[0].slave_addr,
             (unsigned)s_settings.data.pump[0].price);
    app_log_boot(buf);

    snprintf(buf, sizeof(buf), ">>> APP: TRK2 addr=%02u price=%04u\r\n",
             (unsigned)s_settings.data.pump[1].slave_addr,
             (unsigned)s_settings.data.pump[1].price);
    app_log_boot(buf);
}

void APP_Task(void)
{
    /* Keyboard events are queued by timer scan. Here we just consume one event. */
    char key = KEYBOARD_GetKey();

    /* Run protocol/manager tasks (non-blocking) */
    PumpMgr_Task(&s_mgr);

    /* Settings async write task (non-blocking) */
    Settings_Task(&s_settings);

    /* UI */
    UI_Task(&s_ui, key);

    /* USB CDC logger flush */
    CDC_LOG_Task();
}
