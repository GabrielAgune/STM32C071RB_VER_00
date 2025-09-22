#ifndef RTC_HANDLER_H
#define RTC_HANDLER_H

#include <stdint.h>

/**
 * @brief Trata o recebimento de uma string de tempo do DWIN para ajustar o RTC.
 * @param dwin_data Ponteiro para o buffer de dados brutos DWIN.
 * @param len Comprimento do buffer de dados.
 */
void RTC_Handle_Set_Time(const uint8_t* dwin_data, uint16_t len);

#endif // RTC_HANDLER_H