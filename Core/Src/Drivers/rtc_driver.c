#include "rtc_driver.h"
#include "dwin_driver.h" 
#include <stdio.h>       
#include <string.h>      

static RTC_HandleTypeDef* s_hrtc = NULL;
static char s_time_buffer[9]; // "HH:MM:SS"
static char s_date_buffer[9]; // "DD/MM/YY"
static uint32_t s_last_update_tick = 0;

void RTC_Driver_Init(RTC_HandleTypeDef* hrtc)
{
    s_hrtc = hrtc;
    RTC_DateTypeDef sDateCheck = {0};
    HAL_RTC_GetDate(s_hrtc, &sDateCheck, RTC_FORMAT_BIN);

    if (sDateCheck.Year < 24) // Se o ano for inválido, define um padrão
    {
        RTC_TimeTypeDef sTime = { .Hours = 0, .Minutes = 0, .Seconds = 0 };
        RTC_DateTypeDef sDate = { .Date = 26, .Month = RTC_MONTH_SEPTEMBER, .Year = 25, .WeekDay = RTC_WEEKDAY_FRIDAY };
        HAL_RTC_SetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_SetDate(s_hrtc, &sDate, RTC_FORMAT_BIN);
    }
    printf("RTC Driver inicializado.\r\n");
}

// --- NOVAS FUNÇÕES E CORREÇÕES ---

bool RTC_Driver_SetDate(uint8_t day, uint8_t month, uint8_t year)
{
    RTC_DateTypeDef new_date = {0};
    new_date.Date    = day;
    new_date.Month   = month;
    new_date.Year    = year;

    if (HAL_RTC_SetDate(s_hrtc, &new_date, RTC_FORMAT_BIN) == HAL_OK) {
        s_last_update_tick = 0; // Força atualização no display
        return true;
    }
    return false;
}

bool RTC_Driver_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    RTC_TimeTypeDef new_time = {0};
    new_time.Hours   = hours;
    new_time.Minutes = minutes;
    new_time.Seconds = seconds;

    if (HAL_RTC_SetTime(s_hrtc, &new_time, RTC_FORMAT_BIN) == HAL_OK) {
        s_last_update_tick = 0; // Força atualização no display
        return true;
    }
    return false;
}

bool RTC_Driver_GetDate(uint8_t* day, uint8_t* month, uint8_t* year)
{
    if (!day || !month || !year) return false;

    RTC_DateTypeDef sDate = {0};
    if (HAL_RTC_GetDate(s_hrtc, &sDate, RTC_FORMAT_BIN) == HAL_OK) {
        *day   = sDate.Date;
        *month = sDate.Month;
        *year  = sDate.Year;
        return true;
    }
    return false;
}

bool RTC_Driver_GetTime(uint8_t* hours, uint8_t* minutes, uint8_t* seconds)
{
    if (!hours || !minutes || !seconds) return false;

    RTC_TimeTypeDef sTime = {0};
    if (HAL_RTC_GetTime(s_hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK) {
        *hours   = sTime.Hours;
        *minutes = sTime.Minutes;
        *seconds = sTime.Seconds;
        return true;
    }
    return false;
}