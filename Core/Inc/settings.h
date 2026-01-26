/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    settings.h
  * @brief   Persistent settings stored in external I2C EEPROM with CRC.
  *
  * Goals:
  *  - Data integrity: magic + version + payload length + CRC32.
  *  - Two-slot scheme (A/B) with sequence counter.
  *  - EEPROM write is asynchronous (state machine) to avoid long blocking.
  *
  * Notes:
  *  - SLOT_SIZE is 128 bytes -> fits even into 24C02 (256B) as 2 slots.
  *  - If you use bigger EEPROM - it still works.
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef SETTINGS_H
#define SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"
#include "pump_mgr.h"
#include <stdint.h>
#include <stdbool.h>

/* ========== EEPROM configuration (adjust if needed) ========== */

/* HAL expects 8-bit address (7-bit << 1). For 24xx EEPROM typical: 0x50<<1 = 0xA0 */
#ifndef SETTINGS_EEPROM_I2C_ADDR
#define SETTINGS_EEPROM_I2C_ADDR         ((0x50u << 1) /* 0xA0 when A2..A0=0 */)
#endif

/* I2C mem address size: 8-bit for 24C02, 16-bit for 24C32/64/256 etc.
   Our default layout works for both (slot addresses are < 256). */
#ifndef SETTINGS_EEPROM_MEMADD_SIZE
#define SETTINGS_EEPROM_MEMADD_SIZE      (I2C_MEMADD_SIZE_16BIT)
#endif

/* Safe universal page write size (smaller works on any 24xx). */
#ifndef SETTINGS_EEPROM_PAGE_SIZE
#define SETTINGS_EEPROM_PAGE_SIZE        (64u)  /* AT24C256 page = 64 bytes */
#endif

/* ========== Record format ========== */
#define SETTINGS_MAGIC                   (0x53455431u) /* 'SET1' */
#define SETTINGS_VERSION                 (1u)

#define SETTINGS_SLOT_SIZE               (128u)
#define SETTINGS_SLOT0_ADDR              (0x0000u)
#define SETTINGS_SLOT1_ADDR              (0x0080u)

/* Max pump settings stored (matches PumpMgr capacity) */
#define SETTINGS_MAX_PUMPS               (PUMP_MAX_DEVICES)

typedef enum
{
    SETTINGS_SAVE_IDLE = 0,
    SETTINGS_SAVE_BUSY,
    SETTINGS_SAVE_OK,
    SETTINGS_SAVE_ERROR
} SettingsSaveState;

typedef struct
{
    uint8_t  ctrl_addr;
    uint8_t  slave_addr;
    uint16_t price;      /* 0..9999 (decimal), UI uses 4 digits */
} SettingsPump;

typedef struct
{
    uint8_t pump_count; /* how many entries are valid in pump[] */
    SettingsPump pump[SETTINGS_MAX_PUMPS];
} SettingsData;

typedef struct
{
    I2C_HandleTypeDef *hi2c;

    SettingsData data;

    uint32_t seq;
    uint8_t  last_slot;     /* 0 or 1 (which slot is the newest loaded/saved) */

    volatile SettingsSaveState save_state;
    volatile uint8_t save_error;

    /* Async write state */
    volatile uint8_t wr_active;
    volatile uint8_t wr_inflight;
    volatile uint8_t wr_wait_ready;

    uint16_t wr_base;
    uint16_t wr_off;
    uint16_t wr_len;
    uint16_t wr_chunk;

    uint32_t wr_ready_start_ms;

    uint8_t wr_buf[SETTINGS_SLOT_SIZE];
} Settings;

void Settings_Init(Settings *s, I2C_HandleTypeDef *hi2c);

/**
 * @brief Load settings from EEPROM (valid slot with highest seq).
 * @return true if loaded, false if defaults were used.
 */
bool Settings_Load(Settings *s);

/**
 * @brief Apply defaults (does not write to EEPROM).
 */
void Settings_Defaults(Settings *s);

/**
 * @brief Start asynchronous save to EEPROM (A/B slot).
 * @note  Call Settings_Task() frequently until state becomes OK/ERROR.
 * @return true if started.
 */
bool Settings_RequestSave(Settings *s);

/**
 * @brief Must be called often from main loop (does not block long).
 */
void Settings_Task(Settings *s);

SettingsSaveState Settings_GetSaveState(const Settings *s);
uint8_t Settings_GetSaveError(const Settings *s);

/* Helpers to sync with runtime model */
void Settings_ApplyToPumpMgr(const Settings *s, PumpMgr *m);
void Settings_CaptureFromPumpMgr(Settings *s, const PumpMgr *m);

/* Convenient setters (also clamp ranges) */
bool Settings_SetPumpPrice(Settings *s, uint8_t pump_index, uint16_t price);
bool Settings_SetPumpSlaveAddr(Settings *s, uint8_t pump_index, uint8_t slave_addr);
bool Settings_SetPumpCtrlAddr(Settings *s, uint8_t pump_index, uint8_t ctrl_addr);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
