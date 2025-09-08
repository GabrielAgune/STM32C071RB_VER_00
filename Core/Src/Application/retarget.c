/*******************************************************************************
 * @file        retarget.c
 * @brief       Redirecionamento (Retarget) da fun��o printf para UART.
 * @details     Este m�dulo permite que a fun��o padr�o `printf` envie seus dados
 * atrav�s de uma interface UART, o que � essencial para depura��o
 * em sistemas embarcados. Ele suporta o redirecionamento din�mico
 * para diferentes UARTs (ex: uma para debug, outra para um perif�rico).
 ******************************************************************************/

#include "retarget.h"
#include <stdio.h>

//==============================================================================
// Vari�veis Est�ticas e Globais
//==============================================================================

static UART_HandleTypeDef* s_debug_huart = NULL;
static UART_HandleTypeDef* s_dwin_huart = NULL;

// Vari�vel global que define o destino atual do printf.
RetargetDestination_t g_retarget_dest = TARGET_DEBUG;

//==============================================================================
// Fun��es P�blicas
//==============================================================================

// Inicializa o m�dulo de redirecionamento de I/O.
void Retarget_Init(UART_HandleTypeDef* debug_huart, UART_HandleTypeDef* dwin_huart)
{
    s_debug_huart = debug_huart;
    s_dwin_huart = dwin_huart;

    // Desativa o buffer de sa�da do stdout (sa�da padr�o).
    // O modo _IONBF (I/O Not Buffered) garante que cada caractere enviado
    // para printf seja transmitido imediatamente pela UART, sem aguardar
    // um caractere de nova linha '\n' ou o preenchimento do buffer.
    setvbuf(stdout, NULL, _IONBF, 0);
}


//==============================================================================
// Reimplementa��o de Fun��es da Biblioteca C Padr�o
//==============================================================================

// Reimplementa��o da fun��o fputc.
int fputc(int ch, FILE *f)
{
    uint8_t c = (uint8_t)ch;

    // Envia o caractere para o destino atualmente selecionado na vari�vel global
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

// Implementa��o stub da fun��o ferror.
int ferror(FILE *f)
{
    // Simplesmente retorna 0 para indicar "nenhum erro".
    return 0;
}