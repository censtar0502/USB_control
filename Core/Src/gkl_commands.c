/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gkl_commands.c
  * @brief   GasKitLink v1.2 command implementations
  ******************************************************************************
  */
/* USER CODE END Header */

#include "gkl_commands.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * GasKitLink v1.2 Command Implementations
 * ========================================================================= */

/**
 * @brief Send Preset Volume command (V)
 * @param link: GKL link context
 * @param ctrl: Controller address (0x00)
 * @param slave: Slave address (0x01-0xFF)
 * @param nozzle: Nozzle number (1-9)
 * @param volume_cL: Volume in centiliters (1L = 100cL)
 * @param price: Price per liter
 * @return GKL_Result
 * 
 * Format: V{nozzle};{volume_cL};{price}
 * Example: V1;002550;1122 = 25.5L at 1122 per liter
 */
GKL_Result GKL_SendPresetVolume(GKL_Link *link, uint8_t ctrl, uint8_t slave,
                                 uint8_t nozzle, uint32_t volume_cL, uint16_t price)
{
    if (link == NULL) return GKL_ERR;
    
    char data[20];
    int len = snprintf(data, sizeof(data), "%u;%06lu;%04u",
                      (unsigned)nozzle, (unsigned long)volume_cL, (unsigned)price);
    
    if (len < 0 || len >= (int)sizeof(data)) return GKL_ERR;
    
    /* Expect 'S' status response */
    return GKL_Send(link, ctrl, slave, 'V', (const uint8_t *)data, (uint8_t)len, 'S');
}

/**
 * @brief Send Preset Money command (M)
 * @param link: GKL link context
 * @param ctrl: Controller address (0x00)
 * @param slave: Slave address (0x01-0xFF)
 * @param nozzle: Nozzle number (1-9)
 * @param money: Money amount in smallest units
 * @param price: Price per liter
 * @return GKL_Result
 * 
 * Format: M{nozzle};{money};{price}
 * Example: M1;005000;1100 = 5000 sum at 1100 per liter
 */
GKL_Result GKL_SendPresetMoney(GKL_Link *link, uint8_t ctrl, uint8_t slave,
                                uint8_t nozzle, uint32_t money, uint16_t price)
{
    if (link == NULL) return GKL_ERR;
    
    char data[20];
    int len = snprintf(data, sizeof(data), "%u;%06lu;%04u",
                      (unsigned)nozzle, (unsigned long)money, (unsigned)price);
    
    if (len < 0 || len >= (int)sizeof(data)) return GKL_ERR;
    
    /* Expect 'S' status response */
    return GKL_Send(link, ctrl, slave, 'M', (const uint8_t *)data, (uint8_t)len, 'S');
}

/**
 * @brief Send Stop command (B)
 * @param link: GKL link context
 * @param ctrl: Controller address
 * @param slave: Slave address
 * @return GKL_Result
 * 
 * Stops current dispensing operation (pause)
 */
GKL_Result GKL_SendStop(GKL_Link *link, uint8_t ctrl, uint8_t slave)
{
    if (link == NULL) return GKL_ERR;
    
    /* No data, expect 'S' status response */
    return GKL_Send(link, ctrl, slave, 'B', NULL, 0, 'S');
}

/**
 * @brief Send Resume command (G)
 * @param link: GKL link context
 * @param ctrl: Controller address
 * @param slave: Slave address
 * @return GKL_Result
 * 
 * Resumes paused dispensing operation
 */
GKL_Result GKL_SendResume(GKL_Link *link, uint8_t ctrl, uint8_t slave)
{
    if (link == NULL) return GKL_ERR;
    
    /* No data, expect 'S' status response */
    return GKL_Send(link, ctrl, slave, 'G', NULL, 0, 'S');
}

/**
 * @brief Send End command (N)
 * @param link: GKL link context
 * @param ctrl: Controller address
 * @param slave: Slave address
 * @return GKL_Result
 * 
 * Ends current transaction
 */
GKL_Result GKL_SendEnd(GKL_Link *link, uint8_t ctrl, uint8_t slave)
{
    if (link == NULL) return GKL_ERR;
    
    /* No data, expect 'S' status response */
    return GKL_Send(link, ctrl, slave, 'N', NULL, 0, 'S');
}

/**
 * @brief Send Poll Realtime Volume command (L)
 * @param link: GKL link context
 * @param ctrl: Controller address
 * @param slave: Slave address
 * @param nozzle: Nozzle number (1-9)
 * @return GKL_Result
 * 
 * Format: L{nozzle}
 * Response format: L{nozzle};{volume_cL}
 */
GKL_Result GKL_SendPollVolume(GKL_Link *link, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (link == NULL) return GKL_ERR;
    
    char data[2];
    data[0] = '0' + nozzle;
    data[1] = '\0';
    
    /* Expect 'L' response with volume data */
    return GKL_Send(link, ctrl, slave, 'L', (const uint8_t *)data, 1, 'L');
}

/**
 * @brief Send Poll Realtime Money command (R)
 * @param link: GKL link context
 * @param ctrl: Controller address
 * @param slave: Slave address
 * @param nozzle: Nozzle number (1-9)
 * @return GKL_Result
 * 
 * Format: R{nozzle}
 * Response format: R{nozzle};{money}
 */
GKL_Result GKL_SendPollMoney(GKL_Link *link, uint8_t ctrl, uint8_t slave, uint8_t nozzle)
{
    if (link == NULL) return GKL_ERR;
    
    char data[2];
    data[0] = '0' + nozzle;
    data[1] = '\0';
    
    /* Expect 'R' response with money data */
    return GKL_Send(link, ctrl, slave, 'R', (const uint8_t *)data, 1, 'R');
}

/**
 * @brief Send Read Totalizer command (C)
 * @param link: GKL link context
 * @param ctrl: Controller address
 * @param slave: Slave address
 * @param index: Totalizer index (0-7)
 * @return GKL_Result
 * 
 * Format: C{index}
 * Response format: C{index};{totalizer_value}
 * Example: C0;000396003 = 396.003 liters
 */
GKL_Result GKL_SendReadTotalizer(GKL_Link *link, uint8_t ctrl, uint8_t slave, uint8_t index)
{
    if (link == NULL) return GKL_ERR;
    
    char data[2];
    data[0] = '0' + index;
    data[1] = '\0';
    
    /* Expect 'C' response with totalizer data */
    return GKL_Send(link, ctrl, slave, 'C', (const uint8_t *)data, 1, 'C');
}

/**
 * @brief Send Read Transaction command (T)
 * @param link: GKL link context
 * @param ctrl: Controller address
 * @param slave: Slave address
 * @return GKL_Result
 * 
 * Response format: T{volume};{money};{price}
 * Example: T002550;028611;1122 = 25.5L, 28611 sum, 1122 per liter
 */
GKL_Result GKL_SendReadTransaction(GKL_Link *link, uint8_t ctrl, uint8_t slave)
{
    if (link == NULL) return GKL_ERR;
    
    /* No data, expect 'T' response with transaction data */
    return GKL_Send(link, ctrl, slave, 'T', NULL, 0, 'T');
}
