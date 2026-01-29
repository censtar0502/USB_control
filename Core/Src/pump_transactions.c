/* pump_transactions.c - With logging - FIXED */
#include "pump_transactions.h"
#include "gkl_link.h"
#include "cdc_logger.h"
#include <stdio.h>
#include <string.h>

/* Helper: Convert uint32 to BCD */
static void uint32_to_bcd(uint32_t val, uint8_t *out, uint8_t len)
{
    for (int i = len - 1; i >= 0; i--) {
        out[i] = (uint8_t)((val % 100) / 10) << 4 | (val % 10);
        val /= 100;
    }
}

/* Helper: Convert uint16 to BCD */
static void uint16_to_bcd(uint16_t val, uint8_t *out, uint8_t len)
{
    for (int i = len - 1; i >= 0; i--) {
        out[i] = (uint8_t)((val % 100) / 10) << 4 | (val % 10);
        val /= 100;
    }
}

/* Helper: Log frame in compact format */
static void log_frame(uint8_t ctrl, uint8_t slave, char cmd, const uint8_t *data, uint8_t data_len)
{
    uint8_t raw[32];
    uint8_t raw_len = 0;
    
    /* Build frame */
    raw[0] = 0x02;  /* STX */
    raw[1] = ctrl;
    raw[2] = slave;
    raw[3] = (uint8_t)cmd;
    for (uint8_t i = 0; i < data_len; i++) {
        raw[4 + i] = data[i];
    }
    raw_len = 4 + data_len;
    
    /* Calculate checksum */
    uint8_t crc = 0;
    for (uint8_t i = 1; i < raw_len; i++) {
        crc ^= raw[i];
    }
    raw[raw_len++] = crc;
    
    /* Format as compact string */
    char line[80];
    char *p = line;
    
    for (uint8_t i = 0; i < raw_len; i++) {
        uint8_t b = raw[i];
        if (b == 0x02) { p += sprintf(p, "<STX>"); }
        else if (b == 0x00) { p += sprintf(p, "<NUL>"); }
        else if (b == 0x01) { p += sprintf(p, "<SOH>"); }
        else if (b == 0x03) { p += sprintf(p, "<ETX>"); }
        else if (b == 0x04) { p += sprintf(p, "<EOT>"); }
        else if (b == 0x05) { p += sprintf(p, "<ENQ>"); }
        else if (b == 0x06) { p += sprintf(p, "<ACK>"); }
        else if (b == 0x07) { p += sprintf(p, "<BEL>"); }
        else if (b == 0x08) { p += sprintf(p, "<BS>"); }
        else if (b == 0x09) { p += sprintf(p, "<HT>"); }
        else if (b == 0x0A) { p += sprintf(p, "<LF>"); }
        else if (b == 0x0B) { p += sprintf(p, "<VT>"); }
        else if (b == 0x0C) { p += sprintf(p, "<FF>"); }
        else if (b == 0x0D) { p += sprintf(p, "<CR>"); }
        else if (b == 0x0E) { p += sprintf(p, "<SO>"); }
        else if (b == 0x0F) { p += sprintf(p, "<SI>"); }
        else if (b == 0x10) { p += sprintf(p, "<DLE>"); }
        else if (b == 0x11) { p += sprintf(p, "<DC1>"); }
        else if (b == 0x12) { p += sprintf(p, "<DC2>"); }
        else if (b == 0x13) { p += sprintf(p, "<DC3>"); }
        else if (b == 0x14) { p += sprintf(p, "<DC4>"); }
        else if (b == 0x15) { p += sprintf(p, "<NAK>"); }
        else if (b == 0x16) { p += sprintf(p, "<SYN>"); }
        else if (b == 0x17) { p += sprintf(p, "<ETB>"); }
        else if (b == 0x18) { p += sprintf(p, "<CAN>"); }
        else if (b == 0x19) { p += sprintf(p, "<EM>"); }
        else if (b == 0x1A) { p += sprintf(p, "<SUB>"); }
        else if (b == 0x1B) { p += sprintf(p, "<ESC>"); }
        else if (b == 0x1C) { p += sprintf(p, "<FS>"); }
        else if (b == 0x1D) { p += sprintf(p, "<GS>"); }
        else if (b == 0x1E) { p += sprintf(p, "<RS>"); }
        else if (b == 0x1F) { p += sprintf(p, "<US>"); }
        else if (b >= 0x20 && b <= 0x7E) { *p++ = (char)b; }
        else { p += sprintf(p, "<%02X>", b); }
    }
    *p = '\0';
    
    CDC_Log(line);  /* FIXED: only one parameter */
}

/* V - Preset Volume */
bool PumpTrans_PresetVolume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave,
                            uint8_t nozzle, uint32_t volume_dL, uint16_t price)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[7];
    data[0] = nozzle;
    
    uint32_t volume_cL = volume_dL * 10;
    uint32_to_bcd(volume_cL, &data[1], 4);
    uint16_to_bcd(price, &data[5], 2);
    
    log_frame(ctrl, slave, 'V', data, 7);
    
    return (GKL_Send(gkl, ctrl, slave, 'V', data, 7, 'V') == GKL_OK);
}

/* M - Preset Money */
bool PumpTrans_PresetMoney(GKL_Link *gkl, uint8_t ctrl, uint8_t slave,
                           uint8_t nozzle, uint32_t money, uint16_t price)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[7];
    data[0] = nozzle;
    uint32_to_bcd(money, &data[1], 4);
    uint16_to_bcd(price, &data[5], 2);
    
    log_frame(ctrl, slave, 'M', data, 7);
    
    return (GKL_Send(gkl, ctrl, slave, 'M', data, 7, 'M') == GKL_OK);
}

/* B - Stop */
bool PumpTrans_Stop(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    log_frame(ctrl, slave, 'B', NULL, 0);
    
    return (GKL_Send(gkl, ctrl, slave, 'B', NULL, 0, 'B') == GKL_OK);
}

/* G - Resume */
bool PumpTrans_Resume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    log_frame(ctrl, slave, 'G', NULL, 0);
    
    return (GKL_Send(gkl, ctrl, slave, 'G', NULL, 0, 'G') == GKL_OK);
}

/* N - End Transaction */
bool PumpTrans_End(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    log_frame(ctrl, slave, 'N', NULL, 0);
    
    return (GKL_Send(gkl, ctrl, slave, 'N', NULL, 0, 'N') == GKL_OK);
}

/* L - Poll Realtime Volume */
bool PumpTrans_PollRealtimeVolume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[1] = {nozzle};
    log_frame(ctrl, slave, 'L', data, 1);
    
    return (GKL_Send(gkl, ctrl, slave, 'L', data, 1, 'L') == GKL_OK);
}

/* R - Poll Realtime Money */
bool PumpTrans_PollRealtimeMoney(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[1] = {nozzle};
    log_frame(ctrl, slave, 'R', data, 1);
    
    return (GKL_Send(gkl, ctrl, slave, 'R', data, 1, 'R') == GKL_OK);
}

/* C - Read Totalizer */
bool PumpTrans_ReadTotalizer(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[1] = {nozzle};
    log_frame(ctrl, slave, 'C', data, 1);
    
    return (GKL_Send(gkl, ctrl, slave, 'C', data, 1, 'C') == GKL_OK);
}

/* T - Read Transaction */
bool PumpTrans_ReadTransaction(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    log_frame(ctrl, slave, 'T', NULL, 0);
    
    return (GKL_Send(gkl, ctrl, slave, 'T', NULL, 0, 'T') == GKL_OK);
}
