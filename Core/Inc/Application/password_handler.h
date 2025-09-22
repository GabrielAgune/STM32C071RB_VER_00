#ifndef PASSWORD_HANDLER_H
#define PASSWORD_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file password_handler.h
 * @brief Password Input and Validation
 * @details Handles password entry state machine and validation
 */

/**
 * @brief Initialize the password handler
 */
void Password_Handler_Init(void);

/**
 * @brief Handle password configuration input
 * @param dwin_data Raw DWIN data
 * @param len Data length
 */
void Password_Handler_ProcessConfig(const uint8_t* dwin_data, uint16_t len);

/**
 * @brief Handle regular password input
 * @param dwin_data Raw DWIN data
 * @param len Data length
 */
void Password_Handler_ProcessLogin(const uint8_t* dwin_data, uint16_t len);

/**
 * @brief Get current password handler state
 * @return true if waiting for confirmation
 */
bool Password_Handler_IsAwaitingConfirmation(void);

#endif // PASSWORD_HANDLER_H