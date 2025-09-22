#ifndef SCREEN_NAVIGATION_H
#define SCREEN_NAVIGATION_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file screen_navigation.h
 * @brief Screen Navigation Management
 * @details Handles screen transitions and state tracking
 */

/**
 * @brief Initialize the screen navigation system
 */
void Screen_Navigation_Init(void);

/**
 * @brief Get the currently active screen ID
 * @return Current screen ID
 */
uint16_t Screen_Navigation_GetCurrentScreen(void);

/**
 * @brief Set the active screen and update internal state
 * @param screen_id Screen ID to activate
 */
void Screen_Navigation_SetScreen(uint16_t screen_id);

/**
 * @brief Handle navigation to monitor screen
 */
void Screen_Navigation_EnterMonitor(void);

/**
 * @brief Handle escape/back navigation
 * @return true if navigation was handled
 */
bool Screen_Navigation_HandleEscape(void);

/**
 * @brief Handle ON/OFF screen toggle
 */
void Screen_Navigation_ToggleOnOff(void);

#endif // SCREEN_NAVIGATION_H