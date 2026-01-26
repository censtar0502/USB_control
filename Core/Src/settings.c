/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    settings.c
  * @brief   Persistent settings stored in external I2C EEPROM with CRC.
  *
  * EEPROM layout (256B minimal):
  *  - Slot A at 0x00..0x7F
  *  - Slot B at 0x80..0xFF
  *
  * Record header:
  *  [0]  u32 magic   = 'SET1'
  *  [4]  u16 version
  *  [6]  u16 payload_len
  *  [8]  u32 seq
  *  [12] u32 crc32(payload)
  *  [16] payload bytes...
  *  rest filled with 0xFF
  *
  * Payload format (compact):
  *  [0] pump_count
  *  for each pump i:
  *    ctrl_addr (1)
  *    slave_addr (1)
  *    price_le16 (2)
  *
  * Async save: writes pages with HAL_I2C_Mem_Write_IT and waits EEPROM internal
  * write cycle using lightweight HAL_I2C_IsDeviceReady(..., trials=1, timeout=1).
  ******************************************************************************
  */
/* USER CODE END Header */

#include "settings.h"
#include <string.h>

/* Singleton pointer for HAL I2C callbacks dispatching */
static Settings *s_settings_singleton = NULL;

/* ---------------- CRC32 (standard, little-endian) ---------------- */
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint32_t len)
{
    uint32_t c = crc;
    for (uint32_t i = 0; i < len; i++)
    {
        c ^= (uint32_t)data[i];
        for (uint8_t b = 0; b < 8; b++)
        {
            if (c & 1u) c = (c >> 1) ^ 0xEDB88320u;
            else        c = (c >> 1);
        }
    }
    return c;
}

static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    crc = crc32_update(crc, data, len);
    return crc ^ 0xFFFFFFFFu;
}

/* ---------------- Little-endian helpers ---------------- */
static uint16_t rd_u16_le(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void wr_u16_le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static uint32_t rd_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static void wr_u32_le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

/* ---------------- EEPROM blocking read (startup only) ---------------- */
static bool eeprom_read_block(I2C_HandleTypeDef *hi2c, uint16_t mem_addr, uint8_t *dst, uint16_t len)
{
    if (hi2c == NULL || dst == NULL || len == 0u) return false;

    /* Timeout: small but enough for 128 bytes at 100 kHz */
    if (HAL_I2C_Mem_Read(hi2c,
                         SETTINGS_EEPROM_I2C_ADDR,
                         mem_addr,
                         SETTINGS_EEPROM_MEMADD_SIZE,
                         dst,
                         len,
                         50u) == HAL_OK)
    {
        return true;
    }
    return false;
}

/* ---------------- Record parse/build ---------------- */
static bool parse_slot(const uint8_t *slot, SettingsData *out, uint32_t *out_seq)
{
    if (slot == NULL || out == NULL || out_seq == NULL) return false;

    uint32_t magic = rd_u32_le(&slot[0]);
    uint16_t ver   = rd_u16_le(&slot[4]);
    uint16_t plen  = rd_u16_le(&slot[6]);
    uint32_t seq   = rd_u32_le(&slot[8]);
    uint32_t crc_s = rd_u32_le(&slot[12]);

    if (magic != SETTINGS_MAGIC) return false;
    if (ver != (uint16_t)SETTINGS_VERSION) return false;
    if (plen == 0u) return false;
    if ((uint32_t)(16u + plen) > (uint32_t)SETTINGS_SLOT_SIZE) return false;

    const uint8_t *payload = &slot[16];
    uint32_t crc_c = crc32_calc(payload, (uint32_t)plen);
    if (crc_c != crc_s) return false;

    /* Decode payload */
    memset(out, 0, sizeof(*out));

    uint8_t pump_count = payload[0];
    if (pump_count == 0u) return false;
    if (pump_count > (uint8_t)SETTINGS_MAX_PUMPS) pump_count = (uint8_t)SETTINGS_MAX_PUMPS;

    uint32_t needed = 1u + (uint32_t)pump_count * 4u;
    if ((uint32_t)plen < needed) return false;

    out->pump_count = pump_count;
    uint32_t off = 1u;
    for (uint8_t i = 0; i < pump_count; i++)
    {
        out->pump[i].ctrl_addr  = payload[off + 0u];
        out->pump[i].slave_addr = payload[off + 1u];
        out->pump[i].price      = rd_u16_le(&payload[off + 2u]);
        off += 4u;
    }

    *out_seq = seq;
    return true;
}

static void build_slot_image(const Settings *s, uint32_t seq, uint8_t *slot_out)
{
    memset(slot_out, 0xFF, SETTINGS_SLOT_SIZE);

    /* Build payload */
    uint8_t payload[SETTINGS_SLOT_SIZE - 16u];
    memset(payload, 0, sizeof(payload));

    uint8_t count = s->data.pump_count;
    if (count == 0u) count = 1u;
    if (count > (uint8_t)SETTINGS_MAX_PUMPS) count = (uint8_t)SETTINGS_MAX_PUMPS;

    payload[0] = count;
    uint32_t poff = 1u;

    for (uint8_t i = 0; i < count; i++)
    {
        payload[poff + 0u] = s->data.pump[i].ctrl_addr;
        payload[poff + 1u] = s->data.pump[i].slave_addr;
        wr_u16_le(&payload[poff + 2u], s->data.pump[i].price);
        poff += 4u;
    }

    uint16_t payload_len = (uint16_t)poff;
    uint32_t crc = crc32_calc(payload, payload_len);

    /* Header */
    wr_u32_le(&slot_out[0], SETTINGS_MAGIC);
    wr_u16_le(&slot_out[4], (uint16_t)SETTINGS_VERSION);
    wr_u16_le(&slot_out[6], payload_len);
    wr_u32_le(&slot_out[8], seq);
    wr_u32_le(&slot_out[12], crc);

    memcpy(&slot_out[16], payload, payload_len);
}

static void clamp_data(SettingsData *d)
{
    if (d == NULL) return;

    if (d->pump_count == 0u) d->pump_count = 1u;
    if (d->pump_count > (uint8_t)SETTINGS_MAX_PUMPS) d->pump_count = (uint8_t)SETTINGS_MAX_PUMPS;

    for (uint8_t i = 0; i < d->pump_count; i++)
    {
        if (d->pump[i].slave_addr == 0u) d->pump[i].slave_addr = 1u;
        if (d->pump[i].slave_addr > 32u) d->pump[i].slave_addr = 32u;
        if (d->pump[i].price > 9999u) d->pump[i].price = 9999u;
    }
}

/* ---------------- Public API ---------------- */

void Settings_Defaults(Settings *s)
{
    if (s == NULL) return;

    memset(&s->data, 0, sizeof(s->data));
    s->data.pump_count = 2u;

    /* TRK1 default */
    s->data.pump[0].ctrl_addr  = 0x00u;
    s->data.pump[0].slave_addr = 0x01u;
    s->data.pump[0].price      = 0u;

    /* TRK2 default (different address for debug) */
    s->data.pump[1].ctrl_addr  = 0x00u;
    s->data.pump[1].slave_addr = 0x02u;
    s->data.pump[1].price      = 0u;

    s->seq = 0u;
    s->last_slot = 0u;

    s->save_state = SETTINGS_SAVE_IDLE;
    s->save_error = 0u;

    s->wr_active = 0u;
    s->wr_inflight = 0u;
    s->wr_wait_ready = 0u;

    clamp_data(&s->data);
}

void Settings_Init(Settings *s, I2C_HandleTypeDef *hi2c)
{
    if (s == NULL) return;
    memset(s, 0, sizeof(*s));

    s->hi2c = hi2c;
    s_settings_singleton = s;

    Settings_Defaults(s);
}

bool Settings_Load(Settings *s)
{
    if (s == NULL || s->hi2c == NULL) return false;

    uint8_t slot0[SETTINGS_SLOT_SIZE];
    uint8_t slot1[SETTINGS_SLOT_SIZE];

    bool r0 = eeprom_read_block(s->hi2c, (uint16_t)SETTINGS_SLOT0_ADDR, slot0, SETTINGS_SLOT_SIZE);
    bool r1 = eeprom_read_block(s->hi2c, (uint16_t)SETTINGS_SLOT1_ADDR, slot1, SETTINGS_SLOT_SIZE);

    SettingsData d0, d1;
    uint32_t seq0 = 0u, seq1 = 0u;

    bool v0 = false;
    bool v1 = false;

    if (r0) v0 = parse_slot(slot0, &d0, &seq0);
    if (r1) v1 = parse_slot(slot1, &d1, &seq1);

    if (!v0 && !v1)
    {
        Settings_Defaults(s);
        return false;
    }

    if (v0 && (!v1 || (seq0 >= seq1)))
    {
        s->data = d0;
        s->seq = seq0;
        s->last_slot = 0u;
    }
    else
    {
        s->data = d1;
        s->seq = seq1;
        s->last_slot = 1u;
    }

    clamp_data(&s->data);
    s->save_state = SETTINGS_SAVE_IDLE;
    s->save_error = 0u;
    s->wr_active = 0u;
    s->wr_inflight = 0u;
    s->wr_wait_ready = 0u;

    return true;
}

SettingsSaveState Settings_GetSaveState(const Settings *s)
{
    if (s == NULL) return SETTINGS_SAVE_ERROR;
    return s->save_state;
}

uint8_t Settings_GetSaveError(const Settings *s)
{
    if (s == NULL) return 0xFFu;
    return s->save_error;
}

bool Settings_RequestSave(Settings *s)
{
    if (s == NULL || s->hi2c == NULL) return false;
    if (s->wr_active) return false;

    clamp_data(&s->data);

    uint8_t next_slot = (s->last_slot == 0u) ? 1u : 0u;
    uint16_t base = (next_slot == 0u) ? (uint16_t)SETTINGS_SLOT0_ADDR : (uint16_t)SETTINGS_SLOT1_ADDR;

    uint32_t new_seq = s->seq + 1u;
    build_slot_image(s, new_seq, s->wr_buf);

    s->wr_active = 1u;
    s->wr_inflight = 0u;
    s->wr_wait_ready = 0u;

    s->wr_base = base;
    s->wr_off = 0u;
    s->wr_len = SETTINGS_SLOT_SIZE;
    s->wr_chunk = 0u;

    s->wr_ready_start_ms = 0u;

    s->save_state = SETTINGS_SAVE_BUSY;
    s->save_error = 0u;

    /* Temporarily store planned slot in save_error high bit (internal), finalized on success */
    s->save_error = (uint8_t)(next_slot & 0x01u);

    return true;
}

static uint16_t min_u16(uint16_t a, uint16_t b)
{
    return (a < b) ? a : b;
}

void Settings_Task(Settings *s)
{
    if (s == NULL || s->hi2c == NULL) return;
    if (!s->wr_active) return;

    uint32_t now = HAL_GetTick();

    /* 1) Wait for HAL TX complete callback */
    if (s->wr_inflight)
    {
        return;
    }

    /* 2) Wait EEPROM internal write cycle (ACK polling) */
    if (s->wr_wait_ready)
    {
        if (s->wr_ready_start_ms == 0u)
        {
            s->wr_ready_start_ms = now;
        }

        /* Lightweight poll: 1 trial, 1 ms timeout */
        if (HAL_I2C_IsDeviceReady(s->hi2c, SETTINGS_EEPROM_I2C_ADDR, 1u, 2u) == HAL_OK)
        {
            s->wr_wait_ready = 0u;
            s->wr_ready_start_ms = 0u;
        }
        else
        {
            /* Give EEPROM some time (typical write cycle up to 5 ms) */
            if ((now - s->wr_ready_start_ms) > 50u)
            {
                s->wr_active = 0u;
                s->wr_wait_ready = 0u;
                s->save_state = SETTINGS_SAVE_ERROR;
                s->save_error = 1u; /* ready timeout */
            }
            return;
        }
    }

    /* 3) Done? */
    if (s->wr_off >= s->wr_len)
    {
        /* Success */
        uint8_t next_slot = (uint8_t)(s->save_error & 0x01u);
        s->last_slot = next_slot;
        s->seq = s->seq + 1u;

        s->wr_active = 0u;
        s->save_state = SETTINGS_SAVE_OK;
        s->save_error = 0u;
        return;
    }

    /* 4) Start next page write */
    uint16_t abs_addr = (uint16_t)(s->wr_base + s->wr_off);

    uint16_t remaining = (uint16_t)(s->wr_len - s->wr_off);

    uint16_t page_off = (uint16_t)(abs_addr % (uint16_t)SETTINGS_EEPROM_PAGE_SIZE);
    uint16_t page_rem = (uint16_t)((uint16_t)SETTINGS_EEPROM_PAGE_SIZE - page_off);

    uint16_t chunk = min_u16(page_rem, remaining);
    if (chunk == 0u) chunk = remaining;

    s->wr_chunk = chunk;

    if (HAL_I2C_Mem_Write_IT(s->hi2c,
                            SETTINGS_EEPROM_I2C_ADDR,
                            abs_addr,
                            SETTINGS_EEPROM_MEMADD_SIZE,
                            (uint8_t*)&s->wr_buf[s->wr_off],
                            chunk) != HAL_OK)
    {
        s->wr_active = 0u;
        s->save_state = SETTINGS_SAVE_ERROR;
        s->save_error = 2u; /* HAL write start error */
        return;
    }

    s->wr_inflight = 1u;
}

/* ---------------- Sync helpers ---------------- */

void Settings_ApplyToPumpMgr(const Settings *s, PumpMgr *m)
{
    if (s == NULL || m == NULL) return;

    uint8_t count = s->data.pump_count;
    if (count > m->count) count = m->count;

    for (uint8_t i = 0u; i < count; i++)
    {
        uint8_t id = (uint8_t)(i + 1u);
        (void)PumpMgr_SetCtrlAddr(m, id, s->data.pump[i].ctrl_addr);
        (void)PumpMgr_SetSlaveAddr(m, id, s->data.pump[i].slave_addr);
        (void)PumpMgr_SetPrice(m, id, (uint32_t)s->data.pump[i].price);
    }
}

void Settings_CaptureFromPumpMgr(Settings *s, const PumpMgr *m)
{
    if (s == NULL || m == NULL) return;

    uint8_t count = m->count;
    if (count == 0u) count = 1u;
    if (count > (uint8_t)SETTINGS_MAX_PUMPS) count = (uint8_t)SETTINGS_MAX_PUMPS;

    s->data.pump_count = count;

    for (uint8_t i = 0u; i < count; i++)
    {
        uint8_t id = (uint8_t)(i + 1u);
        s->data.pump[i].ctrl_addr  = PumpMgr_GetCtrlAddr(m, id);
        s->data.pump[i].slave_addr = PumpMgr_GetSlaveAddr(m, id);
        uint32_t pr = PumpMgr_GetPrice(m, id);
        if (pr > 9999u) pr = 9999u;
        s->data.pump[i].price = (uint16_t)pr;
    }

    clamp_data(&s->data);
}

bool Settings_SetPumpPrice(Settings *s, uint8_t pump_index, uint16_t price)
{
    if (s == NULL) return false;
    if (pump_index >= s->data.pump_count) return false;
    if (price > 9999u) price = 9999u;

    s->data.pump[pump_index].price = price;
    return true;
}

bool Settings_SetPumpSlaveAddr(Settings *s, uint8_t pump_index, uint8_t slave_addr)
{
    if (s == NULL) return false;
    if (pump_index >= s->data.pump_count) return false;

    if (slave_addr < 1u) slave_addr = 1u;
    if (slave_addr > 32u) slave_addr = 32u;

    s->data.pump[pump_index].slave_addr = slave_addr;
    return true;
}

bool Settings_SetPumpCtrlAddr(Settings *s, uint8_t pump_index, uint8_t ctrl_addr)
{
    if (s == NULL) return false;
    if (pump_index >= s->data.pump_count) return false;

    s->data.pump[pump_index].ctrl_addr = ctrl_addr;
    return true;
}

/* ---------------- HAL I2C callbacks ---------------- */

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    Settings *s = s_settings_singleton;
    if (s == NULL || hi2c == NULL) return;
    if (hi2c != s->hi2c) return;

    if (s->wr_active && s->wr_inflight)
    {
        s->wr_inflight = 0u;
        s->wr_off = (uint16_t)(s->wr_off + s->wr_chunk);
        s->wr_chunk = 0u;

        /* After each page write - wait internal cycle */
        s->wr_wait_ready = 1u;
        s->wr_ready_start_ms = 0u;
    }
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    Settings *s = s_settings_singleton;
    if (s == NULL || hi2c == NULL) return;
    if (hi2c != s->hi2c) return;

    if (s->wr_active)
    {
        s->wr_active = 0u;
        s->wr_inflight = 0u;
        s->wr_wait_ready = 0u;
        s->save_state = SETTINGS_SAVE_ERROR;
        s->save_error = 3u; /* HAL I2C error */
    }
}
