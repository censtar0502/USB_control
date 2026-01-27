/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ui.h
  * @brief   OLED UI + menu for Control Panel (protocol-agnostic).
  ******************************************************************************
  */
/* USER CODE END Header */

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
    UI_SCREEN_HOME = 0,
    UI_SCREEN_MENU,
    UI_SCREEN_DIAG,
    UI_SCREEN_EDIT_PRICE,
    UI_SCREEN_EDIT_ADDR,
    UI_SCREEN_SAVING
} UI_Screen;

typedef struct
{
    PumpMgr   *mgr;
    Settings  *settings;

    uint32_t last_render_ms;

    UI_Screen screen;

    /* Active pump selection (1..N). Used by HOME shortcuts. */
    uint8_t active_pump_id;

    /* Menu navigation */
    uint8_t menu_index;

    /* Edit context */
    uint8_t edit_pump_index; /* 0-based */
    char edit_buf[8];
    uint8_t edit_len;

    /* Transient message timer */
    uint32_t toast_until_ms;
    char toast_line[24];
} UI_Context;

void UI_Init(UI_Context *ui, PumpMgr *mgr, Settings *settings);

/**
 * @brief UI task. Feed raw key symbols from keyboard. Call as often as possible.
 * @param ui    UI context
 * @param key   Raw key char from KEYBOARD_GetKey() (0 if none)
 */
void UI_Task(UI_Context *ui, char key);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
