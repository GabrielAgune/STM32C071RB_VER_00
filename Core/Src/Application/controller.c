// Core/Src/Application/controller.c
// VERSÃO 8.5 - Refatorado para padrão Dispatcher
// Lógica de senha, RTC e display movida para módulos handler dedicados.

#include "controller.h"


//================================================================================
// INCLUSÃO DOS NOVOS HANDLERS
//================================================================================
#include "autenticacao_handler.h"
#include "rtc_handler.h"
#include "display_handler.h"

//================================================================================
// Definições, Enums, Variáveis Estáticas e Protótipos
//================================================================================

// (Variáveis de estado de senha movidas para autenticacao_handler.c)

// Variáveis de estado para a lógica LOCAL (Seleção de Grão)
static int8_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;
static int16_t received_value = 0;
static uint16_t s_current_screen_id = PRINCIPAL; // RASTREADOR DE TELA ATIVA

// Protótipos estáticos (Apenas lógica local permanece)
static void Atualizar_Display_Grao_Selecionado(int8_t indice);
static void Lidar_Com_Selecao_De_Grao(int16_t tecla);
static void Lidar_Com_Entrada_Tela_Graos(void);

// (Funções de parser e lógicas de senha/rtc/display foram removidas)

//================================================================================
// Funções Públicas (Getters/Setters)
//================================================================================

/**
 * @brief (V8.3) Retorna a tela que o controlador acredita estar ativa.
 */
uint16_t Controller_GetCurrentScreen(void)
{
    return s_current_screen_id;
}

/**
 * @brief (V8.3 -> V8.5) Wrapper PÚBLICO para DWIN_Driver_SetScreen que rastreia a tela atual.
 */
void Controller_SetScreen(uint16_t screen_id)
{
    s_current_screen_id = screen_id;
}


//================================================================================
// Função de Callback (O Despachante Principal)
//================================================================================
void Controller_DwinCallback(const uint8_t* data, uint16_t len)
{
    if (len < 6 || data[0] != 0x5A || data[1] != 0xA5) {
        return; 
    }

    if (data[3] == 0x83) { 
        uint16_t vp_address = (data[4] << 8) | data[5];
        
        if (len >= 8) {
            if (vp_address != SENHA_CONFIG && vp_address != SENHA && vp_address != SET_TIME)
            {
                 uint8_t payload_len = data[2]; 
                 if (len >= (3 + payload_len)) {
                    received_value = (data[3 + payload_len - 2] << 8) | data[3 + payload_len - 1];
                 }
            } else {
                 received_value = 0; 
            }
        }
        
        //================================================================
        // DESPACHANTE DE COMANDOS (Estilo G5_Controller)
        //================================================================
        switch (vp_address) {
            case OFF:               Display_Handle_ON_OFF(received_value);                                                 break;
            case SENHA_CONFIG:      Auth_Handle_Login(data, len);                                                          break;
            case SENHA:             Auth_Handle_Set_Password(data, len);                                                   break;
            case SET_TIME:          RTC_Handle_Set_Time(data, len);                                                        break;
            case SELECT_GRAIN:      Lidar_Com_Entrada_Tela_Graos();                                                        break;
            case TECLAS:            if (s_em_tela_de_selecao) {Lidar_Com_Selecao_De_Grao(received_value);}                 break;
            case MONITOR: 					Controller_SetScreen(TELA_MONITOR_SYSTEM);                                             break;
            case ESCAPE:            if (s_current_screen_id == TELA_MONITOR_SYSTEM) {Controller_SetScreen(PRINCIPAL);}     break;
            case DESCARTA_AMOSTRA:  printf("Botao Descarta Amostra Pressionado\n\r");                                      break;
            case PRINT:             printf("Botao Print Pressionado\n\r");                                                 break;
						
            default:                                                                 
							break;
        }
    }
}

//================================================================================
// Implementação da Lógica Local (Seleção de Grãos)
//================================================================================

static void Lidar_Com_Entrada_Tela_Graos(void)
{
    printf("Controller: Entrando na tela de selecao de graos.\r\n");
    s_em_tela_de_selecao = true;
    uint8_t indice_salvo = 0;
    Gerenciador_Config_Get_Grao_Ativo(&indice_salvo);
    s_indice_grao_selecionado = indice_salvo;
    if (s_indice_grao_selecionado >= Gerenciador_Config_Get_Num_Graos()) {
        s_indice_grao_selecionado = 0;
    }
    Atualizar_Display_Grao_Selecionado(s_indice_grao_selecionado);
    DWIN_Driver_SetScreen(SELECT_GRAO); 
}

static void Lidar_Com_Selecao_De_Grao(int16_t tecla)
{
    printf("\r\n>> Funcao Lidar_Com_Selecao_De_Grao chamada.\r\n");
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
            printf("Controller: Grao indice '%d' selecionado. Salvando...\r\n", s_indice_grao_selecionado);
            
            bool sucesso = Gerenciador_Config_Set_Grao_Ativo(s_indice_grao_selecionado);

            if(sucesso) printf("Controller: Salvo na RAM. Sera persistido em breve.\r\n");
            else printf("Controller: ERRO ao definir o grao ativo!\r\n");
            
            s_em_tela_de_selecao = false;
            DWIN_Driver_SetScreen(PRINCIPAL); 
            break;
        case DWIN_TECLA_ESCAPE:
            printf("Controller: Selecao de grao cancelada.\r\n");
            s_em_tela_de_selecao = false;
            DWIN_Driver_SetScreen(PRINCIPAL); 
            break;
        default:
            break;
    }
     printf("<< Fim da Funcao Lidar_Com_Selecao_De_Grao.\r\n");
}


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