/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ui_transactions.c
  * @brief   Transaction UI screens implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "ui_transactions.h"
#include "pump_mgr_transactions.h"
#include "ssd1309.h"
#include <stdio.h>
#include <string.h>

/* Key definitions (from ui.c) */
#define KEY_TOT   ('A')
#define KEY_ESC   ('F')
#define KEY_SET   ('G')
#define KEY_INQ   ('H')
#define KEY_TIM   ('B')  /* CAL - TRK selection */
#define KEY_SEL   ('C')  /* Mode selection L/P/F */
#define KEY_PRI   ('D')  /* Reserved */
#define KEY_RES   ('E')  /* Reserved */
#define KEY_OK    ('K')

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

/* Format volume in dL as XX.XX L */
static void format_volume(uint32_t volume_dL, char *buf, size_t len)
{
    uint32_t liters = volume_dL / 10;
    uint32_t frac = volume_dL % 10;
    snprintf(buf, len, "%lu.%lu", (unsigned long)liters, (unsigned long)frac);
}

/* Format money without leading zeros */
static void format_money(uint32_t money, char *buf, size_t len)
{
    snprintf(buf, len, "%lu", (unsigned long)money);
}

/* ============================================================================
 * SELECT MODE SCREEN - Choose L/P/F
 * ========================================================================= */

void UI_Trans_RenderSelectMode(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: SELECT MODE", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show options */
    if (ui->dispense_mode == DISPENSE_MODE_VOLUME)
        ui_draw_line(2, "> L: LITERS");
    else
        ui_draw_line(2, "  L: LITERS");
        
    if (ui->dispense_mode == DISPENSE_MODE_MONEY)
        ui_draw_line(3, "> P: MONEY");
    else
        ui_draw_line(3, "  P: MONEY");
        
    if (ui->dispense_mode == DISPENSE_MODE_FULL)
        ui_draw_line(4, "> F: FULL TANK");
    else
        ui_draw_line(4, "  F: FULL TANK");
    
    ui_draw_line(6, "SEL:next OK:select");
    ui_draw_line(7, "ESC:back");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandleSelectMode(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    if (key == KEY_SEL)
    {
        /* Cycle through modes */
        if (ui->dispense_mode == DISPENSE_MODE_VOLUME)
            ui->dispense_mode = DISPENSE_MODE_MONEY;
        else if (ui->dispense_mode == DISPENSE_MODE_MONEY)
            ui->dispense_mode = DISPENSE_MODE_FULL;
        else
            ui->dispense_mode = DISPENSE_MODE_VOLUME;
        return true;
    }
    else if (key == KEY_OK)
    {
        /* Confirm selection */
        if (ui->dispense_mode == DISPENSE_MODE_VOLUME)
        {
            ui->screen = UI_SCREEN_PRESET_VOLUME;
            ui->preset_value = 0;
            ui->edit_len = 0;
            memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        }
        else if (ui->dispense_mode == DISPENSE_MODE_MONEY)
        {
            ui->screen = UI_SCREEN_PRESET_MONEY;
            ui->preset_value = 0;
            ui->edit_len = 0;
            memset(ui->edit_buf, 0, sizeof(ui->edit_buf));
        }
        else /* FULL TANK */
        {
            /* Calculate max volume based on price */
            const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, ui->active_pump_id);
            if (dev && dev->price > 0)
            {
                /* max_volume_dL = (999999 * 10) / price */
                ui->preset_value = (999999UL * 10UL) / dev->price;
                ui->preset_nozzle = 1; /* Default nozzle */
                
                /* Send preset and go to ARMED */
                PumpMgr_PresetVolume(ui->mgr, ui->active_pump_id, ui->preset_nozzle, ui->preset_value);
                ui->screen = UI_SCREEN_ARMED;
                ui->armed_time_ms = HAL_GetTick();
            }
        }
        return true;
    }
    else if (key == KEY_ESC)
    {
        ui->screen = UI_SCREEN_HOME;
        ui->dispense_mode = DISPENSE_MODE_IDLE;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * PRESET VOLUME SCREEN - Enter liters
 * ========================================================================= */

void UI_Trans_RenderPresetVolume(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: PRESET VOLUME", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show current input */
    char vol_str[16] = "0";
    if (ui->edit_len > 0)
    {
        strncpy(vol_str, ui->edit_buf, ui->edit_len);
        vol_str[ui->edit_len] = '\0';
    }
    
    snprintf(line, sizeof(line), "L: %s", vol_str);
    ui_draw_line(3, line);
    
    /* Show price for reference */
    const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, ui->active_pump_id);
    if (dev)
    {
        snprintf(line, sizeof(line), "Price: %lu", (unsigned long)dev->price);
        ui_draw_line(5, line);
    }
    
    ui_draw_line(6, "0-9:digit OK:start");
    ui_draw_line(7, "ESC:back");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandlePresetVolume(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    /* Handle digits */
    if (key >= '0' && key <= '9')
    {
        if (ui->edit_len < 6) /* Max 999.9 L */
        {
            ui->edit_buf[ui->edit_len++] = key;
            ui->edit_buf[ui->edit_len] = '\0';
        }
        return true;
    }
    else if (key == KEY_OK)
    {
        /* Parse input as XX.X format (convert to dL) */
        if (ui->edit_len > 0)
        {
            uint32_t value = 0;
            for (uint8_t i = 0; i < ui->edit_len; i++)
            {
                value = value * 10 + (ui->edit_buf[i] - '0');
            }
            
            /* Assume input is in liters, convert to dL */
            /* If user enters "25" we treat as 25.0L = 250 dL */
            ui->preset_value = value * 10;
            ui->preset_nozzle = 1; /* Default nozzle */
            
            /* Send preset */
            PumpMgr_PresetVolume(ui->mgr, ui->active_pump_id, ui->preset_nozzle, ui->preset_value);
            
            ui->screen = UI_SCREEN_ARMED;
            ui->armed_time_ms = HAL_GetTick();
        }
        return true;
    }
    else if (key == KEY_ESC)
    {
        ui->screen = UI_SCREEN_SELECT_MODE;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * PRESET MONEY SCREEN - Enter money amount
 * ========================================================================= */

void UI_Trans_RenderPresetMoney(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: PRESET MONEY", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show current input */
    char money_str[16] = "0";
    if (ui->edit_len > 0)
    {
        strncpy(money_str, ui->edit_buf, ui->edit_len);
        money_str[ui->edit_len] = '\0';
    }
    
    snprintf(line, sizeof(line), "P: %s", money_str);
    ui_draw_line(3, line);
    
    /* Show price for reference */
    const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, ui->active_pump_id);
    if (dev)
    {
        snprintf(line, sizeof(line), "Price: %lu", (unsigned long)dev->price);
        ui_draw_line(5, line);
    }
    
    ui_draw_line(6, "0-9:digit OK:start");
    ui_draw_line(7, "ESC:back");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandlePresetMoney(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    /* Handle digits */
    if (key >= '0' && key <= '9')
    {
        if (ui->edit_len < 6) /* Max 999999 */
        {
            ui->edit_buf[ui->edit_len++] = key;
            ui->edit_buf[ui->edit_len] = '\0';
        }
        return true;
    }
    else if (key == KEY_OK)
    {
        /* Parse input as money */
        if (ui->edit_len > 0)
        {
            uint32_t value = 0;
            for (uint8_t i = 0; i < ui->edit_len; i++)
            {
                value = value * 10 + (ui->edit_buf[i] - '0');
            }
            
            ui->preset_value = value;
            ui->preset_nozzle = 1; /* Default nozzle */
            
            /* Send preset */
            PumpMgr_PresetMoney(ui->mgr, ui->active_pump_id, ui->preset_nozzle, ui->preset_value);
            
            ui->screen = UI_SCREEN_ARMED;
            ui->armed_time_ms = HAL_GetTick();
        }
        return true;
    }
    else if (key == KEY_ESC)
    {
        ui->screen = UI_SCREEN_SELECT_MODE;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * ARMED SCREEN - Waiting for nozzle lift
 * ========================================================================= */

void UI_Trans_RenderArmed(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: ARMED", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show preset */
    if (ui->dispense_mode == DISPENSE_MODE_VOLUME || ui->dispense_mode == DISPENSE_MODE_FULL)
    {
        char vol_str[16];
        format_volume(ui->preset_value, vol_str, sizeof(vol_str));
        snprintf(line, sizeof(line), "Preset: %s L", vol_str);
        ui_draw_line(3, line);
    }
    else
    {
        snprintf(line, sizeof(line), "Preset: %lu", (unsigned long)ui->preset_value);
        ui_draw_line(3, line);
    }
    
    ui_draw_line(5, "Lift nozzle");
    ui_draw_line(6, "to start...");
    ui_draw_line(7, "ESC:cancel");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandleArmed(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    if (key == KEY_ESC)
    {
        /* Cancel transaction */
        PumpMgr_End(ui->mgr, ui->active_pump_id);
        ui->screen = UI_SCREEN_HOME;
        ui->dispense_mode = DISPENSE_MODE_IDLE;
        return true;
    }
    
    /* Check if fuelling started (status changed) */
    const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, ui->active_pump_id);
    if (dev && dev->status == 2) /* Fuelling */
    {
        ui->screen = UI_SCREEN_FUELLING;
        ui->fuelling_start_ms = HAL_GetTick();
        ui->last_poll_ms = 0;
    }
    
    return false;
}

/* TO BE CONTINUED IN PART 2... */

/* ============================================================================
 * FUELLING SCREEN - Dispensing in progress
 * ========================================================================= */

void UI_Trans_RenderFuelling(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: FUELLING", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show realtime volume/money */
    uint32_t rt_vol = PumpMgr_GetRealtimeVolume(ui->mgr, ui->active_pump_id);
    uint32_t rt_money = PumpMgr_GetRealtimeMoney(ui->mgr, ui->active_pump_id);
    
    char vol_str[16];
    format_volume(rt_vol, vol_str, sizeof(vol_str));
    snprintf(line, sizeof(line), "Vol: %s L", vol_str);
    ui_draw_line(3, line);
    
    snprintf(line, sizeof(line), "Sum: %lu", (unsigned long)rt_money);
    ui_draw_line(4, line);
    
    ui_draw_line(6, "RES:pause");
    ui_draw_line(7, "ESC:stop");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandleFuelling(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    if (key == KEY_RES)
    {
        /* Pause */
        PumpMgr_Stop(ui->mgr, ui->active_pump_id);
        ui->screen = UI_SCREEN_PAUSED;
        return true;
    }
    else if (key == KEY_ESC)
    {
        /* Stop and end */
        PumpMgr_End(ui->mgr, ui->active_pump_id);
        ui->screen = UI_SCREEN_COMPLETED;
        
        /* Read final transaction */
        PumpMgr_ReadTransaction(ui->mgr, ui->active_pump_id);
        return true;
    }
    
    /* Check if completed automatically */
    const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, ui->active_pump_id);
    if (dev && dev->status == 3) /* Completed */
    {
        ui->screen = UI_SCREEN_COMPLETED;
        PumpMgr_ReadTransaction(ui->mgr, ui->active_pump_id);
    }
    
    return false;
}

/* ============================================================================
 * PAUSED SCREEN
 * ========================================================================= */

void UI_Trans_RenderPaused(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: PAUSED", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show current values */
    uint32_t rt_vol = PumpMgr_GetRealtimeVolume(ui->mgr, ui->active_pump_id);
    uint32_t rt_money = PumpMgr_GetRealtimeMoney(ui->mgr, ui->active_pump_id);
    
    char vol_str[16];
    format_volume(rt_vol, vol_str, sizeof(vol_str));
    snprintf(line, sizeof(line), "Vol: %s L", vol_str);
    ui_draw_line(3, line);
    
    snprintf(line, sizeof(line), "Sum: %lu", (unsigned long)rt_money);
    ui_draw_line(4, line);
    
    ui_draw_line(6, "RES:resume OK:end");
    ui_draw_line(7, "ESC:cancel");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandlePaused(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    if (key == KEY_RES)
    {
        /* Resume */
        PumpMgr_Resume(ui->mgr, ui->active_pump_id);
        ui->screen = UI_SCREEN_FUELLING;
        return true;
    }
    else if (key == KEY_OK || key == KEY_ESC)
    {
        /* End transaction */
        PumpMgr_End(ui->mgr, ui->active_pump_id);
        ui->screen = UI_SCREEN_COMPLETED;
        PumpMgr_ReadTransaction(ui->mgr, ui->active_pump_id);
        return true;
    }
    
    return false;
}

/* ============================================================================
 * COMPLETED SCREEN
 * ========================================================================= */

void UI_Trans_RenderCompleted(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: COMPLETED", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show final transaction data */
    const PumpDevice *dev = PumpMgr_GetConst(ui->mgr, ui->active_pump_id);
    if (dev)
    {
        char vol_str[16];
        format_volume(dev->last_trx_volume_dL, vol_str, sizeof(vol_str));
        snprintf(line, sizeof(line), "Vol: %s L", vol_str);
        ui_draw_line(3, line);
        
        snprintf(line, sizeof(line), "Sum: %lu", (unsigned long)dev->last_trx_money);
        ui_draw_line(4, line);
        
        snprintf(line, sizeof(line), "Price: %u", dev->last_trx_price);
        ui_draw_line(5, line);
    }
    
    ui_draw_line(7, "OK:home");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandleCompleted(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    if (key == KEY_OK || key == KEY_ESC)
    {
        ui->screen = UI_SCREEN_HOME;
        ui->dispense_mode = DISPENSE_MODE_IDLE;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * TOTALIZER SCREEN
 * ========================================================================= */

void UI_Trans_RenderTotalizer(UI_Context *ui)
{
    if (ui == NULL) return;
    
    ui_clear_screen();
    
    char line[32];
    snprintf(line, sizeof(line), "TRK%u: TOTALIZER", ui->active_pump_id);
    ui_draw_line(0, line);
    ui_draw_line(1, "");
    
    /* Show totalizer 0 */
    uint32_t tot = PumpMgr_GetTotalizer(ui->mgr, ui->active_pump_id, 0);
    char vol_str[16];
    format_volume(tot, vol_str, sizeof(vol_str));
    snprintf(line, sizeof(line), "Total: %s L", vol_str);
    ui_draw_line(3, line);
    
    ui_draw_line(7, "ESC:back");
    
    SSD1309_UpdateScreen();
}

bool UI_Trans_HandleTotalizer(UI_Context *ui, char key)
{
    if (ui == NULL) return false;
    
    if (key == KEY_ESC || key == KEY_OK)
    {
        ui->screen = UI_SCREEN_HOME;
        return true;
    }
    
    return false;
}

/* ============================================================================
 * BACKGROUND TASK - Realtime polling
 * ========================================================================= */

void UI_Trans_Task(UI_Context *ui)
{
    if (ui == NULL) return;
    
    uint32_t now = HAL_GetTick();
    
    /* Poll realtime data during fuelling */
    if (ui->screen == UI_SCREEN_FUELLING || ui->screen == UI_SCREEN_PAUSED)
    {
        if (now - ui->last_poll_ms >= 500) /* Poll every 500ms */
        {
            ui->last_poll_ms = now;
            
            /* Poll both volume and money */
            PumpMgr_PollRealtimeVolume(ui->mgr, ui->active_pump_id, ui->preset_nozzle);
            PumpMgr_PollRealtimeMoney(ui->mgr, ui->active_pump_id, ui->preset_nozzle);
        }
    }
    
    /* Read totalizer on TOTALIZER screen */
    if (ui->screen == UI_SCREEN_TOTALIZER)
    {
        if (now - ui->last_poll_ms >= 2000) /* Poll every 2s */
        {
            ui->last_poll_ms = now;
            PumpMgr_ReadTotalizer(ui->mgr, ui->active_pump_id, 0);
        }
    }
}
