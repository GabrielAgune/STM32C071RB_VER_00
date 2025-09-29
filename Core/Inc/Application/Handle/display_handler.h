// ===== ARQUIVO REESCRITO (Etapa 1): display_handler.h =====

#ifndef DISPLAY_HANDLER_H
#define DISPLAY_HANDLER_H

#include "dwin_driver.h"
#include "controller.h"
#include "gerenciador_configuracoes.h"
#include "dwin_parser.h"
#include "medicao_handler.h"
#include "relato.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>


void Display_Handle_ON_OFF(int16_t received_value);
void Display_ProcessPrintEvent(uint16_t received_value);
void Display_SetRepeticoes(uint16_t received_value);
void Display_SetDecimals(uint16_t received_value);
void Display_SetUser(const uint8_t* dwin_data, uint16_t len, uint16_t received_value);
void Display_SetCompany(const uint8_t* dwin_data, uint16_t len, uint16_t received_value);
void Display_Adj_Capa(uint16_t received_value);
void Display_ShowAbout(void);
void Display_ShowModel(void);

// Funções para controlar o estado da impressão
void Display_SetPrintingEnabled(bool is_enabled);
bool Display_IsPrintingEnabled(void);

#endif // DISPLAY_HANDLER_H