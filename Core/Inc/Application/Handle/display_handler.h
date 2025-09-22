#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include "dwin_driver.h" 
#include <stdint.h>
#include <stdio.h>

/**
 * @brief Controla o backlight da tela (ON/OFF).
 * @param received_value O valor recebido do VP (ex: 0x0010 para OFF).
 */
void Display_Handle_ON_OFF(int16_t received_value);

#endif // DISPLAY_HANDLER_H