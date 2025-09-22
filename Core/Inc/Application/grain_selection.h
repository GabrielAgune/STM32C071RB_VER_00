#ifndef GRAIN_SELECTION_H
#define GRAIN_SELECTION_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @file grain_selection.h
 * @brief Grain Selection Management
 * @details Handles grain selection UI logic and state
 */

/**
 * @brief Initialize the grain selection system
 */
void Grain_Selection_Init(void);

/**
 * @brief Handle entry into grain selection screen
 */
void Grain_Selection_EnterScreen(void);

/**
 * @brief Handle grain selection key input
 * @param key_value Key value received from DWIN
 */
void Grain_Selection_HandleKey(int16_t key_value);

/**
 * @brief Check if currently in grain selection mode
 * @return true if in selection mode
 */
bool Grain_Selection_IsActive(void);

/**
 * @brief Get the currently selected grain index
 * @return Selected grain index
 */
int8_t Grain_Selection_GetSelectedIndex(void);

#endif // GRAIN_SELECTION_H