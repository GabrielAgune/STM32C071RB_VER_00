/*******************************************************************************
 * @file        password_handler.c
 * @brief       Password Input and Validation
 * @version     8.3 (Extracted from controller.c during refactoring)  
 * @details     Handles password entry state machine and validation
 ******************************************************************************/

#include "password_handler.h"
#include "screen_navigation.h"
#include "dwin_driver.h"
#include "gerenciador_configuracoes.h"
#include <string.h>
#include <stdio.h>

//================================================================================
// Definitions
//================================================================================
#define MAX_SENHA_LEN 10

//================================================================================
// Types
//================================================================================
typedef enum {
    ESTADO_SENHA_OCIOSO,
    ESTADO_SENHA_AGUARDANDO_CONFIRMACAO
} EstadoSenha_t;

//================================================================================
// Static Variables
//================================================================================
static EstadoSenha_t s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
static char s_nova_senha_temporaria[MAX_SENHA_LEN + 1];

//================================================================================
// Private Function Prototypes
//================================================================================
static bool Parse_Dwin_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len);

//================================================================================
// Public Functions
//================================================================================

void Password_Handler_Init(void)
{
    s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
    memset(s_nova_senha_temporaria, 0, sizeof(s_nova_senha_temporaria));
}

void Password_Handler_ProcessConfig(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) { 
        printf("Password Handler: Password frame too short.\r\n");
        return;
    }
    
    char senha_digitada[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6]; 
    uint16_t payload_len = len - 6;

    if (!Parse_Dwin_String_Payload_Robust(payload, payload_len, senha_digitada, sizeof(senha_digitada))) {
        printf("Password Handler: Robust parser failed for password.\r\n");
        return;
    }

    if (strlen(senha_digitada) == 0) {
        printf("Password Handler: Empty password received.\r\n");
        Screen_Navigation_SetScreen(SENHA_ERRADA); 
        return;
    }

    char senha_armazenada[MAX_SENHA_LEN + 1] = {0};
    if (!Gerenciador_Config_Get_Senha(senha_armazenada, sizeof(senha_armazenada))) {
        Screen_Navigation_SetScreen(MSG_ERROR); 
        return;
    }
    senha_armazenada[MAX_SENHA_LEN] = '\0';

    if (strcmp(senha_digitada, senha_armazenada) == 0) {
        printf("Password Handler: Correct password! Accessing service menu.\r\n");
        Screen_Navigation_SetScreen(TELA_SERVICO); 
    } else {
        printf("Password Handler: Incorrect password. Entered: '%s' | Expected: '%s'\r\n", 
               senha_digitada, senha_armazenada);
        Screen_Navigation_SetScreen(SENHA_ERRADA); 
    }
}

void Password_Handler_ProcessLogin(const uint8_t* dwin_data, uint16_t len)
{
    if (len <= 7) return; 

    char senha_recebida[MAX_SENHA_LEN + 1];
    const uint8_t* payload = &dwin_data[6];
    uint16_t payload_len = len - 6;

    if (!Parse_Dwin_String_Payload_Robust(payload, payload_len, senha_recebida, sizeof(senha_recebida))) {
         printf("Password Handler: Failed to parse new password.\r\n");
        return;
    }

    if (strlen(senha_recebida) == 0) {
        printf("Password Handler: Empty new password discarded.\r\n");
        return;
    }

    switch (s_estado_senha_atual) {
        case ESTADO_SENHA_OCIOSO:
            printf("Password Handler: Received first password for change.\r\n");
            if (strlen(senha_recebida) < 4) {
                printf("Password Handler: New password too short.\r\n");
                Screen_Navigation_SetScreen(SENHA_MIN_4_CARAC); 
            } else {
                strcpy(s_nova_senha_temporaria, senha_recebida);
                printf("Password Handler: First password OK. Awaiting confirmation.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_AGUARDANDO_CONFIRMACAO;
                Screen_Navigation_SetScreen(TELA_SET_PASS_AGAIN); 
            }
            break;
        
        case ESTADO_SENHA_AGUARDANDO_CONFIRMACAO:
            printf("Password Handler: Received confirmation password.\r\n");
            if (strcmp(s_nova_senha_temporaria, senha_recebida) == 0) {
                printf("Password Handler: Passwords match. Saving new password...\r\n");
                
                bool sucesso = Gerenciador_Config_Set_Senha(s_nova_senha_temporaria);

                if (sucesso) {
                    printf("Password Handler: New password set in RAM. Will be saved soon.\r\n");
                } else {
                    printf("Password Handler: ERROR setting new password (FSM busy?)\r\n");
                }
                
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                Screen_Navigation_SetScreen(TELA_CONFIGURAR); 
            } else {
                printf("Password Handler: Passwords don't match.\r\n");
                s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
                Screen_Navigation_SetScreen(SENHAS_DIFERENTES); 
            }
            break;

        default:
            s_estado_senha_atual = ESTADO_SENHA_OCIOSO;
            break;
    }
}

bool Password_Handler_IsAwaitingConfirmation(void)
{
    return s_estado_senha_atual == ESTADO_SENHA_AGUARDANDO_CONFIRMACAO;
}

//================================================================================
// Private Functions
//================================================================================

static bool Parse_Dwin_String_Payload_Robust(const uint8_t* payload, uint16_t payload_len, char* out_buffer, uint8_t max_len)
{
    if (payload == NULL || out_buffer == NULL || payload_len <= 1 || max_len == 0) {
        return false;
    }
    
    memset(out_buffer, 0, max_len);
    
    for (uint16_t i = 0; i < payload_len && i < (max_len - 1); i++) {
        uint8_t c = payload[i];
        if (c == 0x00 || c == 0xFF) {
            continue; 
        }
        out_buffer[i] = c;
    }
    out_buffer[max_len - 1] = '\0'; 
    return true;
}