#include "interface_usuario.h"
#include "dwin_driver.h"
#include "gerenciador_configuracoes.h"
#include "GXXX_Equacoes.h"
#include "rtc_driver.h"
#include "temp_sensor.h"
#include "app_manager.h" 
#include "retarget.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define VP_SENHA_LOGIN              0x1130
#define VP_SENHA_NOVA               0x1170
#define VP_SELECAO_GRAO             0x1120
#define VP_SELECAO_IDIOMA           0x3000
#define VP_ENTRADA_DATA_HORA        0x1150
#define VP_NOME_GRAO                0x3300
#define VP_UMIDADE_MIN_GRAO         0x4140
#define VP_UMIDADE_MAX_GRAO         0x4150
#define VP_CURVA_GRAO               0x4160
#define VP_VALIDADE_GRAO            0x4170
#define VP_TEMP_INTERNA             0x5010
#define VP_DISPLAY_RTC              0x0010

#define TELA_PRINCIPAL              10
#define TELA_BACKLIGHT_ON           11
#define TELA_CONFIGURACOES          12
#define TELA_ERRO_DATA_HORA         24
#define TELA_DISPENSANDO            14
#define TELA_RASPANDO               16

static const uint8_t CMD_PEDIDO_BACKLIGHT_ON[]   = {0x5A, 0xA5, 0x06, 0x83, 0x11, 0x00, 0x01, 0x00, 0x01};
static const uint8_t CMD_PEDIDO_BACKLIGHT_OFF[]  = {0x5A, 0xA5, 0x06, 0x83, 0x11, 0x00, 0x01, 0x00, 0x00};
static const uint8_t CMD_INICIAR_PROCESSO[] = {0x5A, 0xA5, 0x06, 0x83, 0x15, 0x00, 0x01, 0x00, 0x01};
static const uint8_t CMD_AJUSTAR_BACKLIGHT_10[]  = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x0A, 0x00};
static const uint8_t CMD_AJUSTAR_BACKLIGHT_100[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x64, 0x00};
static const uint8_t CMD_ZERAR_INCREMENTO[] = {0x5A, 0xA5, 0x05, 0x82, 0x15, 0x00, 0x00, 0x00};
static const uint8_t RESPOSTA_OK_DWIN[]     = {0x5A, 0xA5, 0x03, 0x82, 0x4F, 0x4B};

typedef enum {
    UI_STATE_SPLASH_START, UI_STATE_SPLASH_VERSION, UI_STATE_SPLASH_DELAY_1,
    UI_STATE_SPLASH_SCREEN_2_TO_7, UI_STATE_SPLASH_GO_TO_MAIN, UI_STATE_SPLASH_DELAY_2,
    UI_STATE_SPLASH_UPDATE_ALL, UI_STATE_RUNNING
} UI_State_t;

static UI_State_t s_ui_state = UI_STATE_SPLASH_START;
static uint32_t s_ui_timer = 0;
static uint8_t s_splash_screen_counter = 0;
static uint8_t s_indice_grao_atual = 0;
static uint8_t s_indice_idioma_atual = 0;

// Protótipos
static void Tratar_Frame_Dwin(const uint8_t* buffer, uint16_t len);
static void Tratar_Comando_Vp(uint16_t vp_addr, const uint8_t* payload, uint16_t len);
static void Handler_Entrada_Senha(const uint8_t* payload, uint16_t len);
static void Handler_Nova_Senha(const uint8_t* payload, uint16_t len);
static void Handler_Selecao_Idioma(const uint8_t* payload, uint16_t len);
static void Handler_Selecao_Grao(const uint8_t* payload, uint16_t len);
static void Handler_Entrada_Data_Hora(const uint8_t* payload, uint16_t len);
static void Atualizar_Display_Grao(void);
static bool Parse_Dwin_String_Payload(const uint8_t* payload, uint16_t len, char* out_buffer, uint8_t max_len);
static uint8_t Ascii_Para_Uint8(char c1, char c2);
static void Log_Dwin_Rx(const uint8_t* buffer, uint16_t len);

extern UART_HandleTypeDef huart2;

void UI_Init(void) {
    DWIN_Driver_Init(&huart2, Tratar_Frame_Dwin);
    s_ui_state = UI_STATE_SPLASH_START;
    s_ui_timer = HAL_GetTick();
}

void UI_Process(void) {
    switch (s_ui_state) {
        case UI_STATE_SPLASH_START:
            if (HAL_GetTick() - s_ui_timer > 2000) { s_ui_state = UI_STATE_SPLASH_VERSION; }
            break;
        case UI_STATE_SPLASH_VERSION:
            DWIN_Driver_WriteString(0x1000, "1.00.012"); DWIN_Driver_WriteString(0x1020, "1.03");
            DWIN_Driver_WriteString(0x1040, "DWIN"); DWIN_Driver_WriteString(0x1060, "DMG48270C043");
            s_ui_timer = HAL_GetTick(); s_ui_state = UI_STATE_SPLASH_DELAY_1;
            break;
        case UI_STATE_SPLASH_DELAY_1:
            if (HAL_GetTick() - s_ui_timer > 500) {
                DWIN_Driver_SetScreen(1); s_splash_screen_counter = 2;
                s_ui_timer = HAL_GetTick(); s_ui_state = UI_STATE_SPLASH_SCREEN_2_TO_7;
            }
            break;
        case UI_STATE_SPLASH_SCREEN_2_TO_7:
            if (HAL_GetTick() - s_ui_timer > 1500) {
                if (s_splash_screen_counter <= 7) {
                    DWIN_Driver_SetScreen(s_splash_screen_counter++);
                    s_ui_timer = HAL_GetTick();
                } else { s_ui_state = UI_STATE_SPLASH_GO_TO_MAIN; }
            }
            break;
        case UI_STATE_SPLASH_GO_TO_MAIN:
            DWIN_Driver_SetScreen(TELA_PRINCIPAL); s_ui_timer = HAL_GetTick();
            s_ui_state = UI_STATE_SPLASH_DELAY_2;
            break;
        case UI_STATE_SPLASH_DELAY_2:
            if (HAL_GetTick() - s_ui_timer > 100) { s_ui_state = UI_STATE_SPLASH_UPDATE_ALL; }
            break;
        case UI_STATE_SPLASH_UPDATE_ALL:
            Gerenciador_Config_Get_Indice_Idioma(&s_indice_idioma_atual);
            Atualizar_Display_Grao(); UI_Update_Periodic();
            s_ui_state = UI_STATE_RUNNING;
            printf("UI: Inicializacao completa.\r\n");
            break;
        case UI_STATE_RUNNING: break;
    }
}

void UI_Update_Periodic(void) {
    static uint32_t s_contador_tick_temperatura = 0;
    RTC_TimeTypeDef sTime; RTC_DateTypeDef sDate;
    if (RTC_Driver_GetDateTime(&sTime, &sDate) == HAL_OK) {
        uint8_t cmd_buffer[] = {
            0x5A, 0xA5, 0x0B, 0x82, (uint8_t)(VP_DISPLAY_RTC >> 8), (uint8_t)(VP_DISPLAY_RTC & 0xFF),
            sDate.Year, sDate.Month, sDate.Date, sDate.WeekDay,
            sTime.Hours, sTime.Minutes, sTime.Seconds, 0x00
        };
        DWIN_Driver_WriteRawBytes(cmd_buffer, sizeof(cmd_buffer));
    }
    s_contador_tick_temperatura++;
    if (s_contador_tick_temperatura >= 5) {
        s_contador_tick_temperatura = 0;
        float temp_c = TempSensor_GetTemperature();
        int16_t temp_for_dwin = (int16_t)(temp_c * 10.0f);
        DWIN_Driver_WriteInt(VP_TEMP_INTERNA, temp_for_dwin);
    }
}

void UI_On_Auth_Success(void) {
    DWIN_Driver_WriteString(VP_SENHA_LOGIN, "");
    DWIN_Driver_SetScreen(TELA_CONFIGURACOES);
}

void UI_On_Auth_Failure(void) {
    DWIN_Driver_WriteString(VP_SENHA_LOGIN, "Senha Incorreta");
}

void UI_On_Process_Finished(void) {
    DWIN_Driver_SetScreen(TELA_PRINCIPAL);
    DWIN_Driver_WriteRawBytes(CMD_ZERAR_INCREMENTO, sizeof(CMD_ZERAR_INCREMENTO));
}

void UI_On_Process_Step_Changed(ServoStep_t current_step) {
    if (current_step == SERVO_STEP_FUNNEL)   { DWIN_Driver_SetScreen(TELA_DISPENSANDO); }
    else if (current_step == SERVO_STEP_SCRAPER) { DWIN_Driver_SetScreen(TELA_RASPANDO); }
}

static void Tratar_Frame_Dwin(const uint8_t* buffer, uint16_t len) {
    if (len == sizeof(RESPOSTA_OK_DWIN) && memcmp(buffer, RESPOSTA_OK_DWIN, len) == 0) return;
    Log_Dwin_Rx(buffer, len);
		
		if (len == sizeof(CMD_PEDIDO_BACKLIGHT_OFF) && memcmp(buffer, CMD_PEDIDO_BACKLIGHT_OFF, len) == 0) {
        printf("UI: Recebido pedido para DESLIGAR backlight.\r\n");
        DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_100, sizeof(CMD_AJUSTAR_BACKLIGHT_100));
				DWIN_Driver_SetScreen(TELA_PRINCIPAL);
        return; 
    }
    if (len == sizeof(CMD_PEDIDO_BACKLIGHT_ON) && memcmp(buffer, CMD_PEDIDO_BACKLIGHT_ON, len) == 0) {
        printf("UI: Recebido pedido para LIGAR backlight.\r\n");
        DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_10, sizeof(CMD_AJUSTAR_BACKLIGHT_10));
				DWIN_Driver_SetScreen(TELA_BACKLIGHT_ON);
        return; 
    }
    if (len == sizeof(CMD_INICIAR_PROCESSO) && memcmp(buffer, CMD_INICIAR_PROCESSO, len) == 0) {
        App_Manager_Handle_Start_Process();
        return;
    }
    if (len >= 6 && buffer[3] == 0x83) {
        uint16_t vp_addr = ((uint16_t)buffer[4] << 8) | buffer[5];
        Tratar_Comando_Vp(vp_addr, &buffer[6], len - 6);
    }
}

static void Tratar_Comando_Vp(uint16_t vp_addr, const uint8_t* payload, uint16_t len) {
    switch (vp_addr) {
        case VP_SENHA_LOGIN: Handler_Entrada_Senha(payload, len); break;
        case VP_SENHA_NOVA: Handler_Nova_Senha(payload, len); break;
        case VP_SELECAO_IDIOMA: Handler_Selecao_Idioma(payload, len); break;
        case VP_SELECAO_GRAO: Handler_Selecao_Grao(payload, len); break;
        case VP_ENTRADA_DATA_HORA: Handler_Entrada_Data_Hora(payload, len); break;
        default: break;
    }
}

static void Handler_Entrada_Senha(const uint8_t* payload, uint16_t len) {
    char senha[MAX_SENHA_LEN + 1];
    if (Parse_Dwin_String_Payload(payload, len, senha, sizeof(senha))) {
        App_Manager_Handle_Password_Input(senha);
    }
}

static void Handler_Nova_Senha(const uint8_t* payload, uint16_t len) {
    char nova_senha[MAX_SENHA_LEN + 1];
    if (Parse_Dwin_String_Payload(payload, len, nova_senha, sizeof(nova_senha))) {
        App_Manager_Handle_New_Password(nova_senha);
    }
}

static void Handler_Selecao_Idioma(const uint8_t* payload, uint16_t len) {
    if (len >= 3 && payload[0] == 0x01) {
        uint8_t language_value = payload[2];
        if (language_value > 0 && language_value <= 6) {
            s_indice_idioma_atual = language_value - 1;
            Atualizar_Display_Grao();
            Gerenciador_Config_Set_Indice_Idioma(s_indice_idioma_atual);
        }
    }
}

static void Handler_Selecao_Grao(const uint8_t* payload, uint16_t len) {
    if (len >= 3 && payload[0] == 0x01) {
        s_indice_grao_atual = payload[2];
        Atualizar_Display_Grao();
    }
}

static void Handler_Entrada_Data_Hora(const uint8_t* payload, uint16_t len) {
    if (len >= 13) {
        uint8_t d = Ascii_Para_Uint8(payload[1], payload[2]);
        uint8_t m = Ascii_Para_Uint8(payload[3], payload[4]);
        uint8_t y = Ascii_Para_Uint8(payload[5], payload[6]);
        uint8_t h = Ascii_Para_Uint8(payload[7], payload[8]);
        uint8_t min = Ascii_Para_Uint8(payload[9], payload[10]);
        uint8_t s = Ascii_Para_Uint8(payload[11], payload[12]);
        App_Manager_Handle_DateTime_Input(d, m, y, h, min, s);
    } else {
        DWIN_Driver_SetScreen(TELA_ERRO_DATA_HORA);
    }
    DWIN_Driver_WriteString(VP_ENTRADA_DATA_HORA, "");
}

static void Atualizar_Display_Grao(void) {
    char nome_grao[MAX_NOME_GRAO_LEN + 1];
    strncpy(nome_grao, Produto[s_indice_grao_atual].Nome[s_indice_idioma_atual], MAX_NOME_GRAO_LEN);
    nome_grao[MAX_NOME_GRAO_LEN] = '\0';
    DWIN_Driver_WriteString(VP_NOME_GRAO, nome_grao);

    Config_Grao_t dados_grao_eeprom;
    if (Gerenciador_Config_Get_Dados_Grao(s_indice_grao_atual, &dados_grao_eeprom)) {
        DWIN_Driver_WriteInt(VP_UMIDADE_MIN_GRAO, dados_grao_eeprom.umidade_min);
        DWIN_Driver_WriteInt(VP_UMIDADE_MAX_GRAO, dados_grao_eeprom.umidade_max);
        DWIN_Driver_WriteInt(VP_CURVA_GRAO, dados_grao_eeprom.id_curva);
        DWIN_Driver_WriteString(VP_VALIDADE_GRAO, dados_grao_eeprom.validade);
    }
}

static bool Parse_Dwin_String_Payload(const uint8_t* payload, uint16_t len, char* out_buffer, uint8_t max_len) {
    if (payload == NULL || out_buffer == NULL || len < 2) return false;
    memset(out_buffer, 0, max_len);
    uint8_t copy_len = payload[0];
    if (copy_len > len - 1) copy_len = len - 1;
    if (copy_len > max_len - 1) copy_len = max_len - 1;
    memcpy(out_buffer, &payload[1], copy_len);
    return true;
}

static uint8_t Ascii_Para_Uint8(char c1, char c2) {
    uint8_t val = 0;
    if (c1 >= '0' && c1 <= '9') { val = (c1 - '0') * 10; }
    if (c2 >= '0' && c2 <= '9') { val += (c2 - '0'); }
    return val;
}

static void Log_Dwin_Rx(const uint8_t* buffer, uint16_t len) {
    g_retarget_dest = TARGET_DEBUG;
    printf("DWIN RX << ");
    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", buffer[i]);
    }
    printf("\r\n");
}