/* pump_transactions.c - Direct GKL transaction support */
#include "pump_transactions.h"
#include "gkl_link.h"
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

/* V - Preset Volume */
bool PumpTrans_PresetVolume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave,
                            uint8_t nozzle, uint32_t volume_dL, uint16_t price)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[7];
    data[0] = nozzle;
    
    /* Convert dL to cL (×10) */
    uint32_t volume_cL = volume_dL * 10;
    uint32_to_bcd(volume_cL, &data[1], 4);
    
    /* Price in BCD */
    uint16_to_bcd(price, &data[5], 2);
    
    return (GKL_SendCommand(gkl, ctrl, slave, 'V', data, 7) == GKL_OK);
}

/* M - Preset Money */
bool PumpTrans_PresetMoney(GKL_Link *gkl, uint8_t ctrl, uint8_t slave,
                           uint8_t nozzle, uint32_t money, uint16_t price)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[7];
    data[0] = nozzle;
    
    /* Money in BCD */
    uint32_to_bcd(money, &data[1], 4);
    
    /* Price in BCD */
    uint16_to_bcd(price, &data[5], 2);
    
    return (GKL_SendCommand(gkl, ctrl, slave, 'M', data, 7) == GKL_OK);
}

/* B - Stop */
bool PumpTrans_Stop(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    return (GKL_SendCommand(gkl, ctrl, slave, 'B', NULL, 0) == GKL_OK);
}

/* G - Resume */
bool PumpTrans_Resume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    return (GKL_SendCommand(gkl, ctrl, slave, 'G', NULL, 0) == GKL_OK);
}

/* N - End Transaction */
bool PumpTrans_End(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    return (GKL_SendCommand(gkl, ctrl, slave, 'N', NULL, 0) == GKL_OK);
}

/* L - Poll Realtime Volume */
bool PumpTrans_PollRealtimeVolume(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[1] = {nozzle};
    return (GKL_SendCommand(gkl, ctrl, slave, 'L', data, 1) == GKL_OK);
}

/* R - Poll Realtime Money */
bool PumpTrans_PollRealtimeMoney(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[1] = {nozzle};
    return (GKL_SendCommand(gkl, ctrl, slave, 'R', data, 1) == GKL_OK);
}

/* C - Read Totalizer */
bool PumpTrans_ReadTotalizer(GKL_Link *gkl, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    
    uint8_t data[1] = {nozzle};
    return (GKL_SendCommand(gkl, ctrl, slave, 'C', data, 1) == GKL_OK);
}

/* T - Read Transaction */
bool PumpTrans_ReadTransaction(GKL_Link *gkl, uint8_t ctrl, uint8_t slave)
{
    if (!gkl || gkl->state != GKL_STATE_IDLE) return false;
    return (GKL_SendCommand(gkl, ctrl, slave, 'T', NULL, 0) == GKL_OK);
}
