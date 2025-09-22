/*******************************************************************************
 * @file        controller.c
 * @brief       Application Controller Coordinator (Refactored V8.3)
 * @version     8.3 (Refactored for better modularity)
 * @details     Thin coordinator layer that orchestrates screen navigation,
 *              password handling, grain selection, and other UI logic.
 ******************************************************************************/

#include "controller.h"
#include "screen_navigation.h"
#include "password_handler.h"
#include "grain_selection.h"
#include "dwin_driver.h"
#include "rtc_driver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

//================================================================================
// Definitions
//================================================================================
#define MAX_SENHA_LEN 10
#define MAX_VALIDADE_LEN 10

//================================================================================
// Static Variables
//================================================================================
static int16_t received_value = 0;

//================================================================================
// Private Function Prototypes
//================================================================================
static void HandleOnOffToggle(void);
static void HandleTimeParser(const uint8_t* rx_buffer, uint16_t rx_len);
static bool Parse_Dwin_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len);

//================================================================================
// Public Functions
//================================================================================

uint16_t Controller_GetCurrentScreen(void)
{
    return Screen_Navigation_GetCurrentScreen();
}

void Process_Controller(void)
{
    // This function can be used for periodic controller tasks
    // Currently, most processing is event-driven through the DWIN callback
}

void Controller_DwinCallback(const uint8_t* data, uint16_t len)
{
    if (len < 6 || data[0] != 0x5A || data[1] != 0xA5) {
        return; 
    }

    if (data[3] == 0x83) { // VP Return Command (0x83 means "VP written by display")
        uint16_t vp_address = (data[4] << 8) | data[5];
        
        if (len >= 8) {
            // Logic for capturing 'received_value' (for keys, etc.)
            if (vp_address != SENHA_CONFIG && vp_address != SENHA && vp_address != SET_TIME) {
                uint8_t payload_len = data[2]; 
                if (len >= (3 + payload_len)) {
                   received_value = (data[3 + payload_len - 2] << 8) | data[3 + payload_len - 1];
                }
            } else {
                received_value = 0; 
            }
        }
        
        // VP Command Dispatcher
        switch (vp_address) {
            case OFF:               
                HandleOnOffToggle(); 
                break;
                
            case SENHA_CONFIG:      
                Password_Handler_ProcessConfig(data, len); 
                break;
                
            case SELECT_GRAIN:      
                Grain_Selection_EnterScreen(); 
                break;
                
            case TECLAS:            
                if (Grain_Selection_IsActive()) {
                    Grain_Selection_HandleKey(received_value);
                }
                break;
                
            case SENHA:
                Password_Handler_ProcessLogin(data, len);
                break;
                
            case DESCARTA_AMOSTRA:  
                printf("Discard Sample Button Pressed\r\n"); 
                break;
                
            case PRINT:             
                printf("Print Button Pressed\r\n"); 
                break;
                
            case SET_TIME:          
                HandleTimeParser(data, len);
                break;
            
            case MONITOR: // VP 0x7090 (User pressed MONITOR button)
                Screen_Navigation_EnterMonitor();
                break;
            
            case ESCAPE: // VP 0x5000 (Probably "Back" button from Monitor/Service)
                Screen_Navigation_HandleEscape();
                break;
            
            default:
                break;
        }
    }
}

//================================================================================
// Private Functions
//================================================================================

static void HandleOnOffToggle(void)
{
    if (received_value == 0x0010) {
        DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_10, sizeof(CMD_AJUSTAR_BACKLIGHT_10));
        printf("Turn off backlight\r\n");
    } else {
        DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_100, sizeof(CMD_AJUSTAR_BACKLIGHT_100));
        printf("Turn on backlight\r\n");
    }
}

static void HandleTimeParser(const uint8_t* rx_buffer, uint16_t rx_len)
{
    char time_str_safe[16]; 
    int hours, minutes, seconds;

    if (rx_len > 7) { 
        const uint8_t* payload = &rx_buffer[8];
        uint16_t payload_len = rx_len - 8;

        if (!Parse_Dwin_String_Payload_Robust(payload, payload_len, time_str_safe, sizeof(time_str_safe))) {
            printf("RTC Driver: Failed to extract time string (robust parser).\r\n");
            return;
        }

        if (sscanf(time_str_safe, "%d:%d:%d", &hours, &minutes, &seconds) == 3) {
            RTC_Driver_SetTime(hours, minutes, seconds); 
            printf("RTC updated successfully to: %s\r\n", time_str_safe);
        } else {
             printf("RTC Driver: Failed to convert DWIN string '%s'.\r\n", time_str_safe);
        }
    }
}

static bool Parse_Dwin_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len)
{
    if (payload == NULL || out_buffer == NULL || payload_len <= 1 || max_len == 0) {
        return false;
    }
    
    memset(out_buffer, 0, max_len);
    
    for (uint16_t i = 0; i < payload_len && i < (max_len - 1); i++) {
        uint8_t c = payload[i];
        if (c == 0x00 || c == 0xFF) {
            continue; 
        }
        out_buffer[i] = c;
    }
    out_buffer[max_len - 1] = '\0'; 
    return true;
}