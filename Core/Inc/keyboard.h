#ifndef __KEYBOARD_H
#define __KEYBOARD_H

#include "main.h"

// Конфигурация матрицы 5x4
#define KEY_ROWS 5
#define KEY_COLS 4

// Публичные функции
void KEYBOARD_Init(void);
void KEYBOARD_Scan_Process(void); // Вызывать в прерывании таймера (10мс)
char KEYBOARD_GetKey(void);       // Получить символ нажатой клавиши (0 если ничего нет)

#endif /* __KEYBOARD_H */
