#include "dwin_driver.h"
#include <string.h>

#define DWIN_RX_BUFFER_SIZE     64
#define DWIN_TEXT_PAYLOAD_LEN   20
#define DWIN_TEXT_TERMINATOR    0xFF
#define UART_TIMEOUT_MS         100

static UART_HandleTypeDef* s_huart;
static dwin_rx_callback_t s_rx_callback = NULL;
static uint8_t s_rx_buffer[DWIN_RX_BUFFER_SIZE];
static volatile bool s_frame_received = false;
static volatile uint16_t s_received_len = 0;

void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback) {
    s_huart = huart;
    s_rx_callback = callback;
    if (HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

void DWIN_Driver_Process(void) {
    if (!s_frame_received) return;
    uint8_t local_buffer[DWIN_RX_BUFFER_SIZE];
    uint16_t local_len;
    __disable_irq();
    local_len = s_received_len;
    memcpy(local_buffer, s_rx_buffer, local_len);
    s_frame_received = false;
    __enable_irq();
    if (local_len >= 3 && local_buffer[0] == 0x5A && local_buffer[1] == 0xA5) {
        if (s_rx_callback != NULL) {
            s_rx_callback(local_buffer, local_len);
        }
    }
}

void DWIN_Driver_SetScreen(uint16_t screen_id) {
    uint8_t cmd_buffer[] = { 0x5A, 0xA5, 0x07, 0x82, 0x00, 0x84, 0x5A, 0x01, (uint8_t)(screen_id >> 8), (uint8_t)screen_id };
    HAL_UART_Transmit(s_huart, cmd_buffer, sizeof(cmd_buffer), UART_TIMEOUT_MS);
}

void DWIN_Driver_WriteInt(uint16_t vp_address, int16_t value) {
    uint8_t cmd_buffer[] = { 0x5A, 0xA5, 0x05, 0x82, (uint8_t)(vp_address >> 8), (uint8_t)vp_address, (uint8_t)(value >> 8), (uint8_t)value };
    HAL_UART_Transmit(s_huart, cmd_buffer, sizeof(cmd_buffer), UART_TIMEOUT_MS);
}

// NOVA FUNÇÃO IMPLEMENTADA
void DWIN_Driver_WriteInt32(uint16_t vp_address, int32_t value) {
    uint8_t cmd_buffer[] = {
        0x5A, 0xA5, 0x07, 0x82,                         // Header, Length, Command
        (uint8_t)(vp_address >> 8), (uint8_t)vp_address, // VP Address
        (uint8_t)(value >> 24), (uint8_t)(value >> 16), // Value MSB
        (uint8_t)(value >> 8), (uint8_t)value          // Value LSB
    };
    HAL_UART_Transmit(s_huart, cmd_buffer, sizeof(cmd_buffer), UART_TIMEOUT_MS);
}

void DWIN_Driver_WriteString(uint16_t vp_address, const char* text) {
    uint8_t frame_len = 3 + 2 + DWIN_TEXT_PAYLOAD_LEN; // 3 header + 2 addr + payload
    uint8_t cmd_buffer[frame_len];
    size_t text_len = strlen(text);
    if (text_len > DWIN_TEXT_PAYLOAD_LEN) text_len = DWIN_TEXT_PAYLOAD_LEN;

    cmd_buffer[0] = 0x5A;
    cmd_buffer[1] = 0xA5;
    cmd_buffer[2] = 3 + DWIN_TEXT_PAYLOAD_LEN; // Length of data part (cmd+addr+payload)
    cmd_buffer[3] = 0x82;
    cmd_buffer[4] = (uint8_t)(vp_address >> 8);
    cmd_buffer[5] = (uint8_t)(vp_address & 0xFF);
    memcpy(&cmd_buffer[6], text, text_len);
    memset(&cmd_buffer[6 + text_len], DWIN_TEXT_TERMINATOR, DWIN_TEXT_PAYLOAD_LEN - text_len);
    HAL_UART_Transmit(s_huart, cmd_buffer, sizeof(cmd_buffer), UART_TIMEOUT_MS);
}

void DWIN_Driver_WriteRawBytes(const uint8_t* data, uint16_t size) {
    if (s_huart != NULL && data != NULL && size > 0) {
        HAL_UART_Transmit(s_huart, (uint8_t*)data, size, UART_TIMEOUT_MS);
    }
}

void DWIN_Driver_HandleRxEvent(uint16_t size) {
    s_received_len = size;
    s_frame_received = true;
    if (HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

void DWIN_Driver_HandleError(UART_HandleTypeDef *huart) {
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(huart);
    }
    HAL_UART_AbortReceive_IT(huart);
    if (HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK) {
       Error_Handler();
    }
}