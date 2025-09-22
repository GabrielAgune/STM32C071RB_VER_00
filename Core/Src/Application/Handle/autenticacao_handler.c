#include "autenticacao_handler.h"

//================================================================================
// Definições e Variáveis Estáticas (Movidas do controller.c)
//================================================================================
typedef enum {
    ESTADO_SENHA_OCIOSO,
    ESTADO_SENHA_AGUARDANDO_CONFIRMACAO
} EstadoSenha_t;

static EstadoSenha_t s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
static char s_nova_senha_temporaria[MAX_SENHA_LEN + 1];

//================================================================================
// Implementação
//================================================================================

/**
 * @brief (V8.3) Trata a entrada de senha usando o parser robusto.
 * Lógica movida do controller.c
 */
void Auth_Handle_Login(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) { 
        printf("Auth: Frame de senha muito curto.\r\n");
        return;
    }
    
    char senha_digitada[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6]; 
    uint16_t payload_len = len - 6;

    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, senha_digitada, sizeof(senha_digitada))) {
        printf("Auth: Falha no parser robusto da senha.\r\n");
        return;
    }

    if (strlen(senha_digitada) == 0) {
        printf("Auth: Senha vazia recebida.\r\n");
        DWIN_Driver_SetScreen(SENHA_ERRADA); 
        return;
    }

    char senha_armazenada[MAX_SENHA_LEN + 1] = {0};
    if (!Gerenciador_Config_Get_Senha(senha_armazenada, sizeof(senha_armazenada))) {
        DWIN_Driver_SetScreen(MSG_ERROR); 
        return;
    }
    senha_armazenada[MAX_SENHA_LEN] = '\0';

    if (strcmp(senha_digitada, senha_armazenada) == 0) {
        printf("Auth: Senha correta! Acessando menu de servico.\r\n");
        DWIN_Driver_SetScreen(TELA_CONFIGURAR); 
    } else {
        printf("Auth: Senha incorreta. Digitado: '%s' | Esperado: '%s'\r\n", senha_digitada, senha_armazenada);
        DWIN_Driver_SetScreen(SENHA_ERRADA); 
    }
}


/**
 * @brief (V8.3) Trata a nova senha usando o parser robusto.
 * Lógica movida do controller.c
 */
void Auth_Handle_Set_Password(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) return; 

    char senha_recebida[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6];
    uint16_t payload_len = len - 6;

    if (!DWIN_Parse_String_Payload_Robust(payload, payload_len, senha_recebida, sizeof(senha_recebida))) {
         printf("Auth: Falha no parser de nova senha.\r\n");
        return;
    }

    if (strlen(senha_recebida) == 0) {
        printf("Auth: Nova senha vazia descartada.\r\n");
        return;
    }

    switch (s_estado_senha_atual)
    {
        case ESTADO_SENHA_OCIOSO:
            printf("Auth: Recebida primeira senha para alteracao.\r\n");
            if (strlen(senha_recebida) < 4) {
                printf("Auth: Nova senha muito curta.\r\n");
                DWIN_Driver_SetScreen(SENHA_MIN_4_CARAC); 
            } else {
                strcpy(s_nova_senha_temporaria, senha_recebida);
                printf("Auth: Primeira senha OK. Aguardando confirmacao.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_AGUARDANDO_CONFIRMACAO;
                DWIN_Driver_SetScreen(TELA_SET_PASS_AGAIN); 
            }
            break;
        
        case ESTADO_SENHA_AGUARDANDO_CONFIRMACAO:
            printf("Auth: Recebida senha de confirmacao.\r\n");
            if (strcmp(s_nova_senha_temporaria, senha_recebida) == 0) {
                printf("Auth: Senhas coincidem. Salvando nova senha...\r\n");
                
                bool sucesso = Gerenciador_Config_Set_Senha(s_nova_senha_temporaria); 

                if (sucesso) printf("Auth: Nova senha definida na RAM. Sera salva em breve.\r\n");
                else printf("Auth: ERRO ao definir a nova senha (FSM ocupada?)\r\n");
                
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                DWIN_Driver_SetScreen(TELA_CONFIGURAR); 
            } else {
                printf("Auth: Senhas nao coincidem.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                DWIN_Driver_SetScreen(SENHAS_DIFERENTES); 
            }
            break;

        default:
            s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
            break;
    }
}