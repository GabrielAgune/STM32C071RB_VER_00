#ifndef MEDICAO_HANDLER_H
#define MEDICAO_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// Estrutura para armazenar os resultados de uma única medição
typedef struct {
    float Frequencia;
    float Escala_A;
    float Peso;
    float Temp_Sample;
    float Temp_Instru;
    float Densidade;
    float Umidade;
} DadosMedicao_t;


// --- FUNÇÕES "SET" ---
// Usadas durante o processo de medição para atualizar os valores.

void Medicao_Init(void); // Zera os dados para uma nova medição
void Medicao_Set_Frequencia(float valor);
void Medicao_Set_Escala_A(float valor);
void Medicao_Set_Peso(float valor);
void Medicao_Set_Temp_Sample(float valor);
void Medicao_Set_Temp_Instru(float valor);
void Medicao_Set_Densidade(float valor);
void Medicao_Set_Umidade(float valor);


// --- FUNÇÃO "GET" ---
// Usada por outros módulos (como o relatorio_handler) para ler o resultado final.

/**
 * @brief Obtém uma cópia completa dos dados da última medição.
 * @param dados_out Ponteiro para uma struct que será preenchida com os dados.
 */
void Medicao_Get_UltimaMedicao(DadosMedicao_t* dados_out);


#endif // MEDICAO_HANDLER_H