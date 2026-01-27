/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app.h
  * @brief   Application top-level (UI + Pump Manager + Protocol adapters)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32h7xx_hal.h"

void APP_Init(UART_HandleTypeDef *huart_pump1,
              UART_HandleTypeDef *huart_pump2,
              I2C_HandleTypeDef  *hi2c_eeprom);

void APP_Task(void);

#ifdef __cplusplus
}
// Выбрать какой ТРК логировать
#define PUMP_GKL_LOG_TARGET 1  // Только TRK1


// Использовать компактный формат логов (как в эталонном логе)
#define PUMP_GKL_COMPACT_LOG 1  // Включить компактный формат

// Отключить verbose логирование
#define PUMP_GKL_TRACE_FRAMES 0
#endif

#endif /* APP_H */
