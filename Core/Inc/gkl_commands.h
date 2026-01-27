/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gkl_commands.h
  * @brief   GasKitLink v1.2 command wrappers (V/M/B/G/N/L/R/C/T)
  * 
  * This module provides high-level command functions for GasKitLink protocol.
  * Add this file to your project and include in gkl_link.c
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef GKL_COMMANDS_H
#define GKL_COMMANDS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gkl_link.h"

/**
 * @brief GasKitLink v1.2 Command Set
 * 
 * These functions wrap GKL_Send() with proper formatting for each command type.
 */

/* V - Preset Volume: V{nozzle};{volume_cL};{price} */
GKL_Result GKL_SendPresetVolume(GKL_Link *link, uint8_t ctrl, uint8_t slave,
                                 uint8_t nozzle, uint32_t volume_cL, uint16_t price);

/* M - Preset Money: M{nozzle};{money};{price} */
GKL_Result GKL_SendPresetMoney(GKL_Link *link, uint8_t ctrl, uint8_t slave,
                                uint8_t nozzle, uint32_t money, uint16_t price);

/* B - Stop dispensing */
GKL_Result GKL_SendStop(GKL_Link *link, uint8_t ctrl, uint8_t slave);

/* G - Resume dispensing */
GKL_Result GKL_SendResume(GKL_Link *link, uint8_t ctrl, uint8_t slave);

/* N - End transaction */
GKL_Result GKL_SendEnd(GKL_Link *link, uint8_t ctrl, uint8_t slave);

/* L - Poll realtime volume: L{nozzle} */
GKL_Result GKL_SendPollVolume(GKL_Link *link, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

/* R - Poll realtime money: R{nozzle} */
GKL_Result GKL_SendPollMoney(GKL_Link *link, uint8_t ctrl, uint8_t slave, uint8_t nozzle);

/* C - Read totalizer: C{index} */
GKL_Result GKL_SendReadTotalizer(GKL_Link *link, uint8_t ctrl, uint8_t slave, uint8_t index);

/* T - Read transaction */
GKL_Result GKL_SendReadTransaction(GKL_Link *link, uint8_t ctrl, uint8_t slave);

#ifdef __cplusplus
}
#endif

#endif /* GKL_COMMANDS_H */
