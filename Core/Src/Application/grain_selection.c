/*******************************************************************************
 * @file        grain_selection.c
 * @brief       Grain Selection Management
 * @version     8.3 (Extracted from controller.c during refactoring)
 * @details     Handles grain selection UI logic and state
 ******************************************************************************/

#include "grain_selection.h"
#include "screen_navigation.h"
#include "dwin_driver.h"
#include "gerenciador_configuracoes.h"
#include <stdio.h>
#include <string.h>

//================================================================================
// Definitions
//================================================================================
#define DWIN_TECLA_SETA_ESQ    0x03 
#define DWIN_TECLA_SETA_DIR    0x02
#define DWIN_TECLA_CONFIRMA    0x01 
#define DWIN_TECLA_ESCAPE      0x06
#define MAX_NOME_GRAO_LEN      15

//================================================================================
// Static Variables
//================================================================================
static int8_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;

//================================================================================
// Private Function Prototypes  
//================================================================================
static void Atualizar_Display_Grao_Selecionado(int8_t indice);

//================================================================================
// Public Functions
//================================================================================

void Grain_Selection_Init(void)
{
    s_indice_grao_selecionado = 0;
    s_em_tela_de_selecao = false;
}

void Grain_Selection_EnterScreen(void)
{
    printf("Grain Selection: Entering grain selection screen.\r\n");
    s_em_tela_de_selecao = true;
    
    uint8_t indice_salvo = 0;
    Gerenciador_Config_Get_Grao_Ativo(&indice_salvo);
    s_indice_grao_selecionado = indice_salvo;
    
    if (s_indice_grao_selecionado >= Gerenciador_Config_Get_Num_Graos()) {
        s_indice_grao_selecionado = 0;
    }
    
    Atualizar_Display_Grao_Selecionado(s_indice_grao_selecionado);
    Screen_Navigation_SetScreen(SELECT_GRAO); 
}

void Grain_Selection_HandleKey(int16_t key_value)
{
    printf("\r\n>> Grain Selection: Handle key function called.\r\n");
    printf("   Key received from DWIN: 0x%02X\r\n", key_value);
    
    uint8_t total_de_graos = Gerenciador_Config_Get_Num_Graos();
    if (total_de_graos == 0) return;

    switch (key_value) {
        case DWIN_TECLA_SETA_DIR:
            s_indice_grao_selecionado++;
            if (s_indice_grao_selecionado >= total_de_graos) {
                s_indice_grao_selecionado = 0;
            }
            Atualizar_Display_Grao_Selecionado(s_indice_grao_selecionado);
            break;
            
        case DWIN_TECLA_SETA_ESQ:
            s_indice_grao_selecionado--;
            if (s_indice_grao_selecionado < 0) {
                s_indice_grao_selecionado = total_de_graos - 1;
            }
            Atualizar_Display_Grao_Selecionado(s_indice_grao_selecionado);
            break;
            
        case DWIN_TECLA_CONFIRMA:
            printf("Grain Selection: Grain index '%d' selected. Saving...\r\n", s_indice_grao_selecionado);
            
            bool sucesso = Gerenciador_Config_Set_Grao_Ativo(s_indice_grao_selecionado);

            if(sucesso) {
                printf("Grain Selection: Saved to RAM. Will be persisted soon.\r\n");
            } else {
                printf("Grain Selection: ERROR setting active grain!\r\n");
            }
            
            s_em_tela_de_selecao = false;
            Screen_Navigation_SetScreen(PRINCIPAL); 
            break;
            
        case DWIN_TECLA_ESCAPE:
            printf("Grain Selection: Grain selection canceled.\r\n");
            s_em_tela_de_selecao = false;
            Screen_Navigation_SetScreen(PRINCIPAL); 
            break;
            
        default:
            break;
    }
    
    printf("<< End of Grain Selection Handle Key function.\r\n");
}

bool Grain_Selection_IsActive(void)
{
    return s_em_tela_de_selecao;
}

int8_t Grain_Selection_GetSelectedIndex(void)
{
    return s_indice_grao_selecionado;
}

//================================================================================
// Private Functions
//================================================================================

static void Atualizar_Display_Grao_Selecionado(int8_t indice)
{
    Config_Grao_t dados_grao;
    char buffer_display[25]; 
    
    printf("UPDATE_DISPLAY: Trying to read grain at index %d...\r\n", indice);
    
    if (Gerenciador_Config_Get_Dados_Grao(indice, &dados_grao)) {
        printf("UPDATE_DISPLAY: READ SUCCESSFULLY -> Grain: %s\r\n", dados_grao.nome);
        
        DWIN_Driver_WriteString(GRAO_A_MEDIR, dados_grao.nome, MAX_NOME_GRAO_LEN);
        
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_min);
        DWIN_Driver_WriteString(UMI_MIN, buffer_display, strlen(buffer_display));
        
        snprintf(buffer_display, sizeof(buffer_display), "%.1f%%", (float)dados_grao.umidade_max);
        DWIN_Driver_WriteString(UMI_MAX, buffer_display, strlen(buffer_display));
    } else {
        printf("UPDATE_DISPLAY: ERROR reading grain data for index %d\r\n", indice);
    }
}