// Core/Inc/Application/cli_driver.h

#ifndef CLI_DRIVER_H
#define CLI_DRIVER_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Inicializa o driver CLI com a UART de depura��o.
 */
void CLI_Init(UART_HandleTypeDef* debug_huart);

/**
 * @brief Processa comandos CLI pendentes (deve ser chamado no super-loop).
 */
void CLI_Process(void);

/**
 * @brief Callback chamado pela ISR da UART RX para cada caractere recebido.
 */
void CLI_Receive_Char(uint8_t received_char);

/**
 * @brief Fun��o de transmiss�o de baixo n�vel para retarget.c (printf).
 * Adiciona um caractere ao FIFO de transmiss�o e inicia a ISR de TX se necess�rio.
 * Esta fun��o � AT�MICA (segura contra interrup��es).
 */
void CLI_Printf_Transmit(uint8_t ch);

/**
 * @brief Callback interno chamado pela ISR HAL_UART_TxCpltCallback quando a TX da UART1 termina.
 * Esta � a "engine" de esvaziamento do FIFO.
 */
void CLI_HandleTxCplt(void);

/**
 * @brief Verifica se o FIFO de TX do CLI ainda tem dados para enviar.
 * @return true se o FIFO n�o estiver vazio ou a UART estiver ativamente enviando.
 */
bool CLI_IsTxBusy(void);


#endif // CLI_DRIVER_H