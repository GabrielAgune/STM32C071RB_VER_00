#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h> // Adicionar para usar uint8_t e uint16_t

/**
 * @brief Função a ser registrada como callback no DWIN Driver.
 * Ela recebe os dados brutos do display.
 * @param data Ponteiro para o buffer com os dados recebidos.
 * @param len Comprimento dos dados recebidos.
 */
void Controller_DwinCallback(const uint8_t* data, uint16_t len);

/**
 * @brief Processa a lógica principal do controlador.
 * Deve ser chamada repetidamente no loop principal do programa.
 */
void Process_Controller(void);

uint16_t Controller_GetCurrentScreen(void);
#endif /* CONTROLLER_H */