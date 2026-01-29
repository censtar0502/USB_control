/* pump_response_parser.h - Parse GKL transaction responses */
#ifndef PUMP_RESPONSE_PARSER_H
#define PUMP_RESPONSE_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "gkl_link.h"

/* Parse S response - Status (e.g. "S10S", "S31P", "S61U")
 * Data part expected: 3 bytes: ['0'..'9','0'..'9', state_letter]
 */
bool PumpResp_ParseStatus(const GKL_Frame *resp, uint16_t *code_u16, char *state_code);

/* Parse L response - Realtime volume */
bool PumpResp_ParseRealtimeVolume(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *volume_dL);

/* Parse R response - Realtime money */
bool PumpResp_ParseRealtimeMoney(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *money);

/* Parse C response - Totalizer */
bool PumpResp_ParseTotalizer(const GKL_Frame *resp, uint8_t *nozzle, uint32_t *totalizer_dL);

/* Parse T response - Transaction */
bool PumpResp_ParseTransaction(const GKL_Frame *resp, uint8_t *nozzle, 
                                uint32_t *volume_dL, uint32_t *money, uint16_t *price);

#ifdef __cplusplus
}
#endif

#endif /* PUMP_RESPONSE_PARSER_H */
