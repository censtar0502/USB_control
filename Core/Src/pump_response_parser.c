/* pump_response_parser.c - ASCII parser for GasKitLink responses */
#include "pump_response_parser.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

bool PumpResp_ParseStatus(const GKL_Frame *resp, uint16_t *code_u16, char *state_code)
{
    // Expected: resp->cmd == 'S', resp->data_len >= 3
    // Data bytes: d1 d2 state
    if (!resp || resp->cmd != 'S' || resp->data_len < 3u || !code_u16 || !state_code)
    {
        return false;
    }

    uint8_t d1 = resp->data[0];
    uint8_t d2 = resp->data[1];
    uint8_t st = resp->data[2];

    if (!isdigit((int)d1) || !isdigit((int)d2))
    {
        return false;
    }

    *code_u16 = (uint16_t)((uint16_t)(d1 - (uint8_t)'0') * 10u + (uint16_t)(d2 - (uint8_t)'0'));
    *state_code = (char)st;
    return true;
}

/* Copy a numeric substring into a temporary buffer and convert to uint32 */
static bool ascii_digits_to_u32(const uint8_t *p, uint16_t max_len, uint32_t *out)
{
    if (!p || !out || max_len == 0u) return false;

    char tmp[16];
    uint16_t n = 0u;

    while (n < max_len && n < (uint16_t)(sizeof(tmp) - 1u) && isdigit((int)p[n]))
    {
        tmp[n] = (char)p[n];
        n++;
    }

    if (n == 0u) return false;
    tmp[n] = '\0';

    char *endp = NULL;
    unsigned long v = strtoul(tmp, &endp, 10);
    if (endp == tmp) return false;

    *out = (uint32_t)v;
    return true;
}

/* Find the first ';' in data */
static const uint8_t* find_semicolon(const uint8_t *data, uint8_t len)
{
    if (!data || len == 0u) return NULL;
    for (uint8_t i = 0u; i < len; i++)
    {
        if (data[i] == (uint8_t)';') return &data[i];
    }
    return NULL;
}

/*
 * L response - Realtime volume
 * Typical ASCII format in logs: "1q6;000009"
 * - [0] nozzle (ASCII digit)
 * - contains ';'
 * - after ';' a 6-digit number (volume_cL)
 */
bool PumpResp_ParseRealtimeVolume(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *volume_dL)
{
    if (!resp || !nozzle || !volume_dL) return false;
    if (resp->cmd != 'L') return false;
    if (resp->data_len < 4u) return false; /* at least "1;0" */

    if (resp->data[0] < '0' || resp->data[0] > '9') return false;
    *nozzle = (uint8_t)(resp->data[0] - '0');

    const uint8_t *semi = find_semicolon(resp->data, resp->data_len);
    if (!semi) return false;

    const uint8_t *num = semi + 1u;
    uint16_t max_len = (uint16_t)(resp->data_len - (uint8_t)(num - resp->data));
    uint32_t volume_cL = 0u;

    if (!ascii_digits_to_u32(num, max_len, &volume_cL)) return false;

    /* Protocol commonly uses centiLiters; convert to deciLiters for UI (1 decimal). */
    *volume_dL = volume_cL / 10u;
    return true;
}

/*
 * R response - Realtime money
 * Typical ASCII format in logs: "1q6;000121"
 * - [0] nozzle (ASCII digit)
 * - contains ';'
 * - after ';' a 6-digit number (money)
 */
bool PumpResp_ParseRealtimeMoney(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *money)
{
    if (!resp || !nozzle || !money) return false;
    if (resp->cmd != 'R') return false;
    if (resp->data_len < 4u) return false;

    if (resp->data[0] < '0' || resp->data[0] > '9') return false;
    *nozzle = (uint8_t)(resp->data[0] - '0');

    const uint8_t *semi = find_semicolon(resp->data, resp->data_len);
    if (!semi) return false;

    const uint8_t *num = semi + 1u;
    uint16_t max_len = (uint16_t)(resp->data_len - (uint8_t)(num - resp->data));
    uint32_t m = 0u;

    if (!ascii_digits_to_u32(num, max_len, &m)) return false;
    *money = m;
    return true;
}

/*
 * C response - Totalizer
 * Typical ASCII format in logs: "1;000396003"
 * - [0] nozzle (ASCII digit)
 * - ';'
 * - number (usually 9 digits, totalizer_cL)
 */
bool PumpResp_ParseTotalizer(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *totalizer_dL)
{
    if (!resp || !nozzle || !totalizer_dL) return false;
    if (resp->cmd != 'C') return false;
    if (resp->data_len < 3u) return false;

    if (resp->data[0] < '0' || resp->data[0] > '9') return false;
    *nozzle = (uint8_t)(resp->data[0] - '0');

    const uint8_t *semi = find_semicolon(resp->data, resp->data_len);
    if (!semi) return false;

    const uint8_t *num = semi + 1u;
    uint16_t max_len = (uint16_t)(resp->data_len - (uint8_t)(num - resp->data));
    uint32_t total_cL = 0u;

    if (!ascii_digits_to_u32(num, max_len, &total_cL)) return false;

    *totalizer_dL = total_cL / 10u;
    return true;
}

/*
 * T response - Transaction
 * Typical ASCII format in logs: "1p8;005610;000500;1122"
 * Fields separated by ';'
 *  - field0: "1p8" (nozzle digit + mode/status), nozzle is first char
 *  - field1: money (6 digits)
 *  - field2: volume_cL (6 digits)
 *  - field3: price (4 digits)
 */
bool PumpResp_ParseTransaction(const GKL_Frame *resp, uint8_t *nozzle,
                                uint32_t *volume_dL, uint32_t *money, uint16_t *price)
{
    if (!resp || !nozzle || !volume_dL || !money || !price) return false;
    if (resp->cmd != 'T') return false;
    if (resp->data_len < 7u) return false; /* minimal "1;0;0;0" */

    if (resp->data[0] < '0' || resp->data[0] > '9') return false;
    *nozzle = (uint8_t)(resp->data[0] - '0');

    /* Split by ';' without modifying original buffer */
    const uint8_t *p = resp->data;
    const uint8_t *end = resp->data + resp->data_len;

    /* Find first ';' (after header like "1p8") */
    const uint8_t *s1 = NULL;
    for (const uint8_t *t = p; t < end; t++)
    {
        if (*t == (uint8_t)';') { s1 = t; break; }
    }
    if (!s1) return false;

    /* money */
    const uint8_t *m_start = s1 + 1u;
    const uint8_t *s2 = NULL;
    for (const uint8_t *t = m_start; t < end; t++)
    {
        if (*t == (uint8_t)';') { s2 = t; break; }
    }
    if (!s2) return false;

    uint32_t mval = 0u;
    if (!ascii_digits_to_u32(m_start, (uint16_t)(s2 - m_start), &mval)) return false;

    /* volume_cL */
    const uint8_t *v_start = s2 + 1u;
    const uint8_t *s3 = NULL;
    for (const uint8_t *t = v_start; t < end; t++)
    {
        if (*t == (uint8_t)';') { s3 = t; break; }
    }
    if (!s3) return false;

    uint32_t v_cL = 0u;
    if (!ascii_digits_to_u32(v_start, (uint16_t)(s3 - v_start), &v_cL)) return false;

    /* price */
    const uint8_t *p_start = s3 + 1u;
    uint32_t pval32 = 0u;
    if (!ascii_digits_to_u32(p_start, (uint16_t)(end - p_start), &pval32)) return false;

    *money = mval;
    *volume_dL = v_cL / 10u;
    *price = (uint16_t)pval32;
    return true;
}
