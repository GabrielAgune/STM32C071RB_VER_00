#ifndef __DWIN_DRIVER_H
#define __DWIN_DRIVER_H

#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

static const uint8_t CMD_AJUSTAR_BACKLIGHT_10[]  = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x0A, 0x00};
static const uint8_t CMD_AJUSTAR_BACKLIGHT_100[] = {0x5A, 0xA5, 0x05, 0x82, 0x00, 0x82, 0x64, 0x00};

enum
{
		//Váriaveis Globais
		VP_FIRMWARE      = 0x1000,
		VP_HARDWARE      = 0x1010,
		VP_FIRM_IHM      = 0x1020,
		VP_SERIAL        = 0x1030,
		
		//Data e hora
		HORA_SISTEMA     = 0x2000,
		DATA_SISTEMA     = 0x2010,
		
		//Variáveis relatório de medidas
		GRAO_A_MEDIR     = 0x2070,
		UMIDADE_1_CASA   = 0x2100,
		UMIDADE_2_CASAS  = 0x2100,
		TEMP_SAMPLE      = 0x2110,
		DENSIDADE        = 0x2120,
		CURVA            = 0x2130,
		AMOSTRAS         = 0x2140,
		UMI_MIN          = 0x2150,
		UMI_MAX          = 0x2160,
		DATA_VAL         = 0x2170,
		RESULTADO_MEDIDA = 0x2180,
		
		//Váriaveis sistema
		PESO             = 0x2190,
		AD_BALANCA       = 0x2200,
		FAT_CAL_BAL      = 0x2210,
		AD_TEMP_SAMPLE   = 0x2220,
		TEMP_INSTRU      = 0x2230,
		AD_TEMP_INSTRU   = 0x2240,
		FREQUENCIA       = 0x2250,
		ESCALA_A         = 0x2260,
		PHOTDIODE        = 0x2270,
		GAVETA           = 0x2280,
		VP_MESSAGES      = 0x4096,
};


enum
{
	// Tela Principal
	OFF               = 0x2020,
	SENHA_CONFIG      = 0x2030,
	SELECT_GRAIN      = 0x2040,
	PRINT             = 0x2050,
	DESCARTA_AMOSTRA  = 0x2060,
	
	// Menu Configura??o
	SET_TIME          = 0x300F,
	NR_REPETICOES     = 0x3020,
	DECIMALS          = 0x3030,
	DES_HAB_PRINT     = 0x3040,
	SENHA             = 0x3060,
	DIAGNOSTIC        = 0x3070,
	USER              = 0x3080,
	COMPANY           = 0x3090,
	ABOUT_SYS         = 0x3100,
	
	// Menu Servi?o
	TECLAS            = 0X4080,
	ESCAPE            = 0X5000,
	PRESET_PRODUCT    = 0X7010,
	SET_DATE_TIME     = 0X7020,
	MODEL_OEM         = 0X7030,
	ADJUST_SCALE      = 0X7040,
	ADJUST_TERMO      = 0X7050,
	ADJUST_CAPA       = 0X7060,
	SET_SERIAL        = 0X7070,
	SET_UNITS         = 0X7080,
	MONITOR           = 0X7090,
	SERVICE_REPORT    = 0X7100,
	SYSTEM_BURNIN     = 0X7110,
	
};

/*********************************************************
 ID DAS TELAS NO DISPLAY
**********************************************************/

enum
{
	LOGO                  =   0,
	BOOT_CHECK_SERVOS     =   1,
	BOOT_CHECK_CAPACI     =   2,
	BOOT_BALANCE          =   3,
	BOOT_THERMOMETER      =   4,
	BOOT_MEMORY           =   5,
	BOOT_CLOCK            =   6,
	BOOT_CRIPTO           =   7,
	
	PRINCIPAL             =   8,
	SYSTEM_STANDBY        =  11,
	
	MEDE_AJUSTANDO        =  14,
	MEDE_ENCHE_CAMARA     =  13,
	MEDE_RASPA_CAMARA     =  15,
	MEDE_PESO_AMOSTRA     =  16,
	MEDE_TEMP_SAMPLE      =  17,
	MEDE_UMIDADE          =  18,
	MEDE_RESULT_01        =  19,
	MEDE_RESULT_02        = 119,
	MEDE_REPETICAO        =  21,
	MEDE_PRINT_REPORT     =  22,
	
	SELECT_GRAO           =  36,
	
	TELA_CONFIGURAR       =  23,
	TELA_SET_JUST_TIME    =  25,
	TELA_SETUP_REPETICOES =  26,
	TELA_SET_DECIMALS     =  27,
	TELA_SET_COPIES       =  28,
	TELA_SET_BRIGHT       =  29,
	TELA_SET_PASSWORD     =  30,
	TELA_SET_PASS_AGAIN   =  31,
	TELA_AUTO_DIAGNOSIS   =  32,
	TELA_USER             =  34,
	TELA_COMPANY          =  35,
	TELA_ABOUT_SYSTEM     =  33,
	
	TELA_SERVICO          =  46,
	TELA_PRESET_PRODUCT   =  48,
	TELA_ADJUST_TIME      =  49,
	TELA_MODEL_OEM        =  50,
	TELA_ADJUST_SCALE     =  51,
	TELA_ADJUST_TERMO     =  52,
	TELA_ADJUST_CAPA      =  53,
	TELA_SET_SERIAL       =  54,
	TELA_SET_UNITS        =  55,
	TELA_MONITOR_SYSTEM   =  56,
	TELA_REPORT_SERV      =  57,
	TELA_BURNIN           =  58,
	
	MSG_ERROR             =  59,
	MSG_ALERTA            =  60,
	ERROR_GAVETA_MISS     =  61,
	SENHA_ERRADA          =  62,
	SENHA_MIN_4_CARAC     =  63,
	SENHAS_DIFERENTES     =  64,
	
};



// Callback para a camada de Controle
typedef void (*dwin_rx_callback_t)(const uint8_t* buffer, uint16_t len);

// Funções Públicas (API do Driver V8.1)
void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback);
void DWIN_Driver_Process(void); // Processa o buffer de RX (com debounce de pacote)
void DWIN_TX_Pump(void);        // "Bomba" de TX do DWIN (chamada no super-loop)
bool DWIN_Driver_IsTxBusy(void); // Verifica se o FIFO de TX não está vazio

// Funções de Escrita (Assíncronas e Enfileiradas)
void DWIN_Driver_SetScreen(uint16_t screen_id);
void DWIN_Driver_WriteInt(uint16_t vp_address, int16_t value);
void DWIN_Driver_WriteInt32(uint16_t vp_address, int32_t value);
void DWIN_Driver_WriteRawBytes(const uint8_t* data, uint16_t size);
void DWIN_Driver_WriteString(uint16_t vp_address, const char* text, uint16_t max_len);

// --- Handlers de ISR (Chamados pelos Callbacks do HAL) ---
void DWIN_Driver_HandleTxCplt(UART_HandleTypeDef *huart);
void DWIN_Driver_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size); // (Para RX IDLE+DMA)
void DWIN_Driver_HandleError(UART_HandleTypeDef *huart);
#endif // __DWIN_DRIVER_H