/* pump_transactions.c - FIXED: ASCII encoding for GasKit protocol */
#include "pump_transactions.h"
#include "gkl_link.h"
#include "cdc_logger.h"
#include <stdio.h>
#include <string.h>

/* Helper: Log frame with TRK tag and filter */
static void log_frame_with_tag(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave, 
                                char cmd, const uint8_t *data, uint8_t data_len)
{
    if (!gkl) return;
    
    /* Check filter */
#if (PUMP_GKL_LOG_TARGET > 0)
    if (gkl->tag[0] != 0) {
        if (PUMP_GKL_LOG_TARGET == 1 && strcmp(gkl->tag, "TRK1") != 0) return;
        if (PUMP_GKL_LOG_TARGET == 2 && strcmp(gkl->tag, "TRK2") != 0) return;
    }
#endif
    
    /* Build frame */
    uint8_t raw[32];
    raw[0] = 0x02;  /* STX */
    raw[1] = ctrl;
    raw[2] = slave;
    raw[3] = (uint8_t)cmd;
    for (uint8_t i = 0; i < data_len; i++) {
        raw[4 + i] = data[i];
    }
    uint8_t raw_len = 4 + data_len;
    
    /* Calculate checksum */
    uint8_t crc = 0;
    for (uint8_t i = 1; i < raw_len; i++) {
        crc ^= raw[i];
    }
    raw[raw_len++] = crc;
    
    /* Format as compact string */
    char line[120];
    char *p = line;
    
    /* Add tag prefix */
    if (gkl->tag[0] != 0) {
        p += sprintf(p, "[%s] ", gkl->tag);
    }
    
    /* Format frame */
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
    
    CDC_Log(line);
}

/* V - Preset Volume (ASCII format: "V1;000500;1100") */
bool PumpTrans_PresetVolume(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave,
                            uint8_t nozzle, uint32_t volume_dL, uint16_t price)
{
    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;

    /* Convert dL to cL: 5.0L = 50dL = 500cL */
    uint32_t volume_cL = volume_dL * 10u;

    /* Format: "1;000500;1100" (nozzle;volume_cL;price) - 6 digits for centiLiters */
    char buf[20];
    snprintf(buf, sizeof(buf), "%u;%06lu;%04u",
             nozzle, (unsigned long)volume_cL, price);

    uint8_t len = (uint8_t)strlen(buf);

    /*
     * IMPORTANT:
     * V command typically has NO immediate response in reference behavior.
     * The result is observed via subsequent SR polling, so we send with expected_resp_cmd = 0
     * to avoid waiting for a response and to keep pause minimal.
     */
    if (GKL_Send(&gkl->link, ctrl, slave, 'V', (const uint8_t*)buf, len, 0) == GKL_OK)
    {
        log_frame_with_tag(gkl, ctrl, slave, 'V', (const uint8_t*)buf, len);
        return true;
    }
    return false;
}


/* M - Preset Money (ASCII format: "M1;005500;1100") */
bool PumpTrans_PresetMoney(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave,
                           uint8_t nozzle, uint32_t money, uint16_t price)
{
    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;

    /* Format: "1;005500;1100" (nozzle;money;price) */
    char buf[20];
    snprintf(buf, sizeof(buf), "%u;%06lu;%04u",
             nozzle, (unsigned long)money, price);

    uint8_t len = (uint8_t)strlen(buf);

    /*
     * IMPORTANT:
     * M command behaves like a write command; observe result via SR.
     * Send with expected_resp_cmd = 0 (do not block waiting for response).
     */
    if (GKL_Send(&gkl->link, ctrl, slave, 'M', (const uint8_t*)buf, len, 0) == GKL_OK)
    {
        log_frame_with_tag(gkl, ctrl, slave, 'M', (const uint8_t*)buf, len);
        return true;
    }
    return false;
}


/* B - Stop */
bool PumpTrans_Stop(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;

    /*
     * Stop command: result is observed via SR status changes.
     * Send with expected_resp_cmd = 0.
     */
    if (GKL_Send(&gkl->link, ctrl, slave, 'B', NULL, 0u, 0) == GKL_OK)
    {
        log_frame_with_tag(gkl, ctrl, slave, 'B', NULL, 0u);
        return true;
    }
    return false;
}


/* G - Resume */
bool PumpTrans_Resume(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;

    /*
     * Resume command: result is observed via SR status changes.
     * Send with expected_resp_cmd = 0.
     */
    if (GKL_Send(&gkl->link, ctrl, slave, 'G', NULL, 0u, 0) == GKL_OK)
    {
        log_frame_with_tag(gkl, ctrl, slave, 'G', NULL, 0u);
        return true;
    }
    return false;
}


/* N - End Transaction */
bool PumpTrans_End(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;

    /*
     * End/Close command: reference behavior shows SR polling continues and state changes are via Sxx.
     * Send with expected_resp_cmd = 0.
     */
    if (GKL_Send(&gkl->link, ctrl, slave, 'N', NULL, 0u, 0) == GKL_OK)
    {
        log_frame_with_tag(gkl, ctrl, slave, 'N', NULL, 0u);
        return true;
    }
    return false;
}


/* L - Poll Realtime Volume (ASCII format: "1") */
bool PumpTrans_PollRealtimeVolume(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    (void)nozzle; /* Not used by GKL: request has no payload (matches reference log: LM) */

    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;

    /* Realtime volume request has NO data field. Expected response CMD is 'L'. */
    if (GKL_Send(&gkl->link, ctrl, slave, 'L', NULL, 0u, 'L') == GKL_OK)
    {
        log_frame_with_tag(gkl, ctrl, slave, 'L', NULL, 0u);
        return true;
    }
    return false;
}


/* R - Poll Realtime Money (ASCII format: "1") */
bool PumpTrans_PollRealtimeMoney(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    (void)nozzle; /* Not used by GKL: request has no payload (matches reference log: RS) */

    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;

    /* Realtime money request has NO data field. Expected response CMD is 'R'. */
    if (GKL_Send(&gkl->link, ctrl, slave, 'R', NULL, 0u, 'R') == GKL_OK)
    {
        log_frame_with_tag(gkl, ctrl, slave, 'R', NULL, 0u);
        return true;
    }
    return false;
}


/* C - Read Totalizer (ASCII format: "0") */
bool PumpTrans_ReadTotalizer(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;
    
    char buf[2];
    buf[0] = '0' + nozzle;  /* ASCII digit */
    buf[1] = '\0';
    
    if (GKL_Send(&gkl->link, ctrl, slave, 'C', (const uint8_t*)buf, 1, 'C') == GKL_OK) {
        log_frame_with_tag(gkl, ctrl, slave, 'C', (const uint8_t*)buf, 1);
        return true;
    }
    return false;
}

/* T - Read Transaction */
bool PumpTrans_ReadTransaction(PumpProtoGKL *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->link.state != GKL_STATE_IDLE) return false;
    
    if (GKL_Send(&gkl->link, ctrl, slave, 'T', NULL, 0, 'T') == GKL_OK) {
        log_frame_with_tag(gkl, ctrl, slave, 'T', NULL, 0);
        return true;
    }
    return false;
}
