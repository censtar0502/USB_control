/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gkl_parsing.h
  * @brief   GasKitLink response parsing module
  * 
  * This module parses responses from GasKitLink protocol (L/R/C/T commands)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef GKL_PARSING_H
#define GKL_PARSING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "gkl_link.h"
#include "pump_proto.h"

/**
 * @brief Parse GasKitLink response and generate PumpEvent
 * @param frame: Received frame
 * @param event_out: Output event structure
 * @return true if event generated, false if no event or parse error
 */
bool GKL_ParseResponse(const GKL_Frame *frame, PumpEvent *event_out);

/**
 * @brief Get expected response data length for command
 * @param resp_cmd: Response command character
 * @return Expected data length, or 0xFF if unknown
 * 
 * NOTE: This is a helper to update gkl_resp_data_len_for_cmd()
 */
uint8_t GKL_GetResponseDataLen(char resp_cmd);

#ifdef __cplusplus
}
#endif

#endif /* GKL_PARSING_H */
