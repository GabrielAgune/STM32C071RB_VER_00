// ===== ARQUIVO REESCRITO: graos_handler.c =====

#include "graos_handler.h"

// Includes para controlar a UI e o estado
#include "controller.h"
#include "dwin_driver.h"

// Includes de dependência interna
#include "gerenciador_configuracoes.h"
#include <stdio.h>
#include <string.h>

//================================================================================
// Definições e Variáveis Internas
//================================================================================

// Enum para os resultados da lógica de navegação
typedef enum {
    NAV_RESULT_NO_CHANGE,       // Nenhuma ação de UI necessária
    NAV_RESULT_SELECTION_MOVED, // O índice mudou, precisa atualizar o display
    NAV_RESULT_CONFIRMED,       // Seleção confirmada, salvar e sair
    NAV_RESULT_CANCELLED        // Seleção cancelada, sair
} GraosNavResult_t;

static int8_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;

//================================================================================
// Protótipos das Funções de Lógica e UI (Estáticas)
//================================================================================

static void graos_handle_entrada_logic(void);
static GraosNavResult_t graos_handle_navegacao_logic(int16_t tecla);
static void atualizar_display_grao_selecionado(int8_t indice);

//================================================================================
// Funções Públicas (Processadores de Evento)
//================================================================================

void Graos_Handle_Entrada_Tela(void)
{
    // 1. Executa a lógica de negócio
    graos_handle_entrada_logic();

    // 2. Executa as ações de UI
    atualizar_display_grao_selecionado(s_indice_grao_selecionado);
    DWIN_Driver_SetScreen(SELECT_GRAO); // Comanda o display
}

void Graos_Handle_Navegacao(int16_t tecla)
{
    // 1. Executa a lógica de negócio e obtém o resultado
    GraosNavResult_t result = graos_handle_navegacao_logic(tecla);

    // 2. Traduz o resultado em uma ação de UI
    switch (result)
    {
        case NAV_RESULT_SELECTION_MOVED:
            atualizar_display_grao_selecionado(s_indice_grao_selecionado);
            break;

        case NAV_RESULT_CONFIRMED:
            printf("Graos_Handler: Grao indice '%d' selecionado. Salvando...\r\n", s_indice_grao_selecionado);
            Gerenciador_Config_Set_Grao_Ativo(s_indice_grao_selecionado);
            
            Controller_SetScreen(PRINCIPAL);
            DWIN_Driver_SetScreen(PRINCIPAL);
            break;

        case NAV_RESULT_CANCELLED:
            printf("Graos_Handler: Selecao de grao cancelada.\r\n");
            Controller_SetScreen(PRINCIPAL);
            DWIN_Driver_SetScreen(PRINCIPAL);
            break;
        
        case NAV_RESULT_NO_CHANGE:
        default:
            // Nenhuma ação de UI necessária
            break;
    }
}

bool Graos_Esta_Em_Tela_Selecao(void)
{
    return s_em_tela_de_selecao;
}

//================================================================================
// Implementação da Lógica Pura e Ações de UI (Funções Estáticas)
//================================================================================

static void graos_handle_entrada_logic(void)
{
    printf("Graos_Handler: Entrando na logica de selecao de graos.\r\n");
    s_em_tela_de_selecao = true;
    uint8_t indice_salvo = 0;
    Gerenciador_Config_Get_Grao_Ativo(&indice_salvo);
    s_indice_grao_selecionado = indice_salvo;

    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos();
    if (s_indice_grao_selecionado >= total_de_graos) {
        s_indice_grao_selecionado = 0;
    }
}

static GraosNavResult_t graos_handle_navegacao_logic(int16_t tecla)
{
    if (!s_em_tela_de_selecao) {
        return NAV_RESULT_NO_CHANGE;
    }

    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos();
    if (total_de_graos == 0) return NAV_RESULT_NO_CHANGE;

    switch (tecla)
    {
        case DWIN_TECLA_SETA_DIR:
            s_indice_grao_selecionado++;
            if (s_indice_grao_selecionado >= total_de_graos) s_indice_grao_selecionado = 0;
            return NAV_RESULT_SELECTION_MOVED;

        case DWIN_TECLA_SETA_ESQ:
            s_indice_grao_selecionado--;
            if (s_indice_grao_selecionado < 0) s_indice_grao_selecionado = total_de_graos - 1;
            return NAV_RESULT_SELECTION_MOVED;

        case DWIN_TECLA_CONFIRMA:
            s_em_tela_de_selecao = false; // Lógica de mudança de estado
            return NAV_RESULT_CONFIRMED;

        case DWIN_TECLA_ESCAPE:
            s_em_tela_de_selecao = false; // Lógica de mudança de estado
            return NAV_RESULT_CANCELLED;

        default:
            return NAV_RESULT_NO_CHANGE;
    }
}

static void atualizar_display_grao_selecionado(int8_t indice)
{
    Config_Grao_t dados_grao;
    char buffer_display[25]; 
    if (Gerenciador_Config_Get_Dados_Grao(indice, &dados_grao)) 
    {
        DWIN_Driver_WriteString(GRAO_A_MEDIR, dados_grao.nome, MAX_NOME_GRAO_LEN);
        
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_min);
        DWIN_Driver_WriteString(UMI_MIN, buffer_display, strlen(buffer_display));
        
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_max);
        DWIN_Driver_WriteString(UMI_MAX, buffer_display, strlen(buffer_display));
        
        snprintf(buffer_display, sizeof(buffer_display), "%u", (unsigned int)dados_grao.id_curva);
        DWIN_Driver_WriteString(CURVA, buffer_display, strlen(buffer_display));
        
        DWIN_Driver_WriteString(DATA_VAL, dados_grao.validade, MAX_VALIDADE_LEN);
    }
    else
    {
        printf("Graos_Handler: ERRO FATAL ao ler dados do grao no indice %d\r\n", indice);
    }
}