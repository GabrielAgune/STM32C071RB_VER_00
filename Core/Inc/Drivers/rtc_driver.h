// Em Core/Inc/rtc_driver.h

#ifndef RTC_DRIVER_H
#define RTC_DRIVER_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>

void RTC_Driver_Init(RTC_HandleTypeDef* hrtc);
void RTC_Driver_Process(void);

/**
 * @brief Manipula um evento DWIN para ajuste de hora.
 * A fun��o recebe o pacote de dados brutos e � respons�vel por interpret�-lo.
 * @param rx_buffer O buffer de dados brutos recebido do DWIN.
 * @param rx_len O comprimento do buffer de dados brutos.
 */
void Set_Just_Time(const uint8_t* rx_buffer, uint16_t rx_len);

#endif // RTC_DRIVER_H