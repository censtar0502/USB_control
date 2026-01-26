#ifndef __SSD1309_H
#define __SSD1309_H

#include "main.h"

/* Определения размеров дисплея */
#define SSD1309_WIDTH  128
#define SSD1309_HEIGHT 64

/* Определения цветов */
#define BLACK 0
#define WHITE 1

/* Прототипы функций */
void SSD1309_Init(void);
void SSD1309_UpdateScreen(void);
void SSD1309_Fill(uint8_t color);
void SSD1309_SetCursor(uint8_t x, uint8_t y);
void SSD1309_WriteChar(char ch, uint8_t color);
void SSD1309_WriteString(const char *str, uint8_t color);

#endif /* __SSD1309_H */
