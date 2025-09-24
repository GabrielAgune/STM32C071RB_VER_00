#include "graos_handler.h"
#include "controller.h" // Para Controller_SetScreen, IDs de tela e teclas
#include "gerenciador_configuracoes.h" // Para lógica de grãos
#include "dwin_driver.h"  // Para DWIN_Driver_WriteString e VPs
#include <stdio.h>
#include <string.h>

//================================================================================
// Variáveis Estáticas (Movidas do controller.c)
//================================================================================
static int8_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;

//================================================================================
// Protótipo Estático Local (Movido do controller.c)
//================================================================================
static void Atualizar_Display_Grao_Selecionado(int8_t indice);

//================================================================================
// Implementação das Funções Públicas
//================================================================================

/**
 * @brief (Movido do controller.c)
 */
void Graos_Handle_Entrada_Tela(void)
{
    printf("Graos_Handler: Entrando na tela de selecao de graos.\r\n");
    s_em_tela_de_selecao = true;
    uint8_t indice_salvo = 0;
    Gerenciador_Config_Get_Grao_Ativo(&indice_salvo);
    s_indice_grao_selecionado = indice_salvo;
    if (s_indice_grao_selecionado >= Gerenciador_Config_Get_Num_Graos()) {
        s_indice_grao_selecionado = 0;
    }
    Atualizar_Display_Grao_Selecionado(s_indice_grao_selecionado);
		Controller_SetScreen(SELECT_GRAO);
    DWIN_Driver_SetScreen(SELECT_GRAO); 
}

/**
 * @brief (Movido do controller.c)
 */
void Graos_Handle_Navegacao(int16_t tecla)
{

		if (!s_em_tela_de_selecao) {
        return;
    }
		
    printf("\r\n>> Graos_Handler: Navegacao chamada.\r\n");
    printf("   Tecla recebida do DWIN: 0x%02X\r\n", tecla);
    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos();
    if (total_de_graos == 0) return;

    switch (tecla)
    {
        case DWIN_TECLA_SETA_DIR:
            s_indice_grao_selecionado++;
            if (s_indice_grao_selecionado >= total_de_graos) s_indice_grao_selecionado = 0;
            Atualizar_Display_Grao_Selecionado(s_indice_grao_selecionado);
            break;
        case DWIN_TECLA_SETA_ESQ:
            s_indice_grao_selecionado--;
            if (s_indice_grao_selecionado < 0) s_indice_grao_selecionado = total_de_graos - 1;
            Atualizar_Display_Grao_Selecionado(s_indice_grao_selecionado);
            break;
        case DWIN_TECLA_CONFIRMA:
            printf("Graos_Handler: Grao indice '%d' selecionado. Salvando...\r\n", s_indice_grao_selecionado);
            
            bool sucesso = Gerenciador_Config_Set_Grao_Ativo(s_indice_grao_selecionado);

            if(sucesso) printf("Graos_Handler: Salvo na RAM. Sera persistido em breve.\r\n");
            else printf("Graos_Handler: ERRO ao definir o grao ativo!\r\n");
            
            s_em_tela_de_selecao = false;
						Controller_SetScreen(PRINCIPAL);
            DWIN_Driver_SetScreen(PRINCIPAL); 
            break;
        case DWIN_TECLA_ESCAPE:
            printf("Graos_Handler: Selecao de grao cancelada.\r\n");
            s_em_tela_de_selecao = false;
						Controller_SetScreen(PRINCIPAL);
            DWIN_Driver_SetScreen(PRINCIPAL); 
            break;
        default:
            break;
    }
     printf("<< Fim da Funcao Graos_Handler: Navegacao.\r\n");
}

/**
 * @brief (Novo Getter)
 */
bool Graos_Esta_Em_Tela_Selecao(void)
{
    return s_em_tela_de_selecao;
}


//================================================================================
// Implementação da Lógica Local (Movida do controller.c)
//================================================================================

/**
 * @brief (Movido do controller.c)
 */
static void Atualizar_Display_Grao_Selecionado(int8_t indice)
{
    Config_Grao_t dados_grao;
    char buffer_display[25]; 
    printf("ATT_DISPLAY: Tentando ler o grao de indice %d...\r\n", indice);
    if (Gerenciador_Config_Get_Dados_Grao(indice, &dados_grao)) 
    {
        printf("ATT_DISPLAY: LIDO COM SUCESSO -> Grao: %s\r\n", dados_grao.nome);
        DWIN_Driver_WriteString(GRAO_A_MEDIR, dados_grao.nome, MAX_NOME_GRAO_LEN);
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_min);
        DWIN_Driver_WriteString(UMI_MIN, buffer_display, strlen(buffer_display));
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_max);
        DWIN_Driver_WriteString(UMI_MAX, buffer_display, strlen(buffer_display));
        snprintf(buffer_display, sizeof(buffer_display), "%u", (unsigned int)dados_grao.id_curva);
        DWIN_Driver_WriteString(CURVA, buffer_display, strlen(buffer_display));
        DWIN_Driver_WriteString(DATA_VAL, dados_grao.validade, MAX_VALIDADE_LEN);
        printf("ATT_DISPLAY: Todos os dados do indice %d foram ENFILEIRADOS.\r\n", indice);
    }
    else
    {
        printf("Controller: ERRO FATAL ao ler dados do grao no indice %d\r\n", indice);
    }
}