#ifndef GRAOS_HANDLER_H
#define GRAOS_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Trata a entrada na tela de seleção de grãos.
 * (Substitui Lidar_Com_Entrada_Tela_Graos)
 */
void Graos_Handle_Entrada_Tela(void);

/**
 * @brief Trata os eventos de teclas (navegação) na tela de seleção.
 * (Substitui Lidar_Com_Selecao_De_Grao)
 * @param tecla O código da tecla recebida do DWIN.
 */
void Graos_Handle_Navegacao(int16_t tecla);

/**
 * @brief Verifica se o handler de grãos está atualmente ativo 
 * (na tela de seleção).
 * (Substitui a verificação da variável s_em_tela_de_selecao)
 * @return true se a tela de seleção estiver ativa, false caso contrário.
 */
bool Graos_Esta_Em_Tela_Selecao(void);

#endif // GRAOS_HANDLER_H