/* ui.c - Refactored with FSM */
#include "ui.h"
#include "ssd1309.h"
#include "pump_transactions.h"
#include "stm32h7xx_hal.h"
#include <stdio.h>
#include <string.h>

#define KEY_TOT   ('A')
#define KEY_TIM   ('B')
#define KEY_SEL   ('C')
#define KEY_PRI   ('D')
#define KEY_RES   ('E')
#define KEY_ESC   ('F')
#define KEY_OK    ('K')

static void ui_clear(void) { SSD1309_Fill(0); }

static void ui_line(uint8_t row, const char *text)
{
    SSD1309_SetCursor(0, row * 8);
    SSD1309_WriteString(text, 1);
}

static void format_volume(uint32_t volume_dL, char *buf, size_t len)
{
    uint32_t liters = volume_dL / 10;
    uint32_t frac = volume_dL % 10;
    snprintf(buf, len, "%03lu.%01lu", (unsigned long)liters, (unsigned long)frac);
}

static TransactionFSM* ui_get_fsm(UI_Context *ui)
{
    return (ui->active_pump_id == 1) ? ui->trk1_fsm : ui->trk2_fsm;
}

/* ========== HOME SCREEN ========== */
static void ui_render_home(UI_Context *ui)
{
    ui_clear();
    
    char line[32];
    uint8_t row = 0;
    
    for (uint8_t i = 0; i < 2; i++) {
        TransactionFSM *fsm = (i == 0) ? ui->trk1_fsm : ui->trk2_fsm;
        if (!fsm) continue;
        
        uint8_t trk_id = i + 1;
        char sel = (ui->active_pump_id == trk_id) ? '>' : ' ';
        char pause = ' ';
        char active = ' ';
        
        TrxState state = TrxFSM_GetState(fsm);
        
        if (state == TRX_COMPLETE) {
            active = ui->blink_state ? '!' : ' ';
            pause = '*';
        } else if (state == TRX_PAUSED) {
            pause = 'P';
            active = '*';
        } else if (state >= TRX_ARMED) {
            active = '*';
        }
        
        if (state != TRX_IDLE) {
            const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, trk_id);
            uint16_t price = dev ? dev->price : 0;
            
            snprintf(line, sizeof(line), "%c%c%cTRK%u: P%04u", sel, pause, active, trk_id, price);
            ui_line(row++, line);
            
            char vol_str[12];
            format_volume(TrxFSM_GetRealtimeVolume(fsm), vol_str, sizeof(vol_str));
            snprintf(line, sizeof(line), "  L: %s", vol_str);
            ui_line(row++, line);
            
            snprintf(line, sizeof(line), "  P: %06lu", (unsigned long)TrxFSM_GetRealtimeMoney(fsm));
            ui_line(row++, line);
        } else {
            snprintf(line, sizeof(line), "%cTRK%u", sel, trk_id);
            ui_line(row++, line);
        }
    }
    
    SSD1309_UpdateScreen();
}

static bool ui_handle_home(UI_Context *ui, char key)
{
    if (key == KEY_TIM) {
        ui->active_pump_id = 1;
        return true;
    }
    else if (key == KEY_SEL) {
        ui->active_pump_id = 2;
        return true;
    }
    else if (key == KEY_PRI) {
        ui->selected_mode = 0;
        ui->screen = UI_SCREEN_SELECT_MODE;
        return true;
    }
    else if (key == KEY_TOT) {
        ui->screen = UI_SCREEN_TOTALIZER;
        
        /* Request totalizers */
        if (ui->trk1_fsm && ui->trk1_fsm->gkl) {
            const PumpDevice *dev = PumpMgr_GetConst(ui->trk1_fsm->mgr, 1);
            if (dev && ui->trk1_fsm->gkl->link.state == GKL_STATE_IDLE) {
                PumpTrans_ReadTotalizer(ui->trk1_fsm->gkl, dev->ctrl_addr, dev->slave_addr, 0);
            }
        }
        if (ui->trk2_fsm && ui->trk2_fsm->gkl) {
            const PumpDevice *dev = PumpMgr_GetConst(ui->trk2_fsm->mgr, 2);
            if (dev && ui->trk2_fsm->gkl->link.state == GKL_STATE_IDLE) {
                PumpTrans_ReadTotalizer(ui->trk2_fsm->gkl, dev->ctrl_addr, dev->slave_addr, 0);
            }
        }
        return true;
    }
    else if (key == KEY_RES) {
        TransactionFSM *fsm = ui_get_fsm(ui);
        if (fsm) {
            TrxState state = TrxFSM_GetState(fsm);
            if (state == TRX_DISPENSING) {
                TrxFSM_Pause(fsm);
            }
            else if (state == TRX_PAUSED || state == TRX_COMPLETE || 
                     state == TRX_ARMED || state == TRX_PRESET_SENT) {
                TrxFSM_Cancel(fsm);
            }
        }
        return true;
    }
    else if (key == KEY_OK) {
        TransactionFSM *fsm = ui_get_fsm(ui);
        if (fsm && TrxFSM_GetState(fsm) == TRX_PAUSED) {
            TrxFSM_Resume(fsm);
        }
        return true;
    }
    return false;
}

/* ========== SELECT MODE ========== */
static void ui_render_select_mode(UI_Context *ui)
{
    ui_clear();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: MODE", ui->active_pump_id);
    ui_line(0, line);
    ui_line(1, "");
    ui_line(2, (ui->selected_mode == 0) ? ">L: LITERS" : " L: LITERS");
    ui_line(3, (ui->selected_mode == 1) ? ">P: MONEY"  : " P: MONEY");
    ui_line(4, (ui->selected_mode == 2) ? ">F: FULL"   : " F: FULL");
    ui_line(6, "SEL:next");
    ui_line(7, "OK:ok ESC:back");
    
    SSD1309_UpdateScreen();
}

static bool ui_handle_select_mode(UI_Context *ui, char key)
{
    if (key == KEY_SEL || key == KEY_PRI) {
        ui->selected_mode = (ui->selected_mode + 1) % 3;
        return true;
    }
    else if (key == KEY_OK) {
        if (ui->selected_mode == 0) {
            ui->screen = UI_SCREEN_PRESET_VOLUME;
            ui->edit_len = 0;
            memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        } else if (ui->selected_mode == 1) {
            ui->screen = UI_SCREEN_PRESET_MONEY;
            ui->edit_len = 0;
            memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        } else {
            /* Full tank */
            TransactionFSM *fsm = ui_get_fsm(ui);
            if (fsm) {
                const PumpDevice *dev = PumpMgr_GetConst(fsm->mgr, fsm->pump_id);
                if (dev) {
                    uint32_t max_volume_dL = (999999UL * 10UL) / dev->price;
                    TrxFSM_StartVolume(fsm, max_volume_dL);
                }
            }
            ui->screen = UI_SCREEN_HOME;
        }
        return true;
    }
    else if (key == KEY_ESC) {
        ui->screen = UI_SCREEN_HOME;
        return true;
    }
    return false;
}

/* ========== PRESET VOLUME ========== */
static void ui_render_preset_volume(UI_Context *ui)
{
    ui_clear();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: VOLUME", ui->active_pump_id);
    ui_line(0, line);
    ui_line(1, "");
    
    char vol_str[8] = "0";
    if (ui->edit_len > 0) {
        strncpy(vol_str, ui->edit_buf, ui->edit_len);
        vol_str[ui->edit_len] = '\0';
    }
    
    snprintf(line, sizeof(line), "L: %s", vol_str);
    ui_line(3, line);
    ui_line(6, "0-9,.:digit");
    ui_line(7, "OK:start RES:clr");
    
    SSD1309_UpdateScreen();
}

static bool ui_handle_preset_volume(UI_Context *ui, char key)
{
    if (key >= '0' && key <= '9') {
        if (ui->edit_len < 6) {
            ui->edit_buf[ui->edit_len++] = key;
            ui->edit_buf[ui->edit_len] = '\0';
        }
        return true;
    }
    else if (key == '.') {
        if (ui->edit_len < 6) {
            bool has_dot = false;
            for (uint8_t i = 0; i < ui->edit_len; i++) {
                if (ui->edit_buf[i] == '.') { has_dot = true; break; }
            }
            if (!has_dot) {
                ui->edit_buf[ui->edit_len++] = '.';
                ui->edit_buf[ui->edit_len] = '\0';
            }
        }
        return true;
    }
    else if (key == KEY_RES) {
        ui->edit_len = 0;
        memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        return true;
    }
    else if (key == KEY_OK) {
        if (ui->edit_len > 0) {
            /* Parse: "25.5" → 255 dL */
            uint32_t int_part = 0, frac_part = 0;
            bool after_dot = false;
            uint8_t frac_digits = 0;
            
            for (uint8_t i = 0; i < ui->edit_len; i++) {
                if (ui->edit_buf[i] == '.') {
                    after_dot = true;
                } else if (!after_dot) {
                    int_part = int_part * 10 + (ui->edit_buf[i] - '0');
                } else {
                    frac_part = frac_part * 10 + (ui->edit_buf[i] - '0');
                    frac_digits++;
                }
            }
            
            if (frac_digits > 1) frac_part /= 10;
            uint32_t volume_dL = int_part * 10 + frac_part;
            
            TransactionFSM *fsm = ui_get_fsm(ui);
            if (fsm) {
                TrxFSM_StartVolume(fsm, volume_dL);
            }
            
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

/* ========== PRESET MONEY ========== */
static void ui_render_preset_money(UI_Context *ui)
{
    ui_clear();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: MONEY", ui->active_pump_id);
    ui_line(0, line);
    ui_line(1, "");
    
    char money_str[8] = "0";
    if (ui->edit_len > 0) {
        strncpy(money_str, ui->edit_buf, ui->edit_len);
        money_str[ui->edit_len] = '\0';
    }
    
    snprintf(line, sizeof(line), "P: %s", money_str);
    ui_line(3, line);
    ui_line(6, "0-9:digit");
    ui_line(7, "OK:start RES:clr");
    
    SSD1309_UpdateScreen();
}

static bool ui_handle_preset_money(UI_Context *ui, char key)
{
    if (key >= '0' && key <= '9') {
        if (ui->edit_len < 6) {
            ui->edit_buf[ui->edit_len++] = key;
            ui->edit_buf[ui->edit_len] = '\0';
        }
        return true;
    }
    else if (key == KEY_RES) {
        ui->edit_len = 0;
        memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        return true;
    }
    else if (key == KEY_OK) {
        if (ui->edit_len > 0) {
            uint32_t value = 0;
            for (uint8_t i = 0; i < ui->edit_len; i++) {
                value = value * 10 + (ui->edit_buf[i] - '0');
            }
            
            TransactionFSM *fsm = ui_get_fsm(ui);
            if (fsm) {
                TrxFSM_StartMoney(fsm, value);
            }
            
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

/* ========== TOTALIZER ========== */
static void ui_render_totalizer(UI_Context *ui)
{
    ui_clear();
    
    ui_line(0, "TOTALIZERS");
    ui_line(1, "");
    
    for (uint8_t i = 0; i < 2; i++) {
        TransactionFSM *fsm = (i == 0) ? ui->trk1_fsm : ui->trk2_fsm;
        if (!fsm) continue;
        
        char line[32], vol_str[12];
        format_volume(fsm->totalizer_dL, vol_str, sizeof(vol_str));
        snprintf(line, sizeof(line), "TRK%u: %s", i+1, vol_str);
        ui_line(2 + i * 2, line);
    }
    
    ui_line(7, "ESC:back");
    SSD1309_UpdateScreen();
}

static bool ui_handle_totalizer(UI_Context *ui, char key)
{
    if (key == KEY_ESC || key == KEY_OK) {
        ui->screen = UI_SCREEN_HOME;
        return true;
    }
    return false;
}

/* ========== INIT & TASK ========== */
void UI_Init(UI_Context *ui, TransactionFSM *trk1_fsm, TransactionFSM *trk2_fsm, Settings *settings)
{
    if (!ui) return;
    
    memset(ui, 0, sizeof(*ui));
    ui->trk1_fsm = trk1_fsm;
    ui->trk2_fsm = trk2_fsm;
    ui->settings = settings;
    ui->screen = UI_SCREEN_HOME;
    ui->active_pump_id = 1;
    ui->selected_mode = 0;
    ui->blink_timer_ms = HAL_GetTick();
}

void UI_Task(UI_Context *ui, char key)
{
    if (!ui) return;
    
    uint32_t now = HAL_GetTick();
    
    /* Blink timer */
    if ((now - ui->blink_timer_ms) > 500) {
        ui->blink_state = !ui->blink_state;
        ui->blink_timer_ms = now;
    }
    
    /* Handle input */
    bool need_render = false;
    
    if (key != 0) {
        if (ui->screen == UI_SCREEN_HOME) {
            need_render = ui_handle_home(ui, key);
        }
        else if (ui->screen == UI_SCREEN_SELECT_MODE) {
            need_render = ui_handle_select_mode(ui, key);
        }
        else if (ui->screen == UI_SCREEN_PRESET_VOLUME) {
            need_render = ui_handle_preset_volume(ui, key);
        }
        else if (ui->screen == UI_SCREEN_PRESET_MONEY) {
            need_render = ui_handle_preset_money(ui, key);
        }
        else if (ui->screen == UI_SCREEN_TOTALIZER) {
            need_render = ui_handle_totalizer(ui, key);
        }
    }
    
    /* Periodic render */
    if (need_render || (now - ui->last_render_ms) >= 100) {
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
