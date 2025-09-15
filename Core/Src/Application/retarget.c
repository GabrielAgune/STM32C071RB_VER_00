/*******************************************************************************
 * @file        retarget.c
 * @brief       Redirecionamento (Retarget) da função printf para UART (NÃO-BLOQUEANTE).
 * @version     2.1 (Refatorado por Dev STM para usar TX FIFO assíncrono do CLI)
 * @details     Este módulo redireciona o fputc (usado pelo printf) para o
 * driver CLI, que gerencia um buffer circular (FIFO) de transmissão
 * para garantir que chamadas printf NUNCA bloqueiem o sistema.
 ******************************************************************************/

#include "retarget.h"
#include <stdio.h>
#include "cli_driver.h" // Dependência principal para TX assíncrono

//==============================================================================
// Variáveis Estáticas e Globais
//==============================================================================

static UART_HandleTypeDef* s_debug_huart = NULL;
static UART_HandleTypeDef* s_dwin_huart = NULL;

RetargetDestination_t g_retarget_dest = TARGET_DEBUG;

//==============================================================================
// Funções Públicas
//==============================================================================

void Retarget_Init(UART_HandleTypeDef* debug_huart, UART_HandleTypeDef* dwin_huart)
{
    s_debug_huart = debug_huart;
    s_dwin_huart = dwin_huart;

    // Garante que a biblioteca C chame fputc para CADA caractere, imediatamente.
    setvbuf(stdout, NULL, _IONBF, 0);
}


//==============================================================================
// Reimplementação de Funções da Biblioteca C Padrão (REFATORADO)
//==============================================================================

/**
 * @brief Reimplementação NÃO-BLOQUEANTE do fputc.
 */
int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;

    // Ignora escritas destinadas ao DWIN (printf não deve ir para o DWIN)
    if (g_retarget_dest == TARGET_DEBUG)
    {
        if (s_debug_huart != NULL)
        {
            // Chama a função atômica do driver CLI que enfileira o byte no FIFO
            // e retorna imediatamente.
            CLI_Printf_Transmit(c);
        }
    }
    
    return ch;
}

int ferror(FILE *f)
{
    return 0;
}