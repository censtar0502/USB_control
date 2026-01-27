/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    gkl_parsing.c
  * @brief   GasKitLink response parsing implementation
  ******************************************************************************
  */
/* USER CODE END Header */

#include "gkl_parsing.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Response Data Length Table
 * ========================================================================= */

/**
 * @brief Get expected response data length for each command type
 */
uint8_t GKL_GetResponseDataLen(char resp_cmd)
{
    switch (resp_cmd)
    {
        case 'S': return 2u;   /* Status: "10" */
        case 'L': return 10u;  /* Volume: "1;000123" */
        case 'R': return 10u;  /* Money: "1;000123" */
        case 'C': return 11u;  /* Totalizer: "0;000396003" */
        case 'T': return 25u;  /* Transaction: more data */
        case 'D': return 2u;   /* Ack */
        case 'Z': return 6u;   /* Other */
        default:  return 0xFFu; /* Unknown */
    }
}

/* ============================================================================
 * Response Parsing Implementation
 * ========================================================================= */

/**
 * @brief Parse GasKitLink response frame
 * @param frame: Received frame from GKL protocol
 * @param event_out: Output PumpEvent structure
 * @return true if event generated, false otherwise
 */
bool GKL_ParseResponse(const GKL_Frame *frame, PumpEvent *event_out)
{
    if (frame == NULL || event_out == NULL) return false;
    
    /* Clear output event */
    memset(event_out, 0, sizeof(PumpEvent));
    
    /* Set common fields */
    event_out->ctrl_addr = frame->ctrl;
    event_out->slave_addr = frame->slave;
    
    /* Parse by command type */
    switch (frame->cmd)
    {
        case 'S':  /* Status response */
        {
            if (frame->data_len >= 2)
            {
                uint8_t s1 = (uint8_t)(frame->data[0] - '0');
                uint8_t s2 = (uint8_t)(frame->data[1] - '0');
                
                event_out->type = PUMP_EVT_STATUS;
                event_out->u.st.status = s1;
                event_out->u.st.nozzle = s2;
                
                return true;
            }
            break;
        }
        
        case 'L':  /* Realtime volume response */
        {
            /* Format: "g;vvvvvv" - nozzle;volume in cL */
            char *semicolon = strchr((char*)frame->data, ';');
            if (semicolon && frame->data_len >= 8)
            {
                uint8_t nozzle = (uint8_t)(frame->data[0] - '0');
                uint32_t volume_cL = 0;
                
                /* Parse volume in centiliters */
                sscanf(semicolon + 1, "%lu", &volume_cL);
                
                /* Convert cL to dL: 1 dL = 10 cL */
                uint32_t volume_dL = volume_cL / 10;
                
                event_out->type = PUMP_EVT_REALTIME_VOLUME;
                event_out->u.rt_volume.value = volume_dL;
                event_out->u.rt_volume.nozzle = nozzle;
                
                return true;
            }
            break;
        }
        
        case 'R':  /* Realtime money response */
        {
            /* Format: "g;mmmmmm" - nozzle;money */
            char *semicolon = strchr((char*)frame->data, ';');
            if (semicolon && frame->data_len >= 8)
            {
                uint8_t nozzle = (uint8_t)(frame->data[0] - '0');
                uint32_t money = 0;
                
                /* Parse money value */
                sscanf(semicolon + 1, "%lu", &money);
                
                event_out->type = PUMP_EVT_REALTIME_MONEY;
                event_out->u.rt_money.value = money;
                event_out->u.rt_money.nozzle = nozzle;
                
                return true;
            }
            break;
        }
        
        case 'C':  /* Totalizer response */
        {
            /* Format: "i;ttttttttt" - index;totalizer(9 digits in cL) */
            char *semicolon = strchr((char*)frame->data, ';');
            if (semicolon && frame->data_len >= 11)
            {
                uint8_t index = (uint8_t)(frame->data[0] - '0');
                uint32_t totalizer_cL = 0;
                
                /* Parse totalizer in centiliters */
                sscanf(semicolon + 1, "%lu", &totalizer_cL);
                
                /* Convert cL to dL */
                uint32_t totalizer_dL = totalizer_cL / 10;
                
                event_out->type = PUMP_EVT_TOTALIZER;
                event_out->u.totalizer.index = index;
                event_out->u.totalizer.value = totalizer_dL;
                
                return true;
            }
            break;
        }
        
        case 'T':  /* Transaction response */
        {
            /* Format: "vvvvvv;mmmmmm;pppp" - volume;money;price */
            if (frame->data_len >= 17)
            {
                uint32_t volume_cL = 0;
                uint32_t money = 0;
                uint16_t price = 0;
                
                /* Parse transaction data */
                sscanf((char*)frame->data, "%lu;%lu;%hu", &volume_cL, &money, &price);
                
                /* Convert cL to dL */
                uint32_t volume_dL = volume_cL / 10;
                
                event_out->type = PUMP_EVT_TRANSACTION;
                event_out->u.transaction.volume = volume_dL;
                event_out->u.transaction.money = money;
                event_out->u.transaction.price = price;
                
                return true;
            }
            break;
        }
        
        default:
            /* Unknown command - no event generated */
            break;
    }
    
    return false;
}
