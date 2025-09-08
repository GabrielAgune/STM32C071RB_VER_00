#ifndef RTC_DRIVER_H
#define RTC_DRIVER_H

#include "main.h"
#include <time.h>

/**
 * @brief Inicializa o driver do RTC.
 * @return HAL_StatusTypeDef Resultado da operação.
 */
HAL_StatusTypeDef RTC_Driver_Init(void);

/**
 * @brief Obtém a data e hora atuais do hardware do RTC.
 * @param sTime Ponteiro para a estrutura que armazenará a hora.
 * @param sDate Ponteiro para a estrutura que armazenará a data.
 * @return HAL_StatusTypeDef Resultado da operação.
 */
HAL_StatusTypeDef RTC_Driver_GetDateTime(RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate);

/**
 * @brief Define a data e hora no hardware do RTC.
 * @param sTime Ponteiro para a estrutura com a nova hora.
 * @param sDate Ponteiro para a estrutura com a nova data.
 * @return HAL_StatusTypeDef Resultado da operação.
 */
HAL_StatusTypeDef RTC_Driver_SetDateTime(RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate);

/**
 * @brief Obtém a data e hora atuais como um Unix Timestamp.
 * @return uint32_t O valor do timestamp.
 */
uint32_t RTC_Driver_GetUnixTimestamp(void);

/**
 * @brief Define a data e hora do RTC a partir de um Unix Timestamp.
 * @param timestamp O valor do timestamp a ser configurado.
 * @return HAL_StatusTypeDef Resultado da operação.
 */
HAL_StatusTypeDef RTC_Driver_SetUnixTimestamp(uint32_t timestamp);

#endif // RTC_DRIVER_H