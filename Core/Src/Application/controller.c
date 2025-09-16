// Core/Src/Application/controller.c
// VERSÃO 8.4 - Refatorada por Dev STM
// CORREÇÃO: Adiciona rastreamento de entrada/saída da TELA_MONITOR_SYSTEM.

#include "main.h"
#include "controller.h"
#include "dwin_driver.h" // Necessário para ENUMs de Tela (PRINCIPAL, etc.)
#include "rtc.h"
#include "rtc_driver.h" 
#include "gerenciador_configuracoes.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//================================================================================
// Definições, Enums, Variáveis Estáticas e Protótipos
//================================================================================
#define DWIN_TECLA_SETA_ESQ    0x03 
#define DWIN_TECLA_SETA_DIR    0x02
#define DWIN_TECLA_CONFIRMA    0x01 
#define DWIN_TECLA_ESCAPE      0x06

typedef enum {
    ESTADO_SENHA_OCIOSO,
    ESTADO_SENHA_AGUARDANDO_CONFIRMACAO
} EstadoSenha_t;

static EstadoSenha_t s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
static char s_nova_senha_temporaria[MAX_SENHA_LEN + 1];
static int8_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;
static int16_t received_value = 0;
static uint16_t s_current_screen_id = PRINCIPAL; // (V8.3) RASTREADOR DE TELA ATIVA

// Protótipos estáticos
static void Lidar_Com_Entrada_De_Senha(const uint8_t* dwin_data, uint16_t len);
static void Lidar_Com_VP_Senha(const uint8_t* dwin_data, uint16_t len);
static void Atualizar_Display_Grao_Selecionado(int8_t indice);
static void Lidar_Com_Selecao_De_Grao(int16_t tecla);
static void Lidar_Com_Entrada_Tela_Graos(void);
static void Tela_ON_OFF(void);
static void Set_Just_Time_Parser(const uint8_t* rx_buffer, uint16_t rx_len); 
static void Set_Active_Screen(uint16_t screen_id); // (V8.3) Wrapper de rastreamento
static bool Parse_Dwin_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len); // (V8.3) Parser Robusto

//================================================================================
// Funções Públicas (Getters)
//================================================================================

/**
 * @brief (V8.3) Retorna a tela que o controlador acredita estar ativa.
 */
uint16_t Controller_GetCurrentScreen(void)
{
    return s_current_screen_id;
}

/**
 * @brief (V8.3) Wrapper para DWIN_Driver_SetScreen que rastreia a tela atual.
 */
static void Set_Active_Screen(uint16_t screen_id)
{
    s_current_screen_id = screen_id;
    DWIN_Driver_SetScreen(screen_id);
}


//================================================================================
// Função de Callback (Chamada pelo DWIN Driver)
//================================================================================
void Controller_DwinCallback(const uint8_t* data, uint16_t len)
{
    if (len < 6 || data[0] != 0x5A || data[1] != 0xA5) {
        return; 
    }

    if (data[3] == 0x83) { // Comando de Retorno de VP (0x83 significa "VP escrito pelo display")
        uint16_t vp_address = (data[4] << 8) | data[5];
        
        if (len >= 8) {
            // Lógica de captura de 'received_value' (para teclas, etc.)
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
        
        // Despachante de comandos VP
        switch (vp_address) {
            case OFF:               Tela_ON_OFF(); break;
            case SENHA_CONFIG:      Lidar_Com_Entrada_De_Senha(data, len); break;
            case SELECT_GRAIN:      Lidar_Com_Entrada_Tela_Graos(); break;
            case TECLAS:            
                if (s_em_tela_de_selecao) {
                    Lidar_Com_Selecao_De_Grao(received_value);
                }
                break;
            case SENHA:
                Lidar_Com_VP_Senha(data, len);
                break;
            case DESCARTA_AMOSTRA:  printf("Botao Descarta Amostra Pressionado\n\r"); break;
            case PRINT:             printf("Botao Print Pressionado\n\r"); break;
            case SET_TIME:          
                Set_Just_Time_Parser(data, len);
                break;
            
            // **** INÍCIO DA CORREÇÃO V8.4 ****
            case MONITOR: // VP 0x7090 (O usuário pressionou o botão MONITOR)
                // O usuário pressionou o botão para entrar no monitor.
                // A tela DWIN mudou para 56. Devemos atualizar nosso estado interno.
                Set_Active_Screen(TELA_MONITOR_SYSTEM); // Tela 56
                printf("CONTROLLER: Entrando na Tela de Monitor do Sistema.\r\n");
                break;
            
            case ESCAPE: // VP 0x5000 (Provavelmente o botão "Voltar" do Monitor/Serviço)
                // Se estamos no monitor, voltamos para a tela de serviço.
                if (s_current_screen_id == TELA_MONITOR_SYSTEM) {
                     Set_Active_Screen(TELA_SERVICO); // Tela 46
                     printf("CONTROLLER: Saindo do Monitor -> Tela de Servico.\r\n");
                }
                // (Adicione outros 'else if' se o ESCAPE for usado em outras telas)
                break;
            // **** FIM DA CORREÇÃO V8.4 ****
            
            default:
                break;
        }
    }
}

//================================================================================
// Implementação das Funções de Lógica e Parsers
//================================================================================

/**
 * @brief (V8.3) Parser de string robusto
 */
static bool Parse_Dwin_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len)
{
    if (payload == NULL || out_buffer == NULL || payload_len <= 1 || max_len == 0) {
        return false;
    }
		
    memset(out_buffer, 0, max_len);
		
    for (int i = 0; i < (max_len - 1) && (1 + i) < payload_len; i++)
    {
        char c = (char)payload[1 + i];
        if (c == (char)0xFF) 
        {
            break;
        }
        if (c < ' ') 
        {
            continue; 
        }
        out_buffer[i] = c;
    }
    out_buffer[max_len - 1] = '\0'; 
    return true;
}


/**
 * @brief (V8.3) Trata a entrada de senha usando o parser robusto.
 */
static void Lidar_Com_Entrada_De_Senha(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) { 
        printf("Controller: Frame de senha muito curto.\r\n");
        return;
    }
    
    char senha_digitada[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6]; 
    uint16_t payload_len = len - 6;

    if (!Parse_Dwin_String_Payload_Robust(payload, payload_len, senha_digitada, sizeof(senha_digitada))) {
        printf("Controller: Falha no parser robusto da senha.\r\n");
        return;
    }

    if (strlen(senha_digitada) == 0) {
        printf("Controller: Senha vazia recebida.\r\n");
        Set_Active_Screen(SENHA_ERRADA); 
        return;
    }

    char senha_armazenada[MAX_SENHA_LEN + 1] = {0};
    if (!Gerenciador_Config_Get_Senha(senha_armazenada, sizeof(senha_armazenada))) {
        Set_Active_Screen(MSG_ERROR); 
        return;
    }
    senha_armazenada[MAX_SENHA_LEN] = '\0';

    if (strcmp(senha_digitada, senha_armazenada) == 0) {
        printf("Controller: Senha correta! Acessando menu de servico.\r\n");
        Set_Active_Screen(TELA_SERVICO); 
    } else {
        printf("Controller: Senha incorreta. Digitado: '%s' | Esperado: '%s'\r\n", senha_digitada, senha_armazenada);
        Set_Active_Screen(SENHA_ERRADA); 
    }
}


/**
 * @brief (V8.3) Trata a nova senha usando o parser robusto.
 */
static void Lidar_Com_VP_Senha(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) return; 

    char senha_recebida[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6];
    uint16_t payload_len = len - 6;

    if (!Parse_Dwin_String_Payload_Robust(payload, payload_len, senha_recebida, sizeof(senha_recebida))) {
         printf("Controller: Falha no parser de nova senha.\r\n");
        return;
    }

    if (strlen(senha_recebida) == 0) {
        printf("Controller: Nova senha vazia descartada.\r\n");
        return;
    }

    switch (s_estado_senha_atual)
    {
        case ESTADO_SENHA_OCIOSO:
            printf("Controller: Recebida primeira senha para alteracao.\r\n");
            if (strlen(senha_recebida) < 4) {
                printf("Controller: Nova senha muito curta.\r\n");
                Set_Active_Screen(SENHA_MIN_4_CARAC); 
            } else {
                strcpy(s_nova_senha_temporaria, senha_recebida);
                printf("Controller: Primeira senha OK. Aguardando confirmacao.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_AGUARDANDO_CONFIRMACAO;
                Set_Active_Screen(TELA_SET_PASS_AGAIN); 
            }
            break;
        
        case ESTADO_SENHA_AGUARDANDO_CONFIRMACAO:
            printf("Controller: Recebida senha de confirmacao.\r\n");
            if (strcmp(s_nova_senha_temporaria, senha_recebida) == 0) {
                printf("Controller: Senhas coincidem. Salvando nova senha...\r\n");
                
                bool sucesso = Gerenciador_Config_Set_Senha(s_nova_senha_temporaria); // Não-bloqueante

                if (sucesso) printf("Controller: Nova senha definida na RAM. Sera salva em breve.\r\n");
                else printf("Controller: ERRO ao definir a nova senha (FSM ocupada?)\r\n");
                
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                Set_Active_Screen(TELA_CONFIGURAR); 
            } else {
                printf("Controller: Senhas nao coincidem.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                Set_Active_Screen(SENHAS_DIFERENTES); 
            }
            break;

        default:
            s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
            break;
    }
}


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
    Set_Active_Screen(SELECT_GRAO); 
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
            Set_Active_Screen(PRINCIPAL); 
            break;
        case DWIN_TECLA_ESCAPE:
            printf("Controller: Selecao de grao cancelada.\r\n");
            s_em_tela_de_selecao = false;
            Set_Active_Screen(PRINCIPAL); 
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

void Tela_ON_OFF(void)
{
	if (received_value == 0x0010) {
		DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_10, sizeof(CMD_AJUSTAR_BACKLIGHT_10));
		Set_Active_Screen(SYSTEM_STANDBY); 
		printf("Desliga backlight\n\r");
	}
	else {
		DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_100, sizeof(CMD_AJUSTAR_BACKLIGHT_100));
		Set_Active_Screen(PRINCIPAL); 
		printf("Religa backlight\n\r");
	}
}

/**
 * @brief (V8.3) Trata a entrada de hora/data usando o parser robusto.
 */
static void Set_Just_Time_Parser(const uint8_t* rx_buffer, uint16_t rx_len)
{
    char time_str_safe[16]; 
    int hours, minutes, seconds;

    if (rx_len > 7) { 
        const uint8_t* payload = &rx_buffer[6];
        uint16_t payload_len = rx_len - 6;

        if (!Parse_Dwin_String_Payload_Robust(payload, payload_len, time_str_safe, sizeof(time_str_safe)))
        {
            printf("RTC Driver: Falha ao extrair string de tempo (parser robusto).\r\n");
            return;
        }

        if (sscanf(time_str_safe, "%d:%d:%d", &hours, &minutes, &seconds) == 3)
        {
            RTC_Driver_SetTime(hours, minutes, seconds); 
            printf("RTC atualizado com sucesso para: %s\r\n", time_str_safe);
        }
        else
        {
             printf("RTC Driver: Falha ao converter a string DWIN '%s'.\r\n", time_str_safe);
        }
    }
}