/**
  ******************************************************************************
  * @file           : rtc_driver.c
  * @brief          : Módulo para gerenciamento do RTC. (V8.2 - Cooperativo + Otimizado)
  ******************************************************************************
  */

#include "rtc_driver.h"
#include "dwin_driver.h" 
#include "controller.h"   // <<< (V8.3) ADICIONADO para checar tela ativa
#include <stdio.h>       
#include <string.h>      

static RTC_HandleTypeDef* s_hrtc = NULL;
static char s_time_buffer[9]; // "HH:MM:SS"
static char s_date_buffer[9]; // "DD/MM/YY"
static uint32_t s_last_update_tick = 0;

/**
 * @brief Inicializa o driver do RTC.
 */
void RTC_Driver_Init(RTC_HandleTypeDef* hrtc)
{
    s_hrtc = hrtc;

    // Verifica se a hora já está configurada
    RTC_TimeTypeDef sTimeCheck = {0};
    RTC_DateTypeDef sDateCheck = {0};
    HAL_RTC_GetTime(s_hrtc, &sTimeCheck, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(s_hrtc, &sDateCheck, RTC_FORMAT_BIN);

    if (sDateCheck.Year != 25) // (O ano padrão que definimos)
    {
        RTC_TimeTypeDef sTime = {0};
        RTC_DateTypeDef sDate = {0};
        sTime.Hours = 0; sTime.Minutes = 0; sTime.Seconds = 0;
        HAL_RTC_SetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
        sDate.Date = 8; sDate.Month = RTC_MONTH_SEPTEMBER; sDate.Year = 25; 
        sDate.WeekDay = RTC_WEEKDAY_WEDNESDAY;
        HAL_RTC_SetDate(s_hrtc, &sDate, RTC_FORMAT_BIN);
    }
	printf("RTC Driver inicializado.\r\n");
}

/**
 * @brief (V8.3) Processa as tarefas periódicas do RTC (Atualização Condicional do Display).
 * Tarefa "educada" que só envia dados se a tela atual mostrar o relógio.
 */
void RTC_Driver_Process(void)
{
    uint32_t tick_atual = HAL_GetTick();

    if (tick_atual - s_last_update_tick < 1000) {
        return; // Não é hora
    }
    
    // Reseta o timer IMEDIATAMENTE (corrige o "pulo" de segundos)
    s_last_update_tick = tick_atual; 

    // **** (V8.3) LÓGICA DE ATUALIZAÇÃO CONDICIONAL (Sua Proposta) ****
    uint16_t tela_atual = Controller_GetCurrentScreen();
    
    // Só atualiza o relógio se estivermos na tela Principal OU nas telas de ajuste de hora.
    // (Adicione outras telas aqui se necessário)
    if (tela_atual != PRINCIPAL && 
        tela_atual != TELA_SET_JUST_TIME && 
        tela_atual != TELA_ADJUST_TIME)
    {
        return; // Não estamos numa tela que mostra o relógio. Economiza barramento UART.
    }
    // **** FIM DA LÓGICA V8.3 ****


    // Checagem de Contenção (corrige o "lag")
    if (DWIN_Driver_IsTxBusy())
    {
        return; // Pula este ciclo de envio. Tenta novamente no próximo segundo.
    }

    // Barramento livre E hora de atualizar. Envia os dados.
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(s_hrtc, &sDate, RTC_FORMAT_BIN);

    sprintf(s_time_buffer, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
    sprintf(s_date_buffer, "%02d/%02d/%02d", sDate.Date, sDate.Month, sDate.Year);

    DWIN_Driver_WriteString(HORA_SISTEMA, s_time_buffer, 8);
    DWIN_Driver_WriteString(DATA_SISTEMA, s_date_buffer, 8);
}

/**
 * @brief (V8.1) Define a hora do RTC (Hardware). Chamado pelo Controller.
 */
void RTC_Driver_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    RTC_TimeTypeDef new_time = {0};
    new_time.Hours   = hours;
    new_time.Minutes = minutes;
    new_time.Seconds = seconds;

    if (HAL_RTC_SetTime(s_hrtc, &new_time, RTC_FORMAT_BIN) == HAL_OK)
    {
        // Força a atualização imediata no display no próximo ciclo de Process()
        s_last_update_tick = 0; 
    }
}