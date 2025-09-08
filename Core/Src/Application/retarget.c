/*******************************************************************************
 * @file        retarget.c
 * @brief       Redirecionamento (Retarget) da função printf para UART.
 * @details     Este módulo permite que a função padrão `printf` envie seus dados
 * através de uma interface UART, o que é essencial para depuração
 * em sistemas embarcados. Ele suporta o redirecionamento dinâmico
 * para diferentes UARTs (ex: uma para debug, outra para um periférico).
 ******************************************************************************/

#include "retarget.h"
#include <stdio.h>

//==============================================================================
// Variáveis Estáticas e Globais
//==============================================================================

static UART_HandleTypeDef* s_debug_huart = NULL;
static UART_HandleTypeDef* s_dwin_huart = NULL;

// Variável global que define o destino atual do printf.
RetargetDestination_t g_retarget_dest = TARGET_DEBUG;

//==============================================================================
// Funções Públicas
//==============================================================================

// Inicializa o módulo de redirecionamento de I/O.
void Retarget_Init(UART_HandleTypeDef* debug_huart, UART_HandleTypeDef* dwin_huart)
{
    s_debug_huart = debug_huart;
    s_dwin_huart = dwin_huart;

    // Desativa o buffer de saída do stdout (saída padrão).
    // O modo _IONBF (I/O Not Buffered) garante que cada caractere enviado
    // para printf seja transmitido imediatamente pela UART, sem aguardar
    // um caractere de nova linha '\n' ou o preenchimento do buffer.
    setvbuf(stdout, NULL, _IONBF, 0);
}


//==============================================================================
// Reimplementação de Funções da Biblioteca C Padrão
//==============================================================================

// Reimplementação da função fputc.
int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;

    // Envia o caractere para o destino atualmente selecionado na variável global
    switch (g_retarget_dest)
    {
        case TARGET_DWIN:
            if (s_dwin_huart != NULL)
            {
                HAL_UART_Transmit(s_dwin_huart, &c, 1, HAL_MAX_DELAY);
            }
            break;

        case TARGET_DEBUG:
        default:
            if (s_debug_huart != NULL)
            {
                HAL_UART_Transmit(s_debug_huart, &c, 1, HAL_MAX_DELAY);
            }
            break;
    }

    return ch;
}

// Implementação stub da função ferror.
int ferror(FILE *f)
{
    // Simplesmente retorna 0 para indicar "nenhum erro".
    return 0;
}