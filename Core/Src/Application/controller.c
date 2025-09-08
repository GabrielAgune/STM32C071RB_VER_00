#include "main.h"
#include "controller.h"
#include "dwin_driver.h"
#include "rtc.h"
#include "rtc_driver.h"
#include "gerenciador_configuracoes.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//================================================================================
// Definições e Variáveis de Estado do Módulo
//================================================================================

// Definições para os valores que o DWIN envia para o VP_TECLAS (0x4080)
#define DWIN_TECLA_SETA_ESQ    0x03 // Ajustado conforme seu último log
#define DWIN_TECLA_SETA_DIR    0x02
#define DWIN_TECLA_CONFIRMA    0x01 // Ajustado conforme seu último log
#define DWIN_TECLA_ESCAPE      0x06

// Enum para controlar os passos da alteração de senha
typedef enum {
    ESTADO_SENHA_OCIOSO,
    ESTADO_SENHA_AGUARDANDO_NOVA,
    ESTADO_SENHA_AGUARDANDO_CONFIRMACAO
} EstadoSenha_t;

// Variáveis de estado para a alteração de senha
static EstadoSenha_t s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
static char s_nova_senha_temporaria[MAX_SENHA_LEN + 1];

// Variáveis de estado para a seleção de grãos
static int8_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;

// Buffers para comunicação com o DWIN Driver
static uint8_t dwin_rx_buffer[64]; 
static uint16_t dwin_rx_len = 0;
static volatile bool new_data_received = false;
static int16_t received_value = 0;


//================================================================================
// Protótipos de Funções Privadas
//================================================================================
static void Lidar_Com_Entrada_De_Senha(const uint8_t* dwin_data, uint16_t len);
static void Lidar_Com_VP_Senha(const uint8_t* dwin_data, uint16_t len);
static void Atualizar_Display_Grao_Selecionado(int8_t indice);
static void Lidar_Com_Selecao_De_Grao(int16_t tecla);
static void Lidar_Com_Entrada_Tela_Graos(void);
static void Tela_ON_OFF(void);

//================================================================================
// Função de Callback (Chamada pelo DWIN Driver)
//================================================================================
void Controller_DwinCallback(const uint8_t* data, uint16_t len)
{
    if (len > 0 && len < sizeof(dwin_rx_buffer))
    {
        memcpy(dwin_rx_buffer, data, len);
        dwin_rx_len = len;
        new_data_received = true;
    }
}

//================================================================================
// Função Principal de Processamento (Chamada pelo App_Manager)
//================================================================================
void Process_Controller(void)
{
	if (!new_data_received) {
		return; 
	}
	
	new_data_received = false;
	
	if (dwin_rx_len < 6 || dwin_rx_buffer[0] != 0x5A || dwin_rx_buffer[1] != 0xA5) {
		return; 
	}

	if (dwin_rx_buffer[3] == 0x83) {
		uint16_t vp_address = (dwin_rx_buffer[4] << 8) | dwin_rx_buffer[5];
		
		if (dwin_rx_len >= 9) {
			received_value = (dwin_rx_buffer[7] << 8) | dwin_rx_buffer[8];
		} else {
            received_value = 0; // Garante que não haja lixo se o frame for curto
        }
		
        printf("CONTROLLER_DEBUG: VP=0x%X, Valor=0x%X, s_em_tela_de_selecao=%d\r\n", 
               vp_address, received_value, s_em_tela_de_selecao);

		switch (vp_address) {
			case OFF:               Tela_ON_OFF(); break;
			case SENHA_CONFIG:      Lidar_Com_Entrada_De_Senha(dwin_rx_buffer, dwin_rx_len); break;
            case SELECT_GRAIN:      Lidar_Com_Entrada_Tela_Graos(); break;
            case TECLAS:            
                if (s_em_tela_de_selecao) {
                    Lidar_Com_Selecao_De_Grao(received_value);
                }
                break;
            case SENHA:
                Lidar_Com_VP_Senha(dwin_rx_buffer, dwin_rx_len);
                break;
			case DESCARTA_AMOSTRA:  printf("Botao Descarta Amostra Pressionado\n\r"); break;
			case PRINT:             printf("Botao Print Pressionado\n\r"); break;
			case SET_TIME:          Set_Just_Time(dwin_rx_buffer, dwin_rx_len); break;
			
			default:
				// Ação para VPs não mapeados (se necessário)
				break;
		}
	}
}

//================================================================================
// Implementação das Funções de Lógica
//================================================================================

static void Lidar_Com_Entrada_De_Senha(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 6) return;

    char senha_digitada[MAX_SENHA_LEN + 1] = {0};
    uint8_t tamanho_senha = len - 6; // Para login, não há byte de tamanho
    if (tamanho_senha > MAX_SENHA_LEN) tamanho_senha = MAX_SENHA_LEN;
    memcpy(senha_digitada, &dwin_data[6], tamanho_senha);
    for (int i = strlen(senha_digitada) - 1; i >= 0; i--) {
        if (senha_digitada[i] <= ' ') senha_digitada[i] = '\0';
        else break;
    }

    char senha_armazenada[MAX_SENHA_LEN + 1] = {0};
    if (!Gerenciador_Config_Get_Senha(senha_armazenada, sizeof(senha_armazenada))) {
        DWIN_Driver_SetScreen(MSG_ERROR);
        return;
    }
    senha_armazenada[MAX_SENHA_LEN] = '\0';

    if (strcmp(senha_digitada, senha_armazenada) == 0) {
        printf("Controller: Senha correta! Acessando menu de servico.\r\n");
        DWIN_Driver_SetScreen(TELA_SERVICO);
    } else {
        printf("Controller: Senha incorreta. Digitado: '%s' | Esperado: '%s'\r\n", senha_digitada, senha_armazenada);
        DWIN_Driver_SetScreen(SENHA_ERRADA);
    }
}


static void Lidar_Com_VP_Senha(const uint8_t* dwin_data, uint16_t len)
{
    char senha_recebida[MAX_SENHA_LEN + 1] = {0};
    if (len > 7) {
        uint8_t tamanho_a_copiar = len - 7;
        if (tamanho_a_copiar > MAX_SENHA_LEN) tamanho_a_copiar = MAX_SENHA_LEN;
        memcpy(senha_recebida, &dwin_data[7], tamanho_a_copiar);
        for (int i = strlen(senha_recebida) - 1; i >= 0; i--) {
            if (senha_recebida[i] <= ' ') senha_recebida[i] = '\0';
            else break;
        }
    }

    switch (s_estado_senha_atual)
    {
        case ESTADO_SENHA_OCIOSO:
            printf("Controller: Recebida primeira senha para alteracao.\r\n");
            if (strlen(senha_recebida) < 4) {
                printf("Controller: Nova senha muito curta.\r\n");
                DWIN_Driver_SetScreen(SENHA_MIN_4_CARAC);
            } else {
                strcpy(s_nova_senha_temporaria, senha_recebida);
                printf("Controller: Primeira senha OK. Aguardando confirmacao.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_AGUARDANDO_CONFIRMACAO;
                DWIN_Driver_SetScreen(TELA_SET_PASS_AGAIN);
            }
            break;
        
        case ESTADO_SENHA_AGUARDANDO_CONFIRMACAO:
            printf("Controller: Recebida senha de confirmacao.\r\n");
            if (strcmp(s_nova_senha_temporaria, senha_recebida) == 0) {
                printf("Controller: Senhas coincidem. Salvando nova senha...\r\n");
                
                HAL_NVIC_DisableIRQ(USART2_IRQn);
                bool sucesso = Gerenciador_Config_Set_Senha(s_nova_senha_temporaria);
                HAL_NVIC_EnableIRQ(USART2_IRQn);

                if (sucesso) printf("Controller: Salvo com sucesso!\r\n");
                else printf("Controller: ERRO ao salvar a nova senha!\r\n");
                
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                DWIN_Driver_SetScreen(TELA_CONFIGURAR);
            } else {
                printf("Controller: Senhas nao coincidem.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                DWIN_Driver_SetScreen(SENHAS_DIFERENTES);
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
            if(Gerenciador_Config_Set_Grao_Ativo(s_indice_grao_selecionado)) printf("Controller: Salvo com sucesso!\r\n");
            else printf("Controller: ERRO ao salvar a selecao do grao!\r\n");
            s_em_tela_de_selecao = false;
            DWIN_Driver_SetScreen(PRINCIPAL);
            break;
        case DWIN_TECLA_ESCAPE:
            printf("Controller: Selecao de grao cancelada.\r\n");
            s_em_tela_de_selecao = false;
            DWIN_Driver_SetScreen(PRINCIPAL);
            break;
        default:
            printf("   >> AVISO: Tecla desconhecida ou invalida recebida: 0x%02X\r\n", tecla);
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
        printf("ATT_DISPLAY: Todos os dados do indice %d foram enviados para o display.\r\n", indice);
    }
    else
    {
        printf("Controller: ERRO FATAL ao ler dados do grao no indice %d\r\n", indice);
    }
}

void Tela_ON_OFF(void)
{
	if (received_value == 0x0010)
	{
		DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_10, sizeof(CMD_AJUSTAR_BACKLIGHT_10));
		DWIN_Driver_SetScreen(SYSTEM_STANDBY);
		printf("Desliga backlight\n\r");
	}
	else
	{
		DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_100, sizeof(CMD_AJUSTAR_BACKLIGHT_100));
		DWIN_Driver_SetScreen(PRINCIPAL);
		printf("Religa backlight\n\r");
	}
}