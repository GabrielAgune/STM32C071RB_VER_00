/*******************************************************************************
 * @file        screen_navigation.c
 * @brief       Screen Navigation Management
 * @version     8.3 (Extracted from controller.c during refactoring)
 * @details     Handles screen transitions and state tracking
 ******************************************************************************/

#include "screen_navigation.h"
#include "dwin_driver.h"
#include <stdio.h>

//================================================================================
// Static Variables
//================================================================================
static uint16_t s_current_screen_id = PRINCIPAL;

//================================================================================
// Public Functions
//================================================================================

void Screen_Navigation_Init(void)
{
    s_current_screen_id = PRINCIPAL;
}

uint16_t Screen_Navigation_GetCurrentScreen(void)
{
    return s_current_screen_id;
}

void Screen_Navigation_SetScreen(uint16_t screen_id)
{
    s_current_screen_id = screen_id;
    DWIN_Driver_SetScreen(screen_id);
}

void Screen_Navigation_EnterMonitor(void)
{
    Screen_Navigation_SetScreen(TELA_MONITOR_SYSTEM);
    printf("Screen Navigation: Entering Monitor System Screen.\r\n");
}

bool Screen_Navigation_HandleEscape(void)
{
    if (s_current_screen_id == TELA_MONITOR_SYSTEM) {
        Screen_Navigation_SetScreen(TELA_SERVICO);
        printf("Screen Navigation: Exiting Monitor -> Service Screen.\r\n");
        return true;
    }
    
    // Add other escape handlers as needed
    return false; // Escape not handled
}

void Screen_Navigation_ToggleOnOff(void)
{
    // This could be expanded to handle more sophisticated ON/OFF logic
    // For now, it's handled directly by the controller
    printf("Screen Navigation: ON/OFF toggle requested.\r\n");
}