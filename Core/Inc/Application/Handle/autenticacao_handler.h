#ifndef AUTENTICACAO_HANDLER_H
#define AUTENTICACAO_HANDLER_H

#include <stdint.h>
#include "dwin_parser.h"
#include "gerenciador_configuracoes.h"
#include "dwin_driver.h"
#include <stdio.h>
#include <string.h>

/**
 * @brief Trata a tentativa de login (entrada de senha).
 * @param dwin_data Ponteiro para o buffer de dados brutos DWIN.
 * @param len Comprimento do buffer de dados.
 */
void Auth_Handle_Login(const uint8_t* dwin_data, uint16_t len);

/**
 * @brief Trata a definição de uma nova senha (lógica de 2 etapas).
 * @param dwin_data Ponteiro para o buffer de dados brutos DWIN.
 * @param len Comprimento do buffer de dados.
 */
void Auth_Handle_Set_Password(const uint8_t* dwin_data, uint16_t len);

#endif // AUTENTICACAO_HANDLER_H