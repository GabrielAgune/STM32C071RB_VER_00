/*******************************************************************************
 * @file        cli_transport.c
 * @brief       CLI Transport Layer - UART/DMA Management
 * @version     8.3 (Extracted from cli_driver.c during refactoring)
 * @details     Handles low-level UART communication with DMA and FIFO buffering.
 *              Provides a clean interface for CLI communication.
 ******************************************************************************/

#include "cli_transport.h"
#include <string.h>
#include <ctype.h>

//================================================================================
// Definitions
//================================================================================
#define CLI_RX_BUFFER_SIZE      128
#define CLI_TX_FIFO_SIZE        1024
#define CLI_TX_DMA_BUFFER_SIZE  64  

//================================================================================
// Static Variables
//================================================================================
static UART_HandleTypeDef* s_huart_debug = NULL;

// RX (Interrupt-driven single byte)
static uint8_t s_cli_rx_byte; 
static char s_cli_rx_buffer[CLI_RX_BUFFER_SIZE]; 
static uint16_t s_cli_rx_index = 0;
static volatile bool s_command_ready = false;

// TX (Software FIFO + DMA)
static uint8_t s_cli_tx_fifo[CLI_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0;
static volatile uint16_t s_tx_fifo_tail = 0;
static uint8_t s_cli_tx_dma_buffer[CLI_TX_DMA_BUFFER_SIZE]; 
static volatile bool s_dma_tx_busy = false; 

//================================================================================
// Public Functions
//================================================================================

void CLI_Transport_Init(UART_HandleTypeDef* debug_huart)
{
    s_huart_debug = debug_huart;
    if(HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1) != HAL_OK) {
        Error_Handler();
    }
}

bool CLI_Transport_IsCommandReady(void)
{
    return s_command_ready;
}

size_t CLI_Transport_GetCommand(char* buffer, size_t max_length)
{
    if (!s_command_ready || buffer == NULL) {
        return 0;
    }
    
    size_t len = strlen(s_cli_rx_buffer);
    if (len >= max_length) {
        len = max_length - 1;
    }
    
    memcpy(buffer, s_cli_rx_buffer, len);
    buffer[len] = '\0';
    
    // Reset for next command
    memset(s_cli_rx_buffer, 0, CLI_RX_BUFFER_SIZE);
    s_cli_rx_index = 0;
    s_command_ready = false;
    
    return len;
}

void CLI_Transport_TxPump(void)
{
    if (s_dma_tx_busy || (s_tx_fifo_head == s_tx_fifo_tail)) {
        return;
    }

    // Critical section
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    
    if (s_dma_tx_busy) { // Double check
        HAL_NVIC_EnableIRQ(USART1_IRQn);
        HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
        return;
    }
    
    s_dma_tx_busy = true;
    
    uint16_t bytes_to_send = 0;
    while ((s_tx_fifo_tail != s_tx_fifo_head) && (bytes_to_send < CLI_TX_DMA_BUFFER_SIZE)) {
        s_cli_tx_dma_buffer[bytes_to_send] = s_cli_tx_fifo[s_tx_fifo_tail];
        s_tx_fifo_tail = (s_tx_fifo_tail + 1) % CLI_TX_FIFO_SIZE;
        bytes_to_send++;
    }

    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    if (HAL_UART_Transmit_DMA(s_huart_debug, s_cli_tx_dma_buffer, bytes_to_send) != HAL_OK) {
        s_dma_tx_busy = false; 
    }
}

void CLI_Transport_TransmitChar(uint8_t ch)
{
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);

    // Auto-insert \r before \n for proper terminal display
    if (ch == '\n') {
        uint16_t next_head = (s_tx_fifo_head + 1) % CLI_TX_FIFO_SIZE;
        if (next_head != s_tx_fifo_tail) { 
            s_cli_tx_fifo[s_tx_fifo_head] = '\r';
            s_tx_fifo_head = next_head;
        }
    }

    uint16_t next_head = (s_tx_fifo_head + 1) % CLI_TX_FIFO_SIZE;
    if (next_head != s_tx_fifo_tail) { 
        s_cli_tx_fifo[s_tx_fifo_head] = ch;
        s_tx_fifo_head = next_head;
    }
    
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

void CLI_Transport_Print(const char* str)
{
    if (str == NULL) return;
    
    while (*str) {
        CLI_Transport_TransmitChar(*str++);
    }
}

//================================================================================
// ISR Handlers
//================================================================================

void CLI_Transport_HandleRxComplete(UART_HandleTypeDef *huart)
{
    if (s_command_ready) {
        // Ignore byte. Main loop hasn't processed previous command yet.
    }
    else if (s_cli_rx_byte == '\r' || s_cli_rx_byte == '\n') {
        if (s_cli_rx_index > 0) {
            s_cli_rx_buffer[s_cli_rx_index] = '\0'; 
            s_command_ready = true; 
        } else {
            // Echo empty line
            CLI_Transport_TransmitChar('\r');
            CLI_Transport_TransmitChar('\n');
            CLI_Transport_TransmitChar('>');
            CLI_Transport_TransmitChar(' ');
        }
    } 
    else if (s_cli_rx_byte == '\b' || s_cli_rx_byte == 127) { // Backspace
        if (s_cli_rx_index > 0) {
            s_cli_rx_index--;
            CLI_Transport_TransmitChar('\b'); 
            CLI_Transport_TransmitChar(' ');
            CLI_Transport_TransmitChar('\b');
        }
    } 
    else if (s_cli_rx_index < (CLI_RX_BUFFER_SIZE - 1) && isprint(s_cli_rx_byte)) {
        s_cli_rx_buffer[s_cli_rx_index++] = (char)s_cli_rx_byte;
        CLI_Transport_TransmitChar(s_cli_rx_byte); // Echo character
    }
    
    // Re-arm interrupt for next byte
    if (HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1) != HAL_OK) {
         HAL_UART_AbortReceive_IT(s_huart_debug);
         HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1);
    }
}

void CLI_Transport_HandleTxComplete(UART_HandleTypeDef *huart)
{
    s_dma_tx_busy = false; 
}

void CLI_Transport_HandleError(UART_HandleTypeDef *huart)
{
    // Error recovery - restart RX
    HAL_UART_AbortReceive_IT(s_huart_debug);
    HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1);
}