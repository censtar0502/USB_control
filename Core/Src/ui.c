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
#include <stdio.h>
#include <string.h>

#define KEY_TOT       ('A')
#define KEY_ESC       ('F')
#define KEY_SET       ('G')
#define KEY_INQ       ('H')
#define KEY_TIM_CAL   ('B')
#define KEY_DAY_SEL   ('C')
#define KEY_MTH_PRI   ('D')
#define KEY_RES       ('E')
#define KEY_OK        ('K')

#define UI_RENDER_PERIOD_MS   (100u)
#define UI_NO_CONNECT_THRESHOLD   (10u)

static void ui_toast(UI_Context *ui, const char *text, uint32_t ms)
{
    if (ui == NULL || text == NULL) return;
    strncpy(ui->toast_line, text, sizeof(ui->toast_line) - 1u);
    ui->toast_line[sizeof(ui->toast_line) - 1u] = 0;
    ui->toast_until_ms = HAL_GetTick() + ms;
}

static void ui_clear_screen(void)
{
    SSD1309_Fill(0);
}

static void ui_draw_line(uint8_t row, const char *text)
{
    SSD1309_SetCursor(0, (uint8_t)(row * 8u));
    SSD1309_WriteString(text, 1);
}

static const char* ui_save_state_str(const Settings *s, char *buf, size_t buflen)
{
    if (s == NULL || buf == NULL || buflen < 10u) return "";

    SettingsSaveState st = Settings_GetSaveState(s);
    if (st == SETTINGS_SAVE_BUSY)
    {
        snprintf(buf, buflen, "EEP: SAVING");
    }
    else if (st == SETTINGS_SAVE_OK)
    {
        snprintf(buf, buflen, "EEP: OK");
    }
    else if (st == SETTINGS_SAVE_ERROR)
    {
        snprintf(buf, buflen, "EEP: ERR%u", (unsigned)Settings_GetSaveError(s));
    }
    else
    {
        snprintf(buf, buflen, "EEP: IDLE");
    }
    return buf;
}

const char* UI_GetDispenseModeString(DispenseMode mode)
{
    switch (mode)
    {
        case DISPENSE_MODE_VOLUME:   return "L (Volume)";
        case DISPENSE_MODE_MONEY:    return "P (Money)";
        case DISPENSE_MODE_FULL_TANK:return "F (Full Tank)";
        default: return "?";
    }
}

static void ui_render_home(UI_Context *ui)
{
    char line[32];
    char sbuf[16];

    const PumpDevice *d1 = PumpMgr_GetConst(ui->mgr, 1u);
    const PumpDevice *d2 = PumpMgr_GetConst(ui->mgr, 2u);

    ui_clear_screen();

    ui_draw_line(0, "TIM=TRK SEL=MODE TOT=TOTAL");

    if (d1)
    {
        snprintf(line, sizeof(line), "%cTRK1 A%02u P%04lu %s",
                 (ui->active_pump_id == 1u) ? '>' : ' ',
                 (unsigned)d1->slave_addr,
                 (unsigned long)d1->price,
                 UI_GetDispenseModeString(ui->dispense_mode[0]));
        ui_draw_line(1, line);
        if (d1->fail_count >= (uint8_t)UI_NO_CONNECT_THRESHOLD)
        {
            snprintf(line, sizeof(line), " No Connect!! F%u", (unsigned)d1->fail_count);
        }
        else
        {
            snprintf(line, sizeof(line), " S%u N%u F%u", (unsigned)d1->status, (unsigned)d1->nozzle, (unsigned)d1->fail_count);
        }
        ui_draw_line(2, line);
    }
    else
    {
        ui_draw_line(1, "TRK1: --");
        ui_draw_line(2, "");
    }

    if (d2)
    {
        snprintf(line, sizeof(line), "%cTRK2 A%02u P%04lu %s",
                 (ui->active_pump_id == 2u) ? '>' : ' ',
                 (unsigned)d2->slave_addr,
                 (unsigned long)d2->price,
                 UI_GetDispenseModeString(ui->dispense_mode[1]));
        ui_draw_line(3, line);
        if (d2->fail_count >= (uint8_t)UI_NO_CONNECT_THRESHOLD)
        {
            snprintf(line, sizeof(line), " No Connect!! F%u", (unsigned)d2->fail_count);
        }
        else
        {
            snprintf(line, sizeof(line), " S%u N%u F%u", (unsigned)d2->status, (unsigned)d2->nozzle, (unsigned)d2->fail_count);
        }
        ui_draw_line(4, line);
    }
    else
    {
        ui_draw_line(3, "TRK2: --");
        ui_draw_line(4, "");
    }

    ui_draw_line(5, "PRI=PRICE INQ=POLL RES=TRANS");
    ui_draw_line(6, ui_save_state_str(ui->settings, sbuf, sizeof(sbuf)));

    if (ui->toast_until_ms != 0u && (int32_t)(HAL_GetTick() - ui->toast_until_ms) < 0)
    {
        ui_draw_line(7, ui->toast_line);
    }
    else
    {
        ui_draw_line(7, "");
    }

    SSD1309_UpdateScreen();
}

static const char *ui_menu_item(uint8_t idx)
{
    switch (idx)
    {
        case 0: return "TRK1 PRICE";
        case 1: return "TRK1 ADDR";
        case 2: return "TRK2 PRICE";
        case 3: return "TRK2 ADDR";
        case 4: return "SAVE EEPROM";
        case 5: return "EXIT";
        default: return "";
    }
}

static void ui_render_menu(UI_Context *ui)
{
    char line[32];

    ui_clear_screen();
    ui_draw_line(0, "MENU <SET >INQ OK SEL ESC");

    for (uint8_t r = 0u; r < 6u; r++)
    {
        uint8_t idx = r;
        const char *item = ui_menu_item(idx);
        if (item[0] == 0) continue;

        if (ui->menu_index == idx)
        {
            snprintf(line, sizeof(line), "> %s", item);
        }
        else
        {
            snprintf(line, sizeof(line), "  %s", item);
        }
        ui_draw_line((uint8_t)(r + 1u), line);
    }

    SSD1309_UpdateScreen();
}

static void ui_render_edit(UI_Context *ui, bool is_price)
{
    char line[32];
    char title[24];

    uint8_t trk = (uint8_t)(ui->edit_pump_index + 1u);

    ui_clear_screen();

    if (is_price)
    {
        snprintf(title, sizeof(title), "EDIT TRK%u PRICE", (unsigned)trk);
        ui_draw_line(0, title);
        ui_draw_line(1, "Digits: 0-9");
        ui_draw_line(2, "RES/. BKSP OK=OK ESC=CAN");
        snprintf(line, sizeof(line), "VALUE: %s", ui->edit_buf);
        ui_draw_line(4, line);
        ui_draw_line(6, "Range: 0000..9999");
    }
    else
    {
        snprintf(title, sizeof(title), "EDIT TRK%u ADDR", (unsigned)trk);
        ui_draw_line(0, title);
        ui_draw_line(1, "Digits: 0-9");
        ui_draw_line(2, "RES/. BKSP OK=OK ESC=CAN");
        snprintf(line, sizeof(line), "VALUE: %s", ui->edit_buf);
        ui_draw_line(4, line);
        ui_draw_line(6, "Range: 01..32");
    }

    SSD1309_UpdateScreen();
}

static void ui_render_totalizer(UI_Context *ui)
{
    char line[32];

    ui_clear_screen();

    snprintf(line, sizeof(line), "TOTALIZER TRK%u", (unsigned)ui->active_pump_id);
    ui_draw_line(0, line);

    if (!ui->totalizer_data_valid)
    {
        ui_draw_line(1, "Requesting data...");
        ui_draw_line(2, "ESC to cancel");
    }
    else
    {
        for (uint8_t i = 0; i < 6; i++)
        {
            uint32_t liters = ui->totalizer_values[i] / 100;
            uint32_t centiliters = ui->totalizer_values[i] % 100;

            snprintf(line, sizeof(line), "Noz%u: %lu.%02lu L",
                     (unsigned)(i + 1),
                     (unsigned long)liters,
                     (unsigned long)centiliters);
            ui_draw_line((uint8_t)(i + 1), line);
        }

        ui_draw_line(7, "ESC=Back");
    }

    SSD1309_UpdateScreen();
}

static void ui_render_dispense_mode(UI_Context *ui)
{
    char line[32];

    ui_clear_screen();

    snprintf(line, sizeof(line), "DISPENSE MODE TRK%u", (unsigned)ui->active_pump_id);
    ui_draw_line(0, line);

    DispenseMode current = ui->dispense_mode[ui->active_pump_id - 1];
    snprintf(line, sizeof(line), "Current: %s", UI_GetDispenseModeString(current));
    ui_draw_line(2, line);

    ui_draw_line(3, "<SET >INQ to change");
    ui_draw_line(4, "L - Volume preset");
    ui_draw_line(5, "P - Money preset");
    ui_draw_line(6, "F - Full tank");

    ui_draw_line(7, "OK=Accept ESC=Cancel");

    SSD1309_UpdateScreen();
}

static bool ui_is_digit(char k)
{
    return (k >= '0' && k <= '9');
}

static uint32_t ui_parse_u32(const char *s)
{
    uint32_t v = 0u;
    while (s && *s)
    {
        if (*s < '0' || *s > '9') break;
        v = (v * 10u) + (uint32_t)(*s - '0');
        s++;
    }
    return v;
}

static void ui_edit_start(UI_Context *ui, uint8_t pump_index, bool is_price)
{
    ui->edit_pump_index = pump_index;
    ui->edit_len = 0u;
    memset(ui->edit_buf, 0, sizeof(ui->edit_buf));

    if (is_price)
    {
        uint32_t pr = PumpMgr_GetPrice(ui->mgr, (uint8_t)(pump_index + 1u));
        if (pr > 9999u) pr = 9999u;
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%04lu", (unsigned long)pr);
        ui->edit_len = (uint8_t)strlen(ui->edit_buf);
        ui->screen = UI_SCREEN_EDIT_PRICE;
    }
    else
    {
        uint8_t addr = PumpMgr_GetSlaveAddr(ui->mgr, (uint8_t)(pump_index + 1u));
        if (addr < 1u) addr = 1u;
        if (addr > 32u) addr = 32u;
        snprintf(ui->edit_buf, sizeof(ui->edit_buf), "%02u", (unsigned)addr);
        ui->edit_len = (uint8_t)strlen(ui->edit_buf);
        ui->screen = UI_SCREEN_EDIT_ADDR;
    }
}

void UI_Init(UI_Context *ui, PumpMgr *mgr, Settings *settings)
{
    if (ui == NULL) return;
    memset(ui, 0, sizeof(*ui));
    ui->mgr = mgr;
    ui->settings = settings;
    ui->last_render_ms = 0u;
    ui->screen = UI_SCREEN_HOME;
    ui->active_pump_id = 1u;
    ui->menu_index = 0u;
    ui->totalizer_data_valid = false;
    ui->totalizer_current_nozzle = 1;
    ui->toast_until_ms = 0u;
    ui->toast_line[0] = 0;

    ui->dispense_mode[0] = DISPENSE_MODE_MONEY;
    ui->dispense_mode[1] = DISPENSE_MODE_MONEY;

    for (uint8_t i = 0; i < 6; i++)
    {
        ui->totalizer_values[i] = 0;
    }

    ui_render_home(ui);
}

void UI_RequestTotalizerUpdate(UI_Context *ui)
{
    if (ui == NULL || ui->mgr == NULL) return;

    if (ui->totalizer_current_nozzle >= 1 && ui->totalizer_current_nozzle <= 6)
    {
        PumpMgr_RequestTotalizer(ui->mgr, ui->active_pump_id, ui->totalizer_current_nozzle);
    }
}

void UI_HandleTotalizerEvent(UI_Context *ui, uint8_t nozzle, uint32_t value)
{
    if (ui == NULL || nozzle < 1 || nozzle > 6) return;

    ui->totalizer_values[nozzle - 1] = value;

    ui->totalizer_current_nozzle++;
    if (ui->totalizer_current_nozzle > 6)
    {
        ui->totalizer_data_valid = true;
        ui->totalizer_current_nozzle = 1;
    }
}

void UI_Task(UI_Context *ui, char key)
{
    if (ui == NULL || ui->mgr == NULL) return;

    uint32_t now = HAL_GetTick();

    if (key != 0)
    {
        if (ui->active_pump_id < 1u) ui->active_pump_id = 1u;
        if (ui->active_pump_id > 2u) ui->active_pump_id = 2u;

        if (ui->screen == UI_SCREEN_HOME)
        {
            if (key == KEY_SET)
            {
                ui->screen = UI_SCREEN_MENU;
                ui->menu_index = 0u;
                ui_render_menu(ui);
                return;
            }
            else if (key == KEY_TIM_CAL)
            {
                ui->active_pump_id = (ui->active_pump_id == 1u) ? 2u : 1u;
                ui_toast(ui, (ui->active_pump_id == 1u) ? "Active TRK1" : "Active TRK2", 900u);
                ui_render_home(ui);
                return;
            }
            else if (key == KEY_DAY_SEL)
            {
                ui->screen = UI_SCREEN_DISPENSE_MODE;
                ui_render_dispense_mode(ui);
                return;
            }
            else if (key == KEY_TOT)
            {
                ui->screen = UI_SCREEN_TOTALIZER;
                ui->totalizer_data_valid = false;
                ui->totalizer_current_nozzle = 1;
                ui->totalizer_last_update_ms = now;
                for (uint8_t i = 0; i < 6; i++)
                {
                    ui->totalizer_values[i] = 0;
                }
                UI_RequestTotalizerUpdate(ui);
                ui_render_totalizer(ui);
                return;
            }
            else if (key == KEY_MTH_PRI)
            {
                ui_edit_start(ui, (uint8_t)(ui->active_pump_id - 1u), true);
                ui_render_edit(ui, true);
                return;
            }
            else if (key == KEY_INQ)
            {
                PumpMgr_RequestPollAllNow(ui->mgr);
                ui_toast(ui, "Poll now (all)", 900u);
                ui_render_home(ui);
                return;
            }
            else if (key == KEY_OK)
            {
                PumpMgr_RequestPollNow(ui->mgr, ui->active_pump_id);
                ui_toast(ui, (ui->active_pump_id == 1u) ? "Poll TRK1" : "Poll TRK2", 900u);
                ui_render_home(ui);
                return;
            }
            else if (key == KEY_RES)
            {
                PumpMgr_ClearFail(ui->mgr, ui->active_pump_id);
                PumpMgr_RequestPollNow(ui->mgr, ui->active_pump_id);
                ui_toast(ui, (ui->active_pump_id == 1u) ? "Retry TRK1" : "Retry TRK2", 900u);
                ui_render_home(ui);
                return;
            }
        }
        else if (ui->screen == UI_SCREEN_TOTALIZER)
        {
            if (key == KEY_ESC)
            {
                ui->screen = UI_SCREEN_HOME;
                ui_render_home(ui);
                return;
            }
        }
        else if (ui->screen == UI_SCREEN_DISPENSE_MODE)
        {
            if (key == KEY_ESC)
            {
                ui->screen = UI_SCREEN_HOME;
                ui_render_home(ui);
                return;
            }
            else if (key == KEY_SET)
            {
                uint8_t current = (uint8_t)ui->dispense_mode[ui->active_pump_id - 1];
                if (current == 0)
                {
                    current = DISPENSE_MODE_COUNT - 1;
                }
                else
                {
                    current--;
                }
                ui->dispense_mode[ui->active_pump_id - 1] = (DispenseMode)current;
                ui_render_dispense_mode(ui);
                return;
            }
            else if (key == KEY_INQ)
            {
                uint8_t current = (uint8_t)ui->dispense_mode[ui->active_pump_id - 1];
                current++;
                if (current >= DISPENSE_MODE_COUNT)
                {
                    current = 0;
                }
                ui->dispense_mode[ui->active_pump_id - 1] = (DispenseMode)current;
                ui_render_dispense_mode(ui);
                return;
            }
            else if (key == KEY_OK)
            {
                ui_toast(ui, "Mode saved", 900u);
                ui->screen = UI_SCREEN_HOME;
                ui_render_home(ui);
                return;
            }
        }
        else if (ui->screen == UI_SCREEN_MENU)
        {
            if (key == KEY_ESC)
            {
                ui->screen = UI_SCREEN_HOME;
                ui_render_home(ui);
                return;
            }
            else if (key == KEY_SET)
            {
                if (ui->menu_index > 0u) ui->menu_index--;
                ui_render_menu(ui);
                return;
            }
            else if (key == KEY_INQ)
            {
                if (ui->menu_index < 5u) ui->menu_index++;
                ui_render_menu(ui);
                return;
            }
            else if (key == KEY_OK)
            {
                switch (ui->menu_index)
                {
                    case 0: ui_edit_start(ui, 0u, true);  ui_render_edit(ui, true);  return;
                    case 1: ui_edit_start(ui, 0u, false); ui_render_edit(ui, false); return;
                    case 2: ui_edit_start(ui, 1u, true);  ui_render_edit(ui, true);  return;
                    case 3: ui_edit_start(ui, 1u, false); ui_render_edit(ui, false); return;
                    case 4:
                    {
                        if (ui->settings)
                        {
                            Settings_CaptureFromPumpMgr(ui->settings, ui->mgr);
                            if (Settings_RequestSave(ui->settings))
                            {
                                ui_toast(ui, "Saving...", 1500u);
                                ui->screen = UI_SCREEN_HOME;
                                ui_render_home(ui);
                            }
                            else
                            {
                                ui_toast(ui, "Save busy", 1500u);
                                ui_render_menu(ui);
                            }
                        }
                        return;
                    }
                    case 5:
                    default:
                        ui->screen = UI_SCREEN_HOME;
                        ui_render_home(ui);
                        return;
                }
            }
        }
        else if (ui->screen == UI_SCREEN_EDIT_PRICE || ui->screen == UI_SCREEN_EDIT_ADDR)
        {
            bool is_price = (ui->screen == UI_SCREEN_EDIT_PRICE);
            uint8_t max_len = is_price ? 4u : 2u;

            if (key == KEY_ESC)
            {
                ui->screen = UI_SCREEN_MENU;
                ui_render_menu(ui);
                return;
            }
            else if (key == '.' || key == KEY_RES)
            {
                if (ui->edit_len > 0u)
                {
                    ui->edit_len--;
                    ui->edit_buf[ui->edit_len] = 0;
                }
                ui_render_edit(ui, is_price);
                return;
            }
            else if (key == KEY_OK)
            {
                uint32_t v = ui_parse_u32(ui->edit_buf);
                uint8_t id = (uint8_t)(ui->edit_pump_index + 1u);

                if (is_price)
                {
                    if (v > 9999u) v = 9999u;
                    (void)PumpMgr_SetPrice(ui->mgr, id, v);
                    if (ui->settings)
                    {
                        (void)Settings_SetPumpPrice(ui->settings, ui->edit_pump_index, (uint16_t)v);
                    }
                    ui_toast(ui, "Price updated", 1200u);
                }
                else
                {
                    if (v < 1u) v = 1u;
                    if (v > 32u) v = 32u;
                    (void)PumpMgr_SetSlaveAddr(ui->mgr, id, (uint8_t)v);
                    if (ui->settings)
                    {
                        (void)Settings_SetPumpSlaveAddr(ui->settings, ui->edit_pump_index, (uint8_t)v);
                    }
                    ui_toast(ui, "Addr updated", 1200u);
                }

                if (ui->settings)
                {
                    if (Settings_RequestSave(ui->settings))
                    {
                        CDC_Log("UI: Settings save requested");
                        ui_toast(ui, "Saved to EEPROM", 1200u);
                    }
                    else
                    {
                        CDC_Log("UI: Settings save pending");
                    }
                }

                ui->screen = UI_SCREEN_MENU;
                ui_render_menu(ui);
                return;
            }
            else if (ui_is_digit(key))
            {
                if (ui->edit_len < max_len)
                {
                    ui->edit_buf[ui->edit_len++] = key;
                    ui->edit_buf[ui->edit_len] = 0;
                }
                ui_render_edit(ui, is_price);
                return;
            }
        }
    }

    if ((now - ui->last_render_ms) >= UI_RENDER_PERIOD_MS)
    {
        ui->last_render_ms = now;

        if (ui->screen == UI_SCREEN_TOTALIZER &&
            !ui->totalizer_data_valid &&
            (now - ui->totalizer_last_update_ms) > 500u)
        {
            ui->totalizer_last_update_ms = now;
            UI_RequestTotalizerUpdate(ui);
        }

        if (ui->screen == UI_SCREEN_HOME)
        {
            ui_render_home(ui);
        }
        else if (ui->screen == UI_SCREEN_TOTALIZER)
        {
            ui_render_totalizer(ui);
        }
        else if (ui->screen == UI_SCREEN_DISPENSE_MODE)
        {
            ui_render_dispense_mode(ui);
        }
        else if (ui->screen == UI_SCREEN_MENU)
        {
            ui_render_menu(ui);
        }
        else if (ui->screen == UI_SCREEN_EDIT_PRICE)
        {
            ui_render_edit(ui, true);
        }
        else if (ui->screen == UI_SCREEN_EDIT_ADDR)
        {
            ui_render_edit(ui, false);
        }
    }
}
