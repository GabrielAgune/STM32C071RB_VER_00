// Core/Inc/Drivers/rtc_driver.h

#ifndef RTC_DRIVER_H
#define RTC_DRIVER_H

#include "rtc.h" // Inclui o handle do HAL

/**
 * @brief Inicializa o driver do RTC com a data e hora padrão.
 */
void RTC_Driver_Init(RTC_HandleTypeDef* hrtc);

/**
 * @brief Tarefa de processo periódico do RTC (chamada no super-loop).
 * Lê a hora atual e a enfileira para envio ao display DWIN (de forma cooperativa).
 */
void RTC_Driver_Process(void);

/**
 * @brief Define a hora do RTC (chamado pelo Controller após evento DWIN).
 */
void RTC_Driver_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

/**
 * @brief Manipula evento DWIN para ajuste de hora. (Esta função foi movida para controller.c)
 */
 // void Set_Just_Time(const uint8_t* rx_buffer, uint16_t rx_len); // Movido para controller.c


#endif // RTC_DRIVER_H