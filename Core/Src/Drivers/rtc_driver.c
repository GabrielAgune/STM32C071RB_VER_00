/*******************************************************************************
 * @file        rtc_driver.c
 * @brief       Driver de abstração para o periférico RTC (Real-Time Clock).
 * @details     Este módulo fornece uma interface simplificada para as funções
 * do RTC da HAL, além de utilitários para converter o formato
 * de data/hora do hardware para o padrão Unix Timestamp e vice-versa.
 ******************************************************************************/

#include "rtc_driver.h"
#include "rtc.h"
#include <time.h> 

extern RTC_HandleTypeDef hrtc;

//==============================================================================
// Implementação das Funções Públicas
//==============================================================================

// Inicializa o driver do RTC (função de verificação).
HAL_StatusTypeDef RTC_Driver_Init(void)
{
    RTC_TimeTypeDef sTime;
    // Apenas verifica se o RTC está acessível lendo a hora atual.
    return HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
}

// Obtém a data e a hora atuais do RTC.
HAL_StatusTypeDef RTC_Driver_GetDateTime(RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate)
{
    if (HAL_RTC_GetTime(&hrtc, sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_RTC_GetDate(&hrtc, sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_OK;
}

// Ajusta a data e a hora do RTC.
HAL_StatusTypeDef RTC_Driver_SetDateTime(RTC_TimeTypeDef *sTime, RTC_DateTypeDef *sDate)
{
    if (HAL_RTC_SetTime(&hrtc, sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (HAL_RTC_SetDate(&hrtc, sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_OK;
}

// Lê a data e a hora do RTC e converte para Unix Timestamp.
uint32_t RTC_Driver_GetUnixTimestamp(void)
{
    RTC_TimeTypeDef sTime;
    RTC_DateTypeDef sDate;
    RTC_Driver_GetDateTime(&sTime, &sDate);

    struct tm timeinfo = {0};

    // Converte do formato do RTC da ST para o formato da struct tm
    // tm_year é o número de anos desde 1900. O RTC da ST é desde 2000.
    timeinfo.tm_year = sDate.Year + 100;
    // tm_mon é de 0 a 11. O RTC da ST é de 1 a 12.
    timeinfo.tm_mon  = sDate.Month - 1;
    timeinfo.tm_mday = sDate.Date;
    timeinfo.tm_hour = sTime.Hours;
    timeinfo.tm_min  = sTime.Minutes;
    timeinfo.tm_sec  = sTime.Seconds;

    // mktime converte a estrutura 'tm' local em um timestamp.
    time_t timestamp = mktime(&timeinfo);

    return (uint32_t)timestamp;
}

// Recebe um Unix Timestamp e ajusta o relógio do RTC.
HAL_StatusTypeDef RTC_Driver_SetUnixTimestamp(uint32_t timestamp)
{
    time_t raw_time = timestamp;
    struct tm *timeinfo = gmtime(&raw_time);

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    sTime.Hours   = timeinfo->tm_hour;
    sTime.Minutes = timeinfo->tm_min;
    sTime.Seconds = timeinfo->tm_sec;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    sDate.Date    = timeinfo->tm_mday;
    // Conversão inversa de tm_mon (0-11) para o formato do RTC (1-12)
    sDate.Month   = timeinfo->tm_mon + 1;
    // Conversão inversa de tm_year (anos desde 1900) para o formato do RTC (anos desde 2000)
    sDate.Year    = timeinfo->tm_year - 100;
    // tm_wday é 0(Dom)-6. O RTC da ST é 1(Seg)-7. Ajuste para o domingo.
    sDate.WeekDay = (timeinfo->tm_wday == 0) ? 7 : timeinfo->tm_wday;

    return RTC_Driver_SetDateTime(&sTime, &sDate);
}