/* app.h - With FSM */
#ifndef APP_H
#define APP_H

#include "main.h"
#include "pump_proto_gkl.h"
#include "pump_mgr.h"
#include "settings.h"
#include "transaction_fsm.h"
#include "ui.h"

typedef struct {
    /* Protocol */
    PumpProtoGKL gkl1;
    PumpProto proto1;
    
    PumpProtoGKL gkl2;
    PumpProto proto2;
    
    /* Manager */
    PumpMgr mgr;
    
    /* FSM */
    TransactionFSM trk1_fsm;
    TransactionFSM trk2_fsm;
    
    /* Settings */
    Settings settings;
    
    /* UI */
    UI_Context ui;
    
} AppContext;

void APP_Init(UART_HandleTypeDef *huart_trk1, UART_HandleTypeDef *huart_trk2, I2C_HandleTypeDef *hi2c);
void APP_Task(void);
void APP_OnKeyPress(char key);

#endif
