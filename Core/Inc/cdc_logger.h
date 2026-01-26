#ifndef CDC_LOGGER_H
#define CDC_LOGGER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*
 * Non-blocking USB CDC logger with ring-buffer.
 * - Push() puts bytes into ring (drops whole message if no space).
 * - Task() sends when USB is ready (no blocking).
 * - TxCpltCallback() must be called from CDC_TransmitCplt_FS.
 */

void CDC_LOG_Init(void);
void CDC_LOG_Push(const char *s);
void CDC_LOG_Task(void);

/**
 * @brief Simple helper for logging a single message line to CDC logger.
 *        Adds CRLF after the message.
 * @param msg Zero-terminated string
 */
void CDC_Log(const char *msg);
void CDC_LOG_TxCpltCallback(void);
uint32_t CDC_LOG_GetDroppedCount(void);

#ifdef __cplusplus
}
#endif

#endif /* CDC_LOGGER_H */
