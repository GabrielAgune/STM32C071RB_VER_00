/*******************************************************************************
 * @file        cli_driver.c
 * @brief       CLI Driver Coordinator (Refactored V8.3)
 * @version     8.3 (Refactored for better modularity)
 * @details     Thin coordinator layer that orchestrates CLI transport and commands.
 *              Core functionality moved to cli_transport.c and cli_commands.c.
 ******************************************************************************/

#include "cli_driver.h"
#include "cli_transport.h" 
#include "cli_commands.h"
#include <stdio.h>
#include <string.h>

//================================================================================
// Definitions
//================================================================================
#define CLI_COMMAND_BUFFER_SIZE 128

//================================================================================
// Static Variables
//================================================================================
static char s_command_buffer[CLI_COMMAND_BUFFER_SIZE];

//================================================================================
// Public Functions
//================================================================================

void CLI_Init(UART_HandleTypeDef* debug_huart) 
{
    CLI_Transport_Init(debug_huart);
    printf("\r\nCLI Pronta. Digite 'HELP' para comandos.\r\n> ");
}

void CLI_Process(void) 
{
    if (CLI_Transport_IsCommandReady()) {
        printf("\r\n");
        
        size_t len = CLI_Transport_GetCommand(s_command_buffer, CLI_COMMAND_BUFFER_SIZE);
        if (len > 0) {
            CLI_Commands_Process(s_command_buffer);
        }
        
        printf("\r\n> ");
    }
}

void CLI_TX_Pump(void)
{
    CLI_Transport_TxPump();
}

void CLI_Printf_Transmit(uint8_t ch)
{
    CLI_Transport_TransmitChar(ch);
}

//================================================================================
// ISR Handler Wrappers
//================================================================================

void CLI_HandleTxCplt(UART_HandleTypeDef *huart)
{
    CLI_Transport_HandleTxComplete(huart);
}

void CLI_HandleRxCplt(UART_HandleTypeDef *huart)
{
    CLI_Transport_HandleRxComplete(huart);
}

void CLI_HandleError(UART_HandleTypeDef *huart)
{
    CLI_Transport_HandleError(huart);
}