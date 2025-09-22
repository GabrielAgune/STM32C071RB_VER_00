#include "rtc_handler.h"
#include "dwin_parser.h" // Nosso novo parser
#include "rtc_driver.h"        // Driver RTC existente
#include <stdio.h>
#include <string.h>

/**
 * @brief (V8.3) Trata a entrada de hora/data usando o parser robusto.
 * Lógica movida de Set_Just_Time_Parser do controller.c
 */
void RTC_Handle_Set_Time(const uint8_t* rx_buffer, uint16_t rx_len)
{
    char time_str_safe[16]; 
    int hours, minutes, seconds;
		
		
    if (rx_len > 7) { 
        const uint8_t* payload = &rx_buffer[8];
        uint16_t payload_len = rx_len - 8;
				

        if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, time_str_safe, sizeof(time_str_safe)))
        {
            printf("RTC Driver: Falha ao extrair string de tempo (parser robusto).\r\n");
            return;
        }

        if (sscanf(time_str_safe, "%d:%d:%d", &hours, &minutes, &seconds) == 3)
        {
            RTC_Driver_SetTime(hours, minutes, seconds); 
            printf("RTC atualizado com sucesso para: %s\r\n", time_str_safe);
        }
        else
        {
             printf("RTC Driver: Falha ao converter a string DWIN '%s'.\r\n", time_str_safe);
        }
    }
}