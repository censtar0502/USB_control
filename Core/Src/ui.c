/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ui.c
  * @brief   OLED UI + menu for Control Panel (protocol-agnostic).
  ******************************************************************************
  */
/* USER CODE END Header */

#include "ui.h"
#include "ssd1309.h"
#include "cdc_logger.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

/* Physical/function keys */
#define KEY_TOT   ('A')  /* Totalizer all TRKs */
#define KEY_TIM   ('B')  /* Select TRK1 */
#define KEY_SEL   ('C')  /* Select TRK2 */
#define KEY_PRI   ('D')  /* Start transaction (P/L/F) */
#define KEY_RES   ('E')  /* Reset / Pause / End */
#define KEY_ESC   ('F')  /* Back / Cancel */
#define KEY_SET   ('G')  /* Menu / Navigate UP */
#define KEY_INQ   ('H')  /* Navigate DOWN */
#define KEY_OK    ('K')  /* Confirm / Save / Continue */

#define UI_RENDER_PERIOD_MS   (100u)

/* Helper functions */
static void ui_clear_screen(void)
{
    SSD1309_Fill(0);
}

static void ui_draw_line(uint8_t row, const char *text)
{
    SSD1309_SetCursor(0, (uint8_t)(row * 8u));
    SSD1309_WriteString(text, 1);
}

static void ui_toast(UI_Context *ui, const char *text, uint32_t ms)
{
    if (ui == NULL || text == NULL) return;
    strncpy(ui->toast_line, text, sizeof(ui->toast_line) - 1u);
    ui->toast_line[sizeof(ui->toast_line) - 1u] = 0;
    ui->toast_until_ms = HAL_GetTick() + ms;
}

/* Format volume in dL as XXX.XX L */
static void format_volume(uint32_t volume_dL, char *buf, size_t len)
{
    uint32_t liters = volume_dL / 10;
    uint32_t frac = volume_dL % 10;
    snprintf(buf, len, "%03lu.%02lu", (unsigned long)liters, (unsigned long)frac);
}

/* ============================================================================
 * HOME SCREEN - Simplified display
 * ========================================================================= */

static void ui_render_home(UI_Context *ui)
{
    if (ui == NULL) return;

    ui_clear_screen();

    char line[17];  /* 16 chars + null for 128px width */
    uint8_t row = 0;

    /* Render each TRK */
    for (uint8_t i = 0; i < 2; i++)
    {
        uint8_t trk_id = i + 1;
        const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, trk_id);
        if (!dev) continue;

        /* Status symbols */
        char select = (ui->active_pump_id == trk_id) ? '>' : ' ';
        char pause = ' ';
        char active = ' ';

        TrkDisplayState state = ui->trk[i].state;

        if (state == TRK_STATE_NOZZLE_UP) {
            /* Blinking !* */
            active = ui->blink_state ? '!' : ' ';
            pause = '*';
        } else if (state == TRK_STATE_PAUSED) {
            pause = 'P';
            active = '*';
        } else if (state == TRK_STATE_FUELLING || state == TRK_STATE_ARMED) {
            active = '*';
        }

        /* TRK line */
        if (state != TRK_STATE_IDLE) {
            /* With transaction */
            snprintf(line, sizeof(line), "%c%c%cTRK%u: P%04u",
                     select, pause, active, trk_id, (unsigned)dev->price);
            ui_draw_line(row++, line);

            /* Volume line */
            char vol_str[12];
            format_volume(ui->trk[i].rt_volume_dL, vol_str, sizeof(vol_str));
            snprintf(line, sizeof(line), "  L: %s", vol_str);
            ui_draw_line(row++, line);

            /* Money line */
            snprintf(line, sizeof(line), "  P: %06lu", (unsigned long)ui->trk[i].rt_money);
            ui_draw_line(row++, line);
        } else {
            /* Idle - just TRK name */
            snprintf(line, sizeof(line), "%cTRK%u", select, trk_id);
            ui_draw_line(row++, line);
        }
    }

    SSD1309_UpdateScreen();
}

/* ============================================================================
 * SELECT MODE SCREEN
 * ========================================================================= */

static void ui_render_select_mode(UI_Context *ui)
{
    if (ui == NULL) return;

    ui_clear_screen();

    uint8_t idx = ui->active_pump_id - 1;
    DispenseMode mode = ui->trk[idx].mode;

    char line[17];
    snprintf(line, sizeof(line), "TRK%u: MODE", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");

    /* Options */
    ui_draw_line(2, (mode == DISPENSE_MODE_VOLUME) ? ">L: LITERS" : " L: LITERS");
    ui_draw_line(3, (mode == DISPENSE_MODE_MONEY)  ? ">P: MONEY"  : " P: MONEY");
    ui_draw_line(4, (mode == DISPENSE_MODE_FULL)   ? ">F: FULL"   : " F: FULL");

    ui_draw_line(6, "SEL:next");
    ui_draw_line(7, "OK:ok ESC:back");

    SSD1309_UpdateScreen();
}

static bool ui_handle_select_mode(UI_Context *ui, char key)
{
    if (ui == NULL) return false;

    uint8_t idx = ui->active_pump_id - 1;

    if (key == KEY_SEL || key == KEY_PRI) {
        /* Cycle through modes */
        if (ui->trk[idx].mode == DISPENSE_MODE_VOLUME)
            ui->trk[idx].mode = DISPENSE_MODE_MONEY;
        else if (ui->trk[idx].mode == DISPENSE_MODE_MONEY)
            ui->trk[idx].mode = DISPENSE_MODE_FULL;
        else
            ui->trk[idx].mode = DISPENSE_MODE_VOLUME;
        return true;
    }
    else if (key == KEY_OK) {
        /* Confirm */
        if (ui->trk[idx].mode == DISPENSE_MODE_VOLUME) {
            ui->screen = UI_SCREEN_PRESET_VOLUME;
            ui->edit_len = 0;
            memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        } else if (ui->trk[idx].mode == DISPENSE_MODE_MONEY) {
            ui->screen = UI_SCREEN_PRESET_MONEY;
            ui->edit_len = 0;
            memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        } else {
            /* Full tank - start immediately */
            const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, ui->active_pump_id);
            if (dev && dev->price > 0) {
                ui->trk[idx].preset_value = (999999UL * 10UL) / dev->price;
                ui->trk[idx].preset_nozzle = 1;
                ui->trk[idx].state = TRK_STATE_ARMED;

                /* Send preset command - TODO: use protocol */
                ui->screen = UI_SCREEN_HOME;
            }
        }
        return true;
    }
    else if (key == KEY_ESC) {
        ui->screen = UI_SCREEN_HOME;
        ui->trk[idx].mode = DISPENSE_MODE_IDLE;
        return true;
    }

    return false;
}

/* ============================================================================
 * PRESET VOLUME SCREEN
 * ========================================================================= */

static void ui_render_preset_volume(UI_Context *ui)
{
    if (ui == NULL) return;

    ui_clear_screen();

    char line[17];
    snprintf(line, sizeof(line), "TRK%u: VOLUME", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");

    /* Show input */
    char vol_str[8] = "0";
    if (ui->edit_len > 0) {
        strncpy(vol_str, ui->edit_buf, ui->edit_len);
        vol_str[ui->edit_len] = '\0';
    }

    snprintf(line, sizeof(line), "L: %s", vol_str);
    ui_draw_line(3, line);

    ui_draw_line(6, "0-9:digit");
    ui_draw_line(7, "OK:start RES:clr");

    SSD1309_UpdateScreen();
}

static bool ui_handle_preset_volume(UI_Context *ui, char key)
{
    if (ui == NULL) return false;

    uint8_t idx = ui->active_pump_id - 1;

    if (key >= '0' && key <= '9') {
        if (ui->edit_len < 6) {
            ui->edit_buf[ui->edit_len++] = key;
            ui->edit_buf[ui->edit_len] = '\0';
        }
        return true;
    }
    else if (key == KEY_RES) {
        /* Clear input */
        ui->edit_len = 0;
        memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        return true;
    }
    else if (key == KEY_OK) {
        if (ui->edit_len > 0) {
            /* Parse as liters, convert to dL */
            uint32_t value = 0;
            for (uint8_t i = 0; i < ui->edit_len; i++) {
                value = value * 10 + (ui->edit_buf[i] - '0');
            }

            ui->trk[idx].preset_value = value * 10;  /* Convert to dL */
            ui->trk[idx].preset_nozzle = 1;
            ui->trk[idx].state = TRK_STATE_ARMED;
            ui->trk[idx].rt_volume_dL = 0;
            ui->trk[idx].rt_money = 0;

            /* Send preset command - TODO: use protocol when available */
            ui->screen = UI_SCREEN_HOME;
        }
        return true;
    }
    else if (key == KEY_ESC) {
        ui->screen = UI_SCREEN_SELECT_MODE;
        return true;
    }

    return false;
}

/* ============================================================================
 * PRESET MONEY SCREEN
 * ========================================================================= */

static void ui_render_preset_money(UI_Context *ui)
{
    if (ui == NULL) return;

    ui_clear_screen();

    char line[17];
    snprintf(line, sizeof(line), "TRK%u: MONEY", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");

    /* Show input */
    char money_str[8] = "0";
    if (ui->edit_len > 0) {
        strncpy(money_str, ui->edit_buf, ui->edit_len);
        money_str[ui->edit_len] = '\0';
    }

    snprintf(line, sizeof(line), "P: %s", money_str);
    ui_draw_line(3, line);

    ui_draw_line(6, "0-9:digit");
    ui_draw_line(7, "OK:start RES:clr");

    SSD1309_UpdateScreen();
}

static bool ui_handle_preset_money(UI_Context *ui, char key)
{
    if (ui == NULL) return false;

    uint8_t idx = ui->active_pump_id - 1;

    if (key >= '0' && key <= '9') {
        if (ui->edit_len < 6) {
            ui->edit_buf[ui->edit_len++] = key;
            ui->edit_buf[ui->edit_len] = '\0';
        }
        return true;
    }
    else if (key == KEY_RES) {
        /* Clear input */
        ui->edit_len = 0;
        memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        return true;
    }
    else if (key == KEY_OK) {
        if (ui->edit_len > 0) {
            /* Parse money */
            uint32_t value = 0;
            for (uint8_t i = 0; i < ui->edit_len; i++) {
                value = value * 10 + (ui->edit_buf[i] - '0');
            }

            ui->trk[idx].preset_value = value;
            ui->trk[idx].preset_nozzle = 1;
            ui->trk[idx].state = TRK_STATE_ARMED;
            ui->trk[idx].rt_volume_dL = 0;
            ui->trk[idx].rt_money = 0;

            /* Send preset command - TODO: use protocol when available */
            ui->screen = UI_SCREEN_HOME;
        }
        return true;
    }
    else if (key == KEY_ESC) {
        ui->screen = UI_SCREEN_SELECT_MODE;
        return true;
    }

    return false;
}

/* ============================================================================
 * TOTALIZER SCREEN
 * ========================================================================= */

static void ui_render_totalizer(UI_Context *ui)
{
    if (ui == NULL) return;

    ui_clear_screen();

    ui_draw_line(0, "TOTALIZERS");
    ui_draw_line(1, "");

    /* Show both TRKs - TODO: read from protocol when available */
    for (uint8_t i = 0; i < 2; i++) {
        uint8_t trk_id = i + 1;

        char line[17];
        snprintf(line, sizeof(line), "TRK%u: N/A", trk_id);
        ui_draw_line(2 + i * 2, line);
    }

    ui_draw_line(7, "ESC:back");

    SSD1309_UpdateScreen();
}

static bool ui_handle_totalizer(UI_Context *ui, char key)
{
    if (ui == NULL) return false;

    if (key == KEY_ESC || key == KEY_OK) {
        ui->screen = UI_SCREEN_HOME;
        return true;
    }

    return false;
}

/* ============================================================================
 * BACKGROUND TASK - Update states
 * ========================================================================= */

static void ui_update_states(UI_Context *ui)
{
    if (ui == NULL) return;

    uint32_t now = HAL_GetTick();

    /* Update blink */
    if ((now - ui->blink_timer_ms) > 500) {
        ui->blink_state = !ui->blink_state;
        ui->blink_timer_ms = now;
    }

    /* Update each TRK state based on pump status */
    for (uint8_t i = 0; i < 2; i++) {
        uint8_t trk_id = i + 1;
        const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, trk_id);
        if (!dev) continue;

        /* Update state based on pump status */
        if (ui->trk[i].state == TRK_STATE_ARMED && dev->status == 2) {
            /* Started fuelling */
            ui->trk[i].state = TRK_STATE_FUELLING;
        }
        else if (ui->trk[i].state == TRK_STATE_FUELLING && dev->status == 3) {
            /* Completed */
            ui->trk[i].state = TRK_STATE_NOZZLE_UP;
        }

        /* TODO: Poll realtime data when protocol functions available */
        /* For now, use dummy data for testing */
        if (ui->trk[i].state == TRK_STATE_FUELLING) {
            /* Increment dummy values for visual feedback */
            if ((now - ui->trk[i].last_poll_ms) > 500) {
                ui->trk[i].last_poll_ms = now;
                ui->trk[i].rt_volume_dL += 5;  /* +0.5L */
                ui->trk[i].rt_money += dev->price / 2;  /* corresponding money */
            }
        }
    }
}

/* ============================================================================
 * INIT & MAIN TASK
 * ========================================================================= */

void UI_Init(UI_Context *ui, PumpMgr *mgr, Settings *settings)
{
    if (ui == NULL) return;

    memset(ui, 0, sizeof(*ui));
    ui->mgr = mgr;
    ui->settings = settings;
    ui->screen = UI_SCREEN_HOME;
    ui->active_pump_id = 1;
    ui->blink_state = false;
    ui->blink_timer_ms = HAL_GetTick();

    /* Initialize TRK states */
    for (uint8_t i = 0; i < 2; i++) {
        ui->trk[i].state = TRK_STATE_IDLE;
        ui->trk[i].mode = DISPENSE_MODE_VOLUME;
    }
}

void UI_Task(UI_Context *ui, char key)
{
    if (ui == NULL || ui->mgr == NULL) return;

    uint32_t now = HAL_GetTick();

    /* Update states */
    ui_update_states(ui);

    /* Key handling */
    if (key != 0) {
        if (ui->screen == UI_SCREEN_HOME) {
            if (key == KEY_TIM) {
                /* Select TRK1 */
                ui->active_pump_id = 1;
                ui_toast(ui, "TRK1 selected", 500);
            }
            else if (key == KEY_SEL) {
                /* Select TRK2 */
                ui->active_pump_id = 2;
                ui_toast(ui, "TRK2 selected", 500);
            }
            else if (key == KEY_PRI) {
                /* Start transaction */
                uint8_t idx = ui->active_pump_id - 1;
                ui->trk[idx].mode = DISPENSE_MODE_VOLUME;  /* Default */
                ui->screen = UI_SCREEN_SELECT_MODE;
            }
            else if (key == KEY_TOT) {
                /* Show totalizers */
                ui->screen = UI_SCREEN_TOTALIZER;
            }
            else if (key == KEY_RES) {
                /* Handle RES based on active TRK state */
                uint8_t idx = ui->active_pump_id - 1;
                if (ui->trk[idx].state == TRK_STATE_FUELLING) {
                    /* Pause */
                    ui->trk[idx].state = TRK_STATE_PAUSED;
                    /* TODO: Send stop command when available */
                }
                else if (ui->trk[idx].state == TRK_STATE_NOZZLE_UP) {
                    /* Close transaction */
                    ui->trk[idx].state = TRK_STATE_IDLE;
                    ui->trk[idx].rt_volume_dL = 0;
                    ui->trk[idx].rt_money = 0;
                    /* TODO: Send end command when available */
                }
            }
            else if (key == KEY_OK) {
                /* Continue from pause */
                uint8_t idx = ui->active_pump_id - 1;
                if (ui->trk[idx].state == TRK_STATE_PAUSED) {
                    ui->trk[idx].state = TRK_STATE_FUELLING;
                    /* TODO: Send resume command when available */
                }
            }
        }
        else if (ui->screen == UI_SCREEN_SELECT_MODE) {
            ui_handle_select_mode(ui, key);
        }
        else if (ui->screen == UI_SCREEN_PRESET_VOLUME) {
            ui_handle_preset_volume(ui, key);
        }
        else if (ui->screen == UI_SCREEN_PRESET_MONEY) {
            ui_handle_preset_money(ui, key);
        }
        else if (ui->screen == UI_SCREEN_TOTALIZER) {
            ui_handle_totalizer(ui, key);
        }
    }

    /* Periodic render */
    if ((now - ui->last_render_ms) >= UI_RENDER_PERIOD_MS) {
        ui->last_render_ms = now;

        if (ui->screen == UI_SCREEN_HOME) {
            ui_render_home(ui);
        }
        else if (ui->screen == UI_SCREEN_SELECT_MODE) {
            ui_render_select_mode(ui);
        }
        else if (ui->screen == UI_SCREEN_PRESET_VOLUME) {
            ui_render_preset_volume(ui);
        }
        else if (ui->screen == UI_SCREEN_PRESET_MONEY) {
            ui_render_preset_money(ui);
        }
        else if (ui->screen == UI_SCREEN_TOTALIZER) {
            ui_render_totalizer(ui);
        }
    }
}
