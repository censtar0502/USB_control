/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file pump_proto_gkl.c
  * @brief GasKitLink (CENSTAR) implementation of PumpProto interface
  ******************************************************************************
  */
/* USER CODE END Header */

#include "pump_proto_gkl.h"

#include "cdc_logger.h"

#include <string.h>
#include <stdio.h>

/* ===================== Small local helpers ===================== */

static uint8_t q_next(uint8_t v)
{
    return (uint8_t)((uint8_t)(v + 1u) % (uint8_t)PUMP_GKL_EVTQ_LEN);
}

static bool q_is_full(PumpProtoGKL *gkl)
{
    return (q_next(gkl->q_head) == gkl->q_tail);
}

static bool q_is_empty(PumpProtoGKL *gkl)
{
    return (gkl->q_head == gkl->q_tail);
}

static void q_push(PumpProtoGKL *gkl, const PumpEvent *e)
{
    if (gkl == NULL || e == NULL) return;
    if (q_is_full(gkl))
    {
        /* Drop oldest (never block CPU) */
        gkl->q_tail = q_next(gkl->q_tail);
    }
    gkl->q[gkl->q_head] = *e;
    gkl->q_head = q_next(gkl->q_head);
}

static bool q_pop(PumpProtoGKL *gkl, PumpEvent *out)
{
    if (gkl == NULL || out == NULL) return false;
    if (q_is_empty(gkl)) return false;
    *out = gkl->q[gkl->q_tail];
    gkl->q_tail = q_next(gkl->q_tail);
    return true;
}

static void maybe_report_error(PumpProtoGKL *gkl)
{
    if (gkl == NULL) return;

    GKL_Stats st = GKL_GetStats(&gkl->link);
    if (st.last_error == GKL_OK) return;

    /* Avoid spamming same error repeatedly */
    if (gkl->last_reported_err == st.last_error && gkl->last_reported_failcnt == st.consecutive_fail)
    {
        return;
    }

    gkl->last_reported_err = st.last_error;
    gkl->last_reported_failcnt = st.consecutive_fail;

    PumpEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = PUMP_EVT_ERROR;
    ev.ctrl_addr = gkl->pending_ctrl;
    ev.slave_addr = gkl->pending_slave;
    ev.error_code = (uint8_t)st.last_error;
    ev.fail_count = st.consecutive_fail;
    q_push(gkl, &ev);
}

/* ===================== Logging helpers (USB CDC) ===================== */

static const char* gkl_err_str(GKL_Result e)
{
    switch (e)
    {
        case GKL_OK: return "OK";
        case GKL_ERR_PARAM: return "PARAM";
        case GKL_ERR_BUSY: return "BUSY";
        case GKL_ERR_UART: return "UART";
        case GKL_ERR_TIMEOUT: return "TIMEOUT";
        case GKL_ERR_CRC: return "CRC";
        case GKL_ERR_FORMAT: return "FORMAT";
        default: return "ERR";
    }
}

static void gkl_append_byte_token(char *out, size_t outsz, size_t *pos, uint8_t b)
{
    switch (b)
    {
        case 0x02:
            if ((*pos + 5u) < outsz)
            {
                out[*pos++] = '<'; out[*pos++] = 'S'; out[*pos++] = 'T'; out[*pos++] = 'X'; out[*pos++] = '>';
            }
            return;
        case 0x00:
            if ((*pos + 5u) < outsz)
            {
                out[*pos++] = '<'; out[*pos++] = 'N'; out[*pos++] = 'U'; out[*pos++] = 'L'; out[*pos++] = '>';
            }
            return;
        case 0x01:
            if ((*pos + 5u) < outsz)
            {
                out[*pos++] = '<'; out[*pos++] = 'S'; out[*pos++] = 'O'; out[*pos++] = 'H'; out[*pos++] = '>';
            }
            return;
        case 0x03:
            if ((*pos + 5u) < outsz)
            {
                out[*pos++] = '<'; out[*pos++] = 'E'; out[*pos++] = 'T'; out[*pos++] = 'X'; out[*pos++] = '>';
            }
            return;
        default: break;
    }

    if (b >= 0x20u && b <= 0x7Eu)
    {
        if ((*pos + 1u) < outsz)
        {
            out[*pos] = (char)b;
            (*pos)++;
            out[*pos] = 0;
        }
        return;
    }

    /* Non-printable -> hex token */
    char tmp[8];
    (void)snprintf(tmp, sizeof(tmp), "<%02X>", (unsigned)b);
    size_t len = strlen(tmp);
    if ((*pos + len) < outsz)
    {
        memcpy(&out[*pos], tmp, len);
        *pos += len;
    }
}

static void gkl_format_frame_compact(const uint8_t *bytes, uint8_t len, char *out, size_t outsz)
{
    if (out == NULL || outsz == 0u) return;
    out[0] = 0;
    if (bytes == NULL || len == 0u) return;

    size_t pos = 0u;
    for (uint8_t i = 0u; i < len; i++)
    {
        gkl_append_byte_token(out, outsz, &pos, bytes[i]);
        if ((pos + 1u) >= outsz) break;
    }
    out[outsz - 1u] = 0;
}

static void gkl_log_line(PumpProtoGKL *gkl, const char *line)
{
    if (line == NULL) return;

#if (PUMP_GKL_LOG_TARGET > 0)
    /* Filter logs based on tag */
    if (gkl && gkl->tag[0] != 0)
    {
        if (PUMP_GKL_LOG_TARGET == 1 && strcmp(gkl->tag, "TRK1") != 0) return;
        if (PUMP_GKL_LOG_TARGET == 2 && strcmp(gkl->tag, "TRK2") != 0) return;
    }
#endif

    /* Skip verbose logging if compact mode is enabled */
#if (PUMP_GKL_COMPACT_LOG == 1)
    /* Just push the line without tag */
    CDC_LOG_Push(line);
#else
    if (gkl && gkl->tag[0] != 0)
    {
        char buf[320];
        (void)snprintf(buf, sizeof(buf), "%s %s", gkl->tag, line);
        CDC_LOG_Push(buf);
    }
    else
    {
        CDC_LOG_Push(line);
    }
#endif
}

/* ===================== PumpProto vtable implementation ===================== */

static void gkl_task(void *ctx)
{
    PumpProtoGKL *gkl = (PumpProtoGKL*)ctx;
    if (gkl == NULL) return;

    /* ===================== DEBUG: dump any raw RX bytes ===================== */
    {
        uint32_t uerr = 0u;
        if (GKL_GetAndClearUartError(&gkl->link, &uerr))
        {
#if (PUMP_GKL_COMPACT_LOG == 0)
            char l[80];
            (void)snprintf(l, sizeof(l), "UART_ERR=0x%08lX\r\n", (unsigned long)uerr);
            gkl_log_line(gkl, l);
#endif
        }

        if (gkl->link.raw_rx_overflow)
        {
            gkl->link.raw_rx_overflow = 0u;
#if (PUMP_GKL_COMPACT_LOG == 0)
            gkl_log_line(gkl, "RAW_RX overflow\r\n");
#endif
        }

#if (PUMP_GKL_COMPACT_LOG == 0)
        uint8_t tmp[48];
        uint16_t n;
        while ((n = GKL_RawRxDrain(&gkl->link, tmp, (uint16_t)sizeof(tmp))) != 0u)
        {
            char line[240];
            size_t pos = 0u;
            pos += (size_t)snprintf(&line[pos], sizeof(line) - pos, "RAW_RX ");
            for (uint16_t i = 0u; i < n && (pos + 4u) < sizeof(line); i++)
            {
                pos += (size_t)snprintf(&line[pos], sizeof(line) - pos, "%02X ", (unsigned)tmp[i]);
            }
            pos += (size_t)snprintf(&line[pos], sizeof(line) - pos, "\r\n");
            gkl_log_line(gkl, line);
        }
#endif
    }

    GKL_Task(&gkl->link);

    if (GKL_HasResponse(&gkl->link))
    {
        GKL_Frame fr;
        if (GKL_GetResponse(&gkl->link, &fr))
        {
#if (PUMP_GKL_COMPACT_LOG == 1)
            /* Compact format: <STX><NUL><SOH>SR (like reference log) */
            uint8_t raw[GKL_MAX_FRAME_LEN];
            uint8_t raw_len = 0u;
            raw[0] = GKL_STX;
            raw[1] = fr.ctrl;
            raw[2] = fr.slave;
            raw[3] = (uint8_t)fr.cmd;
            for (uint8_t i = 0u; i < fr.data_len; i++)
            {
                raw[4u + i] = fr.data[i];
            }
            raw_len = (uint8_t)(5u + fr.data_len);
            uint8_t c = 0u;
            for (uint8_t i = 1u; i < (uint8_t)(raw_len - 1u); i++) c ^= raw[i];
            raw[raw_len - 1u] = c;

            char line[64];
            gkl_format_frame_compact(raw, raw_len, line, sizeof(line));
            strcat(line, "\r\n");
            gkl_log_line(gkl, line);
#else
            /* Verbose format */
#if (PUMP_GKL_TRACE_FRAMES)
            uint8_t raw[GKL_MAX_FRAME_LEN];
            uint8_t raw_len = 0u;
            raw[0] = GKL_STX;
            raw[1] = fr.ctrl;
            raw[2] = fr.slave;
            raw[3] = (uint8_t)fr.cmd;
            for (uint8_t i = 0u; i < fr.data_len; i++)
            {
                raw[4u + i] = fr.data[i];
            }
            raw_len = (uint8_t)(5u + fr.data_len);
            uint8_t c = 0u;
            for (uint8_t i = 1u; i < (uint8_t)(raw_len - 1u); i++) c ^= raw[i];
            raw[raw_len - 1u] = c;

            char fstr[240];
            gkl_format_frame_bytes(raw, raw_len, fstr, sizeof(fstr));
            char l[300];
            char hstr[240];
            gkl_format_hex_bytes(raw, raw_len, hstr, sizeof(hstr));
            (void)snprintf(l, sizeof(l), "RX %s | HEX: %s\r\n", fstr, hstr);
            gkl_log_line(gkl, l);
#endif
#endif

            if (gkl->no_connect_latched)
            {
                gkl->no_connect_latched = 0u;
#if (PUMP_GKL_COMPACT_LOG == 0)
                gkl_log_line(gkl, "LINK OK\r\n");
#endif
            }

            if (fr.cmd == 'S' && fr.data_len >= 2u)
            {
                uint8_t st = fr.data[0];
                uint8_t noz = fr.data[1];

                if (st >= (uint8_t)'0' && st <= (uint8_t)'9') st -= (uint8_t)'0';
                if (noz >= (uint8_t)'0' && noz <= (uint8_t)'9') noz -= (uint8_t)'0';

                PumpEvent ev;
                memset(&ev, 0, sizeof(ev));
                ev.type = PUMP_EVT_STATUS;
                ev.ctrl_addr = fr.ctrl;
                ev.slave_addr = fr.slave;
                ev.status = st;
                ev.nozzle = noz;
                q_push(gkl, &ev);
            }
        }
        gkl->pending = 0u;
        return;
    }

    if (gkl->pending)
    {
        GKL_Stats st = GKL_GetStats(&gkl->link);
        if (st.last_error != GKL_OK)
        {
            maybe_report_error(gkl);

            if ((st.consecutive_fail >= (uint8_t)PUMP_GKL_NO_CONNECT_THRESHOLD) && (gkl->no_connect_latched == 0u))
            {
                gkl->no_connect_latched = 1u;
#if (PUMP_GKL_COMPACT_LOG == 0)
                char l[128];
                (void)snprintf(l, sizeof(l),
                               "No Connect!! fail=%u err=%s rx=%u len=%u last=0x%02X tot=%lu/%lu\r\n",
                               (unsigned)st.consecutive_fail,
                               gkl_err_str(st.last_error),
                               (unsigned)st.rx_seen_since_tx,
                               (unsigned)st.rx_len,
                               (unsigned)st.last_rx_byte,
                               (unsigned long)st.rx_total_bytes,
                               (unsigned long)st.rx_total_frames);
                gkl_log_line(gkl, l);
#endif
            }
            gkl->pending = 0u;
        }
    }
}

static bool gkl_is_idle(void *ctx)
{
    PumpProtoGKL *gkl = (PumpProtoGKL*)ctx;
    if (gkl == NULL) return false;
    return (GKL_GetStats(&gkl->link).state == GKL_STATE_IDLE);
}

static PumpProtoResult gkl_send_poll_status(void *ctx, uint8_t ctrl_addr, uint8_t slave_addr)
{
    PumpProtoGKL *gkl = (PumpProtoGKL *)ctx;
    if (gkl == NULL) return PUMP_PROTO_ERR;

    GKL_Result r = GKL_Send(&gkl->link, ctrl_addr, slave_addr, 'S', NULL, 0u, 'S');
    if (r == GKL_ERR_BUSY) return PUMP_PROTO_BUSY;
    if (r != GKL_OK) return PUMP_PROTO_ERR;

#if (PUMP_GKL_COMPACT_LOG == 1)
    /* Compact format for TX frames */
    uint8_t raw[GKL_MAX_FRAME_LEN];
    uint8_t raw_len = 0u;
    if (GKL_BuildFrame(ctrl_addr, slave_addr, 'S', NULL, 0u, raw, &raw_len) == GKL_OK)
    {
        char line[64];
        gkl_format_frame_compact(raw, raw_len, line, sizeof(line));
        strcat(line, "\r\n");
        gkl_log_line(gkl, line);
    }
#else
#if (PUMP_GKL_TRACE_FRAMES)
    uint8_t raw[GKL_MAX_FRAME_LEN];
    uint8_t raw_len = 0u;
    if (GKL_BuildFrame(ctrl_addr, slave_addr, 'S', NULL, 0u, raw, &raw_len) == GKL_OK)
    {
        char fstr[240];
        gkl_format_frame_bytes(raw, raw_len, fstr, sizeof(fstr));
        char l[300];
        char hstr[240];
        gkl_format_hex_bytes(raw, raw_len, hstr, sizeof(hstr));
        (void)snprintf(l, sizeof(l), "TX %s | HEX: %s\r\n", fstr, hstr);
        gkl_log_line(gkl, l);
    }
#endif
#endif

    gkl->pending = 1u;
    gkl->pending_ctrl = ctrl_addr;
    gkl->pending_slave = slave_addr;

    GKL_Stats st = GKL_GetStats(&gkl->link);
    gkl->pending_rx_bytes_start = st.rx_total_bytes;

    return PUMP_PROTO_OK;
}

static bool gkl_pop_event(void *ctx, PumpEvent *out)
{
    PumpProtoGKL *gkl = (PumpProtoGKL*)ctx;
    return q_pop(gkl, out);
}

static const PumpProtoVTable s_vt = {
    .task             = gkl_task,
    .is_idle          = gkl_is_idle,
    .send_poll_status = gkl_send_poll_status,
    .pop_event        = gkl_pop_event
};

/* ===================== Public API ===================== */

void PumpProtoGKL_Init(PumpProtoGKL *gkl, UART_HandleTypeDef *huart)
{
    if (gkl == NULL) return;
    memset(gkl, 0, sizeof(*gkl));
    gkl->q_head = 0u;
    gkl->q_tail = 0u;
    gkl->pending = 0u;
    gkl->pending_ctrl = 0u;
    gkl->pending_slave = 0u;
    gkl->last_reported_err = GKL_OK;
    gkl->last_reported_failcnt = 0u;
    gkl->no_connect_latched = 0u;

    gkl->tag[0] = 0;

    GKL_Init(&gkl->link, huart);
}

void PumpProtoGKL_SetTag(PumpProtoGKL *gkl, const char *tag)
{
    if (gkl == NULL) return;
    if (tag == NULL)
    {
        gkl->tag[0] = 0;
        return;
    }

    size_t i = 0u;
    for (; i < (sizeof(gkl->tag) - 1u) && tag[i] != 0; i++)
    {
        gkl->tag[i] = tag[i];
    }
    gkl->tag[i] = 0;
}

void PumpProtoGKL_Bind(PumpProto *out, PumpProtoGKL *gkl)
{
    if (out == NULL) return;
    out->vt  = &s_vt;
    out->ctx = (void*)gkl;
}
