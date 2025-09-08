#ifndef DWIN_DRIVER_H
#define DWIN_DRIVER_H

#include "main.h"
#include <stdbool.h>

typedef void (*dwin_rx_callback_t)(const uint8_t* buffer, uint16_t len);

void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback);
void DWIN_Driver_Process(void);
void DWIN_Driver_SetScreen(uint16_t screen_id);
void DWIN_Driver_WriteInt(uint16_t vp_address, int16_t value);
void DWIN_Driver_WriteInt32(uint16_t vp_address, int32_t value); // <-- NOVA FUNÇÃO
void DWIN_Driver_WriteString(uint16_t vp_address, const char* text);
void DWIN_Driver_WriteRawBytes(const uint8_t* data, uint16_t size);

void DWIN_Driver_HandleRxEvent(uint16_t size);
void DWIN_Driver_HandleError(UART_HandleTypeDef *huart);

#endif // DWIN_DRIVER_H