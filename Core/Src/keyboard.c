#include "keyboard.h"

// Карта клавиш согласно ТЗ
static const char KeyMap[KEY_ROWS][KEY_COLS] = {
		    {'H', 'G', 'F', 'A'},
		    {'3', '2', '1', 'B'},
		    {'6', '5', '4', 'C'},
		    {'9', '8', '7', 'D'},
		    {'K', '0', '.', 'E'}
};

// Порты и пины строк (Outputs)
static GPIO_TypeDef* RowPorts[KEY_ROWS] = {GPIOB, GPIOE, GPIOE, GPIOE, GPIOE};
static uint16_t RowPins[KEY_ROWS] = {GPIO_PIN_2, GPIO_PIN_7, GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_10};

// Порты и пины столбцов (Inputs с Pull-up)
static GPIO_TypeDef* ColPorts[KEY_COLS] = {GPIOE, GPIOE, GPIOE, GPIOE};
static uint16_t ColPins[KEY_COLS] = {GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13, GPIO_PIN_14};

static volatile char ActiveKey = 0;
static volatile uint8_t KeyReady = 0;

void KEYBOARD_Init(void) {
    // Убеждаемся, что все строки в High (неактивны)
    for(uint8_t r = 0; r < KEY_ROWS; r++) {
        HAL_GPIO_WritePin(RowPorts[r], RowPins[r], GPIO_PIN_SET);
    }
}

/**
 * Основная логика сканирования с антидребезгом.
 * Вызывается каждые 10 мс из HAL_TIM_PeriodElapsedCallback.
 */
void KEYBOARD_Scan_Process(void) {
    static uint8_t debounce_timer = 0;
    static char candidate_key = 0;
    char current_detected = 0;

    // 1. Поиск нажатой клавиши
    for (uint8_t r = 0; r < KEY_ROWS; r++) {
        HAL_GPIO_WritePin(RowPorts[r], RowPins[r], GPIO_PIN_RESET); // Активируем строку

        for (uint8_t c = 0; c < KEY_COLS; c++) {
            if (HAL_GPIO_ReadPin(ColPorts[c], ColPins[c]) == GPIO_PIN_RESET) {
                current_detected = KeyMap[r][c];
                break;
            }
        }

        HAL_GPIO_WritePin(RowPorts[r], RowPins[r], GPIO_PIN_SET); // Деактивируем строку
        if (current_detected) break;
    }

    // 2. Алгоритм антидребезга
    if (current_detected != 0 && current_detected == candidate_key) {
        if (++debounce_timer >= 3) { // Удержание 30 мс
            if (!KeyReady) {
                ActiveKey = current_detected;
                KeyReady = 1;
            }
            debounce_timer = 3;
        }
    } else {
        debounce_timer = 0;
        candidate_key = current_detected;
        if (current_detected == 0) KeyReady = 0; // Кнопка отпущена
    }
}

char KEYBOARD_GetKey(void) {
    if (KeyReady && ActiveKey != 0) {
        char temp = ActiveKey;
        ActiveKey = 0; // Сбрасываем после чтения, чтобы не дублировать
        return temp;
    }
    return 0;
}
