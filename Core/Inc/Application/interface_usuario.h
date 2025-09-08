// Core/Inc/Application/interface_usuario.h

#ifndef INTERFACE_USUARIO_H
#define INTERFACE_USUARIO_H

#include "main.h"
#include "app_eventos.h" // A UI ainda precisa saber o que é um ServoStep_t

/**
 * @brief Inicializa o módulo da interface do usuário de forma não-bloqueante.
 */
void UI_Init(void);

/**
 * @brief Processa a máquina de estados da UI, incluindo a sequência de splash screen.
 * Deve ser chamada repetidamente no loop principal.
 */
void UI_Process(void);

/**
 * @brief Atualiza os dados periódicos na tela (RTC e Temperatura).
 */
void UI_Update_Periodic(void);

// --- Funções de Comando (Controladas pelo App_Manager) ---

/**
 * @brief Ação a ser executada na UI após uma autenticação bem-sucedida.
 */
void UI_On_Auth_Success(void);

/**
 * @brief Ação a ser executada na UI após uma falha de autenticação.
 */
void UI_On_Auth_Failure(void);

/**
 * @brief Atualiza a tela da UI com base no passo atual do processo.
 * @param step O passo atual da sequência de servos.
 */
void UI_On_Process_Step_Changed(ServoStep_t step);

/**
 * @brief Ação a ser executada na UI quando o processo principal é finalizado.
 */
void UI_On_Process_Finished(void);

#endif // INTERFACE_USUARIO_H