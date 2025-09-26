#include "rtc_handler.h"
#include "dwin_parser.h" // Nosso novo parser
#include "rtc_driver.h"        
#include "dwin_driver.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief (V8.3) Trata a entrada de hora/data usando o parser robusto.
 * Lógica movida de Set_Just_Time_Parser do controller.c
 */
void RTC_Handle_Set_Time(const uint8_t* dwin_data, uint16_t len)
{
    char parsed_string[16] = {0};
    const uint8_t* payload = &dwin_data[8];
    uint16_t payload_len = len - 8;

    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, parsed_string, sizeof(parsed_string))) {
        printf("RTC Handler (Time): Falha ao extrair string.\r\n");
        return;
    }

    uint8_t h, min, s;
    if (sscanf(parsed_string, "%hhu:%hhu:%hhu", &h, &min, &s) == 3) {
        RTC_Driver_SetTime(h, min, s);
        printf("RTC Handler (Time): Hora ajustada para %02d:%02d:%02d\r\n", h, min, s);
    } else {
        printf("RTC Handler (Time): Formato de string de hora invalido: '%s'.\r\n", parsed_string);
    }
}

/**
 * @brief Trata a entrada de data e/ou hora a partir de uma string do DWIN.
 * A função detecta se a string contém data, hora, ou ambos.
 */
void RTC_Handle_Set_Date_And_Time(const uint8_t* dwin_data, uint16_t len)
{
    char parsed_string[32] = {0};
    
    // 1. Extrair a string do payload DWIN
    const uint8_t* payload = &dwin_data[6];
    uint16_t payload_len = len - 6;
    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, parsed_string, sizeof(parsed_string))) {
        printf("RTC Handler: Falha ao extrair string.\r\n");
        return;
    }
    printf("RTC Handler: Recebido string '%s'\r\n", parsed_string);

    // 2. Tentar extrair data e hora da string
    uint8_t d, m, y, h, min, s;
    bool date_found = false;
    bool time_found = false;

    // Tenta formato completo: "DD/MM/YY HH:MM:SS"
    if (sscanf(parsed_string, "%hhu/%hhu/%hhu %hhu:%hhu:%hhu", &d, &m, &y, &h, &min, &s) == 6) {
        date_found = true;
        time_found = true;
    }
    // Senão, tenta apenas data: "DD/MM/YY"
    else if (sscanf(parsed_string, "%hhu/%hhu/%hhu", &d, &m, &y) == 3) {
        date_found = true;
    }
    // Senão, tenta apenas hora: "HH:MM:SS"
    else if (sscanf(parsed_string, "%hhu:%hhu:%hhu", &h, &min, &s) == 3) {
        time_found = true;
    }

    if (!date_found && !time_found) {
        printf("RTC Handler: Formato de string irreconhecivel.\r\n");
        return;
    }

    // 3. Se a atualização for parcial, busca os dados atuais para não sobrescrevê-los
    if (!time_found) {
        RTC_Driver_GetTime(&h, &min, &s); // Mantém a hora atual
    }
    if (!date_found) {
        RTC_Driver_GetDate(&d, &m, &y); // Mantém a data atual
    }

    // 4. Aplica as novas configurações ao hardware
    if (date_found) RTC_Driver_SetDate(d, m, y);
    if (time_found) RTC_Driver_SetTime(h, min, s);

    printf("RTC Handler: RTC atualizado.\r\n");
    
    // 5. Envia os valores de volta para os VPs de display na tela
    char buffer_display[20];
    snprintf(buffer_display, sizeof(buffer_display), "%02d/%02d/%02d", d, m, y);
    DWIN_Driver_WriteString(DATA_SISTEMA, buffer_display, strlen(buffer_display));

    snprintf(buffer_display, sizeof(buffer_display), "%02d:%02d:%02d", h, min, s);
    DWIN_Driver_WriteString(HORA_SISTEMA, buffer_display, strlen(buffer_display));
}