// Core/Inc/Application/cli_driver.h

#ifndef CLI_DRIVER_H
#define CLI_DRIVER_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Inicializa o driver CLI com a UART de depuração. (Inicia o RX IT de 1 byte).
 */
void CLI_Init(UART_HandleTypeDef* debug_huart);

/**
 * @brief Processa comandos CLI pendentes (deve ser chamado no super-loop).
 */
void CLI_Process(void);

/**
 * @brief "Bomba" de TX do CLI. Chamada pelo super-loop para enviar dados do FIFO via DMA.
 */
void CLI_TX_Pump(void);

/**
 * @brief Função de transmissão de baixo nível para retarget.c (printf).
 * Adiciona um caractere ao FIFO de transmissão (atômico).
 */
void CLI_Printf_Transmit(uint8_t ch);

// --- Handlers de ISR (Chamados pelos Callbacks do HAL em stm32c0xx_it.c) ---
void CLI_HandleTxCplt(UART_HandleTypeDef *huart);
void CLI_HandleRxCplt(UART_HandleTypeDef *huart);
void CLI_HandleError(UART_HandleTypeDef *huart);

#endif // CLI_DRIVER_H