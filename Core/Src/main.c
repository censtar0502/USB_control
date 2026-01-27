/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1309.h"
#include "keyboard.h"
#include "usbd_cdc_if.h"
#include <stdio.h>
#include <string.h>
#include "gkl_link.h"
#include "cdc_logger.h"

#include "app.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi2;
DMA_HandleTypeDef hdma_spi2_tx;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
DMA_HandleTypeDef hdma_usart2_rx;
DMA_HandleTypeDef hdma_usart2_tx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart3_tx;

/* USER CODE BEGIN PV */
uint32_t last_alive_tick = 0;
char msg_buf[128];

/* Debug heartbeat in log (disable by default to reduce noise) */
#ifndef SYS_UPTIME_LOG_ENABLE
#define SYS_UPTIME_LOG_ENABLE   (0u)
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */
void System_Log(const char *message);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief Коллбэк таймера для сканирования клавиатуры
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	if (htim->Instance == TIM3) {
		KEYBOARD_Scan_Process();
	}
}

/**
 * @brief Логирование только по USB CDC (через очередь, без потерь и без блокировок CPU)
 */
void System_Log(const char *message) {
	CDC_LOG_Push(message);
}

/**
 * @brief UART TX complete callback
 * @note  USART2/USART3 используются для связи с ТРК (GKL или другой протокол в будущем).
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
	/* Forward to GasKitLink driver dispatcher (supports multiple UART links) */
	GKL_Global_UART_TxCpltCallback(huart);
}

/**
 * @brief UART RX complete callback
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	/* Forward to GasKitLink driver dispatcher (supports multiple UART links) */
	GKL_Global_UART_RxCpltCallback(huart);
}

/**
 * @brief UART error callback
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart) {
	/* Forward to GasKitLink driver dispatcher (supports multiple UART links) */
	GKL_Global_UART_ErrorCallback(huart);
}
/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* Настройка MPU для корректной работы памяти */
	MPU_Config();

	/* Включение кэшей для производительности H7 */
	SCB_EnableICache();
	SCB_EnableDCache();

	/* Сброс всей периферии, инициализация Flash интерфейса и Systick. */
	HAL_Init();

	/* Настройка тактирования системы */
	SystemClock_Config();

	/* Инициализация всей сконфигурированной периферии */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_I2C1_Init();
	MX_SPI2_Init();
	MX_TIM2_Init();
	MX_TIM3_Init();
	MX_USART2_UART_Init();
	MX_USART3_UART_Init();
	MX_USB_DEVICE_Init();

	/* USER CODE BEGIN 2 */
	CDC_LOG_Init();
	HAL_Delay(500);
	System_Log(">>> System Booting...\r\n");

	/* Аппаратный сброс дисплея с использованием новых меток из main.h */
	HAL_GPIO_WritePin(SPI2_RST_GPIO_Port, SPI2_RST_Pin, GPIO_PIN_RESET);
	HAL_Delay(100);
	HAL_GPIO_WritePin(SPI2_RST_GPIO_Port, SPI2_RST_Pin, GPIO_PIN_SET);
	HAL_Delay(200);

	/* Инициализация SSD1309 */
	SSD1309_Init();

	/* Очистка экрана от мусора (белого шума) */
	SSD1309_Fill(0);
	SSD1309_UpdateScreen();
	HAL_Delay(100);

	/* Инициализация модуля клавиатуры */
	KEYBOARD_Init();

	/* Вывод приветственного сообщения */
	SSD1309_SetCursor(30, 10);
	SSD1309_WriteString("H750 CONTROL", 1);
	SSD1309_SetCursor(30, 30);
	SSD1309_WriteString("STATUS: READY", 1);

	SSD1309_UpdateScreen();

	System_Log(">>> Display Initialized. Starting Timers.\r\n");

	/* Запуск таймера сканирования клавиатуры (прерывания) */
	HAL_TIM_Base_Start_IT(&htim3);

	/* Application init (protocol plugins + UI + managers)
	 NOTE: USART2/USART3 are reserved for TRK links (no USART2 logging). */
	APP_Init(&huart2, &huart3, &hi2c1);
	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* Run application (USB logging, protocol plugins, UI, managers) */
		APP_Task();

		/* Heartbeat log is optional (disabled by default) */
#if (SYS_UPTIME_LOG_ENABLE)
    if (HAL_GetTick() - last_alive_tick >= 5000u) {
        last_alive_tick = HAL_GetTick();
        snprintf(msg_buf, sizeof(msg_buf), "SYS: Uptime %lu sec\r\n", (unsigned long)(last_alive_tick/1000u));
        System_Log(msg_buf);
    }
#endif
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Supply configuration update enable
	 */
	HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

	/** Configure the main internal regulator output voltage
	 */
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

	while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {
	}

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE
			| RCC_OSCILLATORTYPE_HSI48;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 5;
	RCC_OscInitStruct.PLL.PLLN = 192;
	RCC_OscInitStruct.PLL.PLLP = 2;
	RCC_OscInitStruct.PLL.PLLQ = 15;
	RCC_OscInitStruct.PLL.PLLR = 2;
	RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
	RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
	RCC_OscInitStruct.PLL.PLLFRACN = 0;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1
			| RCC_CLOCKTYPE_D1PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
	RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief I2C1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C1_Init(void) {
	hi2c1.Instance = I2C1;
	hi2c1.Init.Timing = 0x00B03FDB;
	hi2c1.Init.OwnAddress1 = 0;
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c1.Init.OwnAddress2 = 0;
	hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief SPI2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI2_Init(void) {
	/* SPI2 parameter configuration*/
	hspi2.Instance = SPI2;
	hspi2.Init.Mode = SPI_MODE_MASTER;
	hspi2.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
	hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi2.Init.NSS = SPI_NSS_SOFT;
	/* Установлен делитель 32 для предотвращения ошибок на OLED дисплее */
	hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
	hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi2.Init.CRCPolynomial = 0x0;
	hspi2.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
	hspi2.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
	hspi2.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
	hspi2.Init.TxCRCInitializationPattern =
			SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	hspi2.Init.RxCRCInitializationPattern =
			SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
	hspi2.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
	hspi2.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
	hspi2.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
	hspi2.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
	hspi2.Init.IOSwap = SPI_IO_SWAP_DISABLE;
	if (HAL_SPI_Init(&hspi2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief TIM2 Initialization Function (System Tick replacement/General)
 */
static void MX_TIM2_Init(void) {
	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 4799;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 499;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief TIM3 Initialization Function (Keyboard Scan Timer)
 */
static void MX_TIM3_Init(void) {
	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 479;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 499;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief USART2 Initialization Function (System Logs)
 */
static void MX_USART2_UART_Init(void) {
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 9600;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief USART3 Initialization Function (Protocol Traffic)
 */
static void MX_USART3_UART_Init(void) {
	huart3.Instance = USART3;
	huart3.Init.BaudRate = 9600;
	huart3.Init.WordLength = UART_WORDLENGTH_8B;
	huart3.Init.StopBits = UART_STOPBITS_1;
	huart3.Init.Parity = UART_PARITY_NONE;
	huart3.Init.Mode = UART_MODE_TX_RX;
	huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart3.Init.OverSampling = UART_OVERSAMPLING_16;
	huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart3) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief Enable DMA controller clock
 */
static void MX_DMA_Init(void) {
	/* DMA controller clock enable */
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Stream1_IRQn interrupt configuration (USART2_RX) */
	HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
	/* DMA1_Stream2_IRQn interrupt configuration (USART2_TX) */
	HAL_NVIC_SetPriority(DMA1_Stream2_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream2_IRQn);
	/* DMA1_Stream3_IRQn interrupt configuration (USART3_RX) */
	HAL_NVIC_SetPriority(DMA1_Stream3_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream3_IRQn);
	/* DMA1_Stream4_IRQn interrupt configuration (USART3_TX) */
	HAL_NVIC_SetPriority(DMA1_Stream4_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream4_IRQn);
	/* DMA1_Stream5_IRQn interrupt configuration (SPI2_TX) */
	HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 2, 0);
	HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

/**
 * @brief GPIO Initialization Function
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();

	/* Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB,
			SPI2_DC_Pin | KeyRow_1_Pin | SPI2_CS_Pin | SPI2_RST_Pin,
			GPIO_PIN_SET);
	HAL_GPIO_WritePin(GPIOE,
			KeyRow_2_Pin | KeyRow_3_Pin | KeyRow_4_Pin | KeyRow_5_Pin,
			GPIO_PIN_SET);

	/* Configure GPIO pins : SPI2_DC_Pin KeyRow_1_Pin SPI2_CS_Pin SPI2_RST_Pin */
	GPIO_InitStruct.Pin = SPI2_DC_Pin | KeyRow_1_Pin | SPI2_CS_Pin
			| SPI2_RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* Configure GPIO pins : KeyRow_2_Pin KeyRow_3_Pin KeyRow_4_Pin KeyRow_5_Pin */
	GPIO_InitStruct.Pin = KeyRow_2_Pin | KeyRow_3_Pin | KeyRow_4_Pin
			| KeyRow_5_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

	/* Configure GPIO pins : KeyCol_1_Pin KeyCol_2_Pin KeyCol_3_Pin KeyCol_4_Pin */
	GPIO_InitStruct.Pin = KeyCol_1_Pin | KeyCol_2_Pin | KeyCol_3_Pin
			| KeyCol_4_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
}

/**
 * @brief MPU Configuration
 */
static void MPU_Config(void) {
	MPU_Region_InitTypeDef MPU_InitStruct = { 0 };

	/* Disables the MPU */
	HAL_MPU_Disable();

	/** Initializes and configures the Region and the memory to be protected
	 */
	MPU_InitStruct.Enable = MPU_REGION_ENABLE;
	MPU_InitStruct.Number = MPU_REGION_NUMBER0;
	MPU_InitStruct.BaseAddress = 0x24000000;
	MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;
	MPU_InitStruct.SubRegionDisable = 0x00;
	MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
	MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
	MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
	MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
	MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

	HAL_MPU_ConfigRegion(&MPU_InitStruct);
	/* Enables the MPU */
	HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	__disable_irq();
	while (1) {
		CDC_LOG_Task();

	}
	/* USER CODE END Error_Handler_Debug */
}
