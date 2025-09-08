/**
  ******************************************************************************
  * @file           : rtc_driver.c
  * @brief          : Módulo para gerenciamento do Real-Time Clock (RTC).
  * Este módulo encapsula a inicialização, atualização periódica
  * no display e o tratamento de eventos DWIN para ajuste de hora.
  ******************************************************************************
  */

#include "rtc_driver.h"
#include "dwin_driver.h" // Necessário para enviar dados para o display
#include <stdio.h>       // Para sprintf e sscanf
#include <string.h>      // Para memcpy

// --- Variáveis Estáticas do Módulo (privadas) ---

// Ponteiro para o handle do RTC, guardado localmente após a inicialização
static RTC_HandleTypeDef* s_hrtc = NULL;

// Buffers para as strings de data e hora formatadas
static char s_time_buffer[9]; // Formato "HH:MM:SS" + '\0'
static char s_date_buffer[9]; // Formato "DD/MM/YY" + '\0'

// Variável para controlar o tempo de atualização no display (1 segundo)
static uint32_t s_last_update_tick = 0;


/**
 * @brief Inicializa o driver do RTC com a data e hora padrão.
 * @param hrtc Ponteiro para a estrutura do HAL RTC (gerada pelo CubeMX).
 */
void RTC_Driver_Init(RTC_HandleTypeDef* hrtc)
{
    s_hrtc = hrtc;

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};


    sTime.Hours = 0;
    sTime.Minutes = 0;
    sTime.Seconds = 0;
    HAL_RTC_SetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);


    sDate.Date = 8;
    sDate.Month = RTC_MONTH_SEPTEMBER;
    sDate.Year = 25;
    sDate.WeekDay = RTC_WEEKDAY_WEDNESDAY;
    HAL_RTC_SetDate(s_hrtc, &sDate, RTC_FORMAT_BIN);
	
	printf("RTC Driver inicializado.\r\n");
}

/**
 * @brief Processa as tarefas periódicas do RTC.
 * Deve ser chamada continuamente no loop principal (while(1)).
 * Esta função atualiza o display DWIN com a hora/data a cada segundo.
 */
void RTC_Driver_Process(void)
{
    if (HAL_GetTick() - s_last_update_tick >= 1000)
    {
        s_last_update_tick = HAL_GetTick();

        RTC_TimeTypeDef sTime = {0};
        RTC_DateTypeDef sDate = {0};

        HAL_RTC_GetTime(s_hrtc, &sTime, RTC_FORMAT_BIN);
        HAL_RTC_GetDate(s_hrtc, &sDate, RTC_FORMAT_BIN);

        // Formata as strings de hora e data
        sprintf(s_time_buffer, "%02d:%02d:%02d", sTime.Hours, sTime.Minutes, sTime.Seconds);
        sprintf(s_date_buffer, "%02d/%02d/%02d", sDate.Date, sDate.Month, sDate.Year);

        // Envia as strings para os VPs corretos no display DWIN
        DWIN_Driver_WriteString(HORA_SISTEMA, s_time_buffer, 8);
        DWIN_Driver_WriteString(DATA_SISTEMA, s_date_buffer, 8);
    }
}

/**
 * @brief Manipula um evento DWIN para ajuste de hora.
 * A função recebe o pacote de dados brutos e é responsável por interpretá-lo,
 * extraindo a string de hora e atualizando o hardware do RTC.
 * @param rx_buffer O buffer de dados brutos recebido do DWIN.
 * @param rx_len O comprimento do buffer de dados brutos.
 */
void Set_Just_Time(const uint8_t* rx_buffer, uint16_t rx_len)
{
    char time_str_safe[16];
    int hours, minutes, seconds;

    // 1. Lógica de PARSING: Este módulo sabe como interpretar o pacote DWIN
    if (rx_len > 9) {
        // O texto da hora começa no 10º byte (índice 9)
        size_t text_len = rx_len - 9;
        if (text_len > sizeof(time_str_safe) - 1) {
            text_len = sizeof(time_str_safe) - 1;
        }
        memcpy(time_str_safe, &rx_buffer[9], text_len);
        time_str_safe[text_len] = '\0';

        // 2. Lógica de NEGÓCIO: Converter a string e atualizar o RTC
        if (sscanf(time_str_safe, "%d:%d:%d", &hours, &minutes, &seconds) == 3)
        {
            RTC_TimeTypeDef new_time = {0};
            new_time.Hours   = hours;
            new_time.Minutes = minutes;
            new_time.Seconds = seconds;

            if (HAL_RTC_SetTime(s_hrtc, &new_time, RTC_FORMAT_BIN) == HAL_OK)
            {
                // Força a atualização imediata no display no próximo ciclo de Process()
                s_last_update_tick = 0; 
                printf("RTC atualizado com sucesso para: %s\r\n", time_str_safe);
            }
            else
            {
                printf("Falha ao configurar o RTC via HAL.\r\n");
            }
        }
        else
        {
             printf("RTC Driver: Falha ao converter a string DWIN '%s'.\r\n", time_str_safe);
        }
    }
}