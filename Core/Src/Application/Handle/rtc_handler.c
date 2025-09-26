
#include "rtc_handler.h"

// Includes para controlar a UI e o driver de hardware
#include "dwin_driver.h"
#include "rtc_driver.h"

// Includes de dependência interna
#include "dwin_parser.h"
#include <stdio.h>
#include <string.h>

//================================================================================
// Definições Internas
//================================================================================

// Enum para os resultados da lógica de ajuste do RTC
typedef enum {
    RTC_SET_OK,
    RTC_SET_FAIL_PARSE,
    RTC_SET_FAIL_HW
} RtcSetResult_t;

// Struct para passar os dados parseados entre as funções lógicas e de UI
typedef struct {
    uint8_t day;
    uint8_t month;
    uint8_t year;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} RtcData_t;


//================================================================================
// Protótipos das Funções de Lógica Pura (Estáticas)
//================================================================================

static RtcSetResult_t rtc_handle_set_date_and_time_logic(const uint8_t* dwin_data, uint16_t len, RtcData_t* out_data);
static RtcSetResult_t rtc_handle_set_time_logic(const uint8_t* dwin_data, uint16_t len);
static void rtc_update_display(const RtcData_t* data);

//================================================================================
// Funções Públicas (Processadores de Evento)
//================================================================================

void RTC_Handle_Set_Time(const uint8_t* dwin_data, uint16_t len)
{
    // A lógica para esta função é um subconjunto da função completa abaixo,
    // então por simplicidade vamos manter a mais completa.
    // Se precisar de ambas, o padrão seria o mesmo.
    RTC_Handle_Set_Date_And_Time(dwin_data, len);
}

void RTC_Handle_Set_Date_And_Time(const uint8_t* dwin_data, uint16_t len)
{
    RtcData_t parsed_data;
    
    // 1. Executa a lógica de negócio
    RtcSetResult_t result = rtc_handle_set_date_and_time_logic(dwin_data, len, &parsed_data);

    // 2. Age sobre o resultado
    if (result == RTC_SET_OK)
    {
        // 2a. A lógica foi bem-sucedida, então atualizamos a UI para refletir os novos dados.
        printf("RTC Handler: RTC atualizado com sucesso. Atualizando display.\r\n");
        rtc_update_display(&parsed_data);
    }
    else
    {
        // 2b. A lógica falhou, apenas registramos no log. Nenhuma ação de UI.
        printf("RTC Handler: Falha ao atualizar RTC. Nenhum feedback para o usuario.\r\n");
    }
}


//================================================================================
// Implementação da Lógica Pura e Ações de UI (Funções Estáticas)
//================================================================================

static RtcSetResult_t rtc_handle_set_date_and_time_logic(const uint8_t* dwin_data, uint16_t len, RtcData_t* out_data)
{
    char parsed_string[32] = {0};
    
    // 1. Extrair a string do payload DWIN
    const uint8_t* payload = &dwin_data[8];
    uint16_t payload_len = len - 8;
    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, parsed_string, sizeof(parsed_string))) {
        printf("RTC Logic: Falha ao extrair string.\r\n");
        return RTC_SET_FAIL_PARSE;
    }
    printf("RTC Logic: Recebido string '%s'\r\n", parsed_string);

    // 2. Tentar extrair data e hora da string
    uint8_t d=0, m=0, y=0, h=0, min=0, s=0;
    bool date_found = false;
    bool time_found = false;

    if (sscanf(parsed_string, "%hhu/%hhu/%hhu %hhu:%hhu:%hhu", &d, &m, &y, &h, &min, &s) == 6) {
        date_found = true; time_found = true;
    } else if (sscanf(parsed_string, "%hhu/%hhu/%hhu", &d, &m, &y) == 3) {
        date_found = true;
    } else if (sscanf(parsed_string, "%hhu:%hhu:%hhu", &h, &min, &s) == 3) {
        time_found = true;
    }

    if (!date_found && !time_found) {
        printf("RTC Logic: Formato de string irreconhecivel.\r\n");
        return RTC_SET_FAIL_PARSE;
    }

    // 3. Se a atualização for parcial, busca os dados atuais do hardware
    if (!time_found) { RTC_Driver_GetTime(&h, &min, &s); }
    if (!date_found) { RTC_Driver_GetDate(&d, &m, &y); }

    // 4. Aplica as novas configurações ao hardware
    if (date_found) {
        if (!RTC_Driver_SetDate(d, m, y)) return RTC_SET_FAIL_HW;
    }
    if (time_found) {
        if (!RTC_Driver_SetTime(h, min, s)) return RTC_SET_FAIL_HW;
    }
    
    // 5. Preenche a struct de saída para a camada de UI usar
    out_data->day = d; out_data->month = m; out_data->year = y;
    out_data->hour = h; out_data->minute = min; out_data->second = s;
    
    return RTC_SET_OK;
}

// (A função rtc_handle_set_time_logic pode ser omitida se a função completa acima for suficiente)

/**
 * @brief Função dedicada a enviar os dados de data/hora para o display
 */
static void rtc_update_display(const RtcData_t* data)
{
    char buffer_display[20];
    
    snprintf(buffer_display, sizeof(buffer_display), "%02d/%02d/%02d", data->day, data->month, data->year);
    DWIN_Driver_WriteString(DATA_SISTEMA, buffer_display, strlen(buffer_display));

    snprintf(buffer_display, sizeof(buffer_display), "%02d:%02d:%02d", data->hour, data->minute, data->second);
    DWIN_Driver_WriteString(HORA_SISTEMA, buffer_display, strlen(buffer_display));
}