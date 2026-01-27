/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    ui_transactions.h
  * @brief   Transaction UI screens (SELECT/PRESET/ARMED/FUELLING/etc)
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef UI_TRANSACTIONS_H
#define UI_TRANSACTIONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ui.h"

/**
 * @brief Render transaction screens
 */
void UI_Trans_RenderSelectMode(UI_Context *ui);
void UI_Trans_RenderPresetVolume(UI_Context *ui);
void UI_Trans_RenderPresetMoney(UI_Context *ui);
void UI_Trans_RenderArmed(UI_Context *ui);
void UI_Trans_RenderFuelling(UI_Context *ui);
void UI_Trans_RenderPaused(UI_Context *ui);
void UI_Trans_RenderCompleted(UI_Context *ui);
void UI_Trans_RenderTotalizer(UI_Context *ui);

/**
 * @brief Handle keys for transaction screens
 * @return true if key was handled
 */
bool UI_Trans_HandleSelectMode(UI_Context *ui, char key);
bool UI_Trans_HandlePresetVolume(UI_Context *ui, char key);
bool UI_Trans_HandlePresetMoney(UI_Context *ui, char key);
bool UI_Trans_HandleArmed(UI_Context *ui, char key);
bool UI_Trans_HandleFuelling(UI_Context *ui, char key);
bool UI_Trans_HandlePaused(UI_Context *ui, char key);
bool UI_Trans_HandleCompleted(UI_Context *ui, char key);
bool UI_Trans_HandleTotalizer(UI_Context *ui, char key);

/**
 * @brief Background task for realtime polling during fuelling
 */
void UI_Trans_Task(UI_Context *ui);

#ifdef __cplusplus
}
#endif

#endif /* UI_TRANSACTIONS_H */
