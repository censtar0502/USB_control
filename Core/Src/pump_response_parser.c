/* pump_response_parser.c */
#include "pump_response_parser.h"
#include <string.h>

/* Helper: Convert BCD to uint32 */
static uint32_t bcd_to_uint32(const uint8_t *bcd, uint8_t len)
{
    uint32_t val = 0;
    for (uint8_t i = 0; i < len; i++) {
        val = val * 100 + ((bcd[i] >> 4) * 10 + (bcd[i] & 0x0F));
    }
    return val;
}

/* Helper: Convert BCD to uint16 */
static uint16_t bcd_to_uint16(const uint8_t *bcd, uint8_t len)
{
    uint16_t val = 0;
    for (uint8_t i = 0; i < len; i++) {
        val = val * 100 + ((bcd[i] >> 4) * 10 + (bcd[i] & 0x0F));
    }
    return val;
}

/* Parse L response: 
   Format: <nozzle(1)><volume_cL(4 BCD)> = 5 bytes */
bool PumpResp_ParseRealtimeVolume(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *volume_dL)
{
    if (!resp || !nozzle || !volume_dL) return false;
    if (resp->cmd != 'L' || resp->data_len < 5) return false;
    
    *nozzle = resp->data[0];
    uint32_t volume_cL = bcd_to_uint32(&resp->data[1], 4);
    *volume_dL = volume_cL / 10;  /* Convert cL to dL */
    
    return true;
}

/* Parse R response:
   Format: <nozzle(1)><money(4 BCD)> = 5 bytes */
bool PumpResp_ParseRealtimeMoney(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *money)
{
    if (!resp || !nozzle || !money) return false;
    if (resp->cmd != 'R' || resp->data_len < 5) return false;
    
    *nozzle = resp->data[0];
    *money = bcd_to_uint32(&resp->data[1], 4);
    
    return true;
}

/* Parse C response:
   Format: <nozzle(1)><totalizer_cL(5 BCD)> = 6 bytes */
bool PumpResp_ParseTotalizer(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *totalizer_dL)
{
    if (!resp || !nozzle || !totalizer_dL) return false;
    if (resp->cmd != 'C' || resp->data_len < 6) return false;
    
    *nozzle = resp->data[0];
    uint32_t totalizer_cL = bcd_to_uint32(&resp->data[1], 5);
    *totalizer_dL = totalizer_cL / 10;  /* Convert cL to dL */
    
    return true;
}

/* Parse T response:
   Format: <nozzle(1)><volume_cL(4 BCD)><money(4 BCD)><price(2 BCD)> = 11 bytes */
bool PumpResp_ParseTransaction(const GKL_Frame *resp, uint8_t *nozzle,
                                uint32_t *volume_dL, uint32_t *money, uint16_t *price)
{
    if (!resp || !nozzle || !volume_dL || !money || !price) return false;
    if (resp->cmd != 'T' || resp->data_len < 11) return false;
    
    *nozzle = resp->data[0];
    uint32_t volume_cL = bcd_to_uint32(&resp->data[1], 4);
    *volume_dL = volume_cL / 10;
    *money = bcd_to_uint32(&resp->data[5], 4);
    *price = bcd_to_uint16(&resp->data[9], 2);
    
    return true;
}
