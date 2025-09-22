#ifndef CLI_TRANSPORT_H
#define CLI_TRANSPORT_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * @file cli_transport.h
 * @brief CLI Transport Layer - UART/DMA Management
 * @details Handles low-level UART communication with DMA and FIFO buffering
 */

/**
 * @brief Initialize the CLI transport layer
 * @param debug_huart Pointer to UART handle for CLI communication
 */
void CLI_Transport_Init(UART_HandleTypeDef* debug_huart);

/**
 * @brief Check if a complete command is ready for processing
 * @return true if command is ready, false otherwise
 */
bool CLI_Transport_IsCommandReady(void);

/**
 * @brief Get the received command and mark it as processed
 * @param buffer Buffer to copy the command into
 * @param max_length Maximum length of the buffer
 * @return Number of characters copied
 */
size_t CLI_Transport_GetCommand(char* buffer, size_t max_length);

/**
 * @brief Transmit pump - processes TX FIFO and sends data via DMA
 * @note Should be called regularly from main loop
 */
void CLI_Transport_TxPump(void);

/**
 * @brief Add a character to the transmit FIFO (non-blocking)
 * @param ch Character to transmit
 */
void CLI_Transport_TransmitChar(uint8_t ch);

/**
 * @brief Print a string to the CLI output
 * @param str String to transmit
 */
void CLI_Transport_Print(const char* str);

// ISR Handlers - called from HAL callbacks
void CLI_Transport_HandleTxComplete(UART_HandleTypeDef *huart);
void CLI_Transport_HandleRxComplete(UART_HandleTypeDef *huart);
void CLI_Transport_HandleError(UART_HandleTypeDef *huart);

#endif // CLI_TRANSPORT_H