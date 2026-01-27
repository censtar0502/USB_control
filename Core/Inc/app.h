#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "pump_proto.h"
#include "pump_proto_gkl.h"
#include "pump_mgr.h"
#include "settings.h"
#include "ui.h"

/* Application context */
typedef struct
{
    /* TRK1 protocol instance (USART2) */
    PumpProtoGKL gkl1;
    PumpProto    proto1;

    /* TRK2 protocol instance (USART3) */
    PumpProtoGKL gkl2;
    PumpProto    proto2;

    /* Pump manager (protocol-agnostic) */
    PumpMgr      mgr;

    /* Non-volatile settings (EEPROM) */
    Settings     settings;

    /* OLED UI context */
    UI_Context   ui;

    /* Event queue for UI events (simple implementation) */
    struct {
        PumpEvent events[8];
        uint8_t head;
        uint8_t tail;
    } event_queue;

} AppContext;

void APP_Init(UART_HandleTypeDef *huart_trk1, UART_HandleTypeDef *huart_trk2, I2C_HandleTypeDef *hi2c);
void APP_Task(void);

/* Event queue functions */
void APP_PushEvent(PumpEvent *ev);
bool APP_PopEvent(PumpEvent *ev);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
