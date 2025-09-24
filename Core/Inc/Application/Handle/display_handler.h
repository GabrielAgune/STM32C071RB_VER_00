#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include "dwin_driver.h"
#include "gerenciador_configuracoes.h"
#include "dwin_parser.h"
#include "relato.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/**
 * @brief Controla o backlight da tela (ON/OFF).
 * @param received_value O valor recebido do VP (ex: 0x0010 para OFF).
 */
void Display_Handle_ON_OFF(int16_t received_value);

void Set_Repeticoes(uint16_t received_value);

void Set_Decimals(uint16_t received_value);

void Print(uint16_t received_value);

void Type_User(const uint8_t* dwin_data, uint16_t len, uint16_t received_value);
void Type_Company(const uint8_t* dwin_data, uint16_t len, uint16_t received_value);

void Handle_QR(uint16_t received_value);
void About(void);
#endif // DISPLAY_HANDLER_H