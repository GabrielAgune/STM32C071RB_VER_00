#ifndef GERENCIADOR_CONFIGURACOES_H
#define GERENCIADOR_CONFIGURACOES_H

#include "main.h"
#include "eeprom_driver.h"
#include <stdbool.h>
#include <stdint.h>

//==============================================================================
// Definições de Configuração
//==============================================================================

#define MAX_GRAOS 7
#define MAX_NOME_GRAO_LEN 16
#define MAX_SENHA_LEN 10
#define MAX_VALIDADE_LEN 10
#define MAX_USUARIOS 10

#define HARDWARE "1.00"
#define FIRMWARE "0.00.001"
#define FIRM_IHM "0.00.02"



//==============================================================================
// Estruturas de Dados
//==============================================================================

typedef struct {
    char nome[MAX_NOME_GRAO_LEN + 1];
    char validade[MAX_VALIDADE_LEN + 2];
    uint32_t id_curva;
    int16_t umidade_min;
    int16_t umidade_max;
} Config_Grao_t;

typedef struct {
	char Nome[20];
	char Empresa[20];
} Config_Usuario_t;

typedef struct {
    uint32_t versao_struct;
    uint8_t indice_idioma_selecionado;
    uint8_t indice_grao_ativo;
    uint8_t preenchimento[2];
    char senha_sistema[MAX_SENHA_LEN + 2];
    
    float fat_cal_a_gain;
    float fat_cal_a_zero;
		
		uint16_t nr_repetition;
		uint16_t nr_decimals;
    Config_Grao_t graos[MAX_GRAOS];
		Config_Usuario_t usuarios[MAX_USUARIOS];
		char nr_serial[16];
    uint32_t crc;
} Config_Aplicacao_t;



//==============================================================================
// Mapeamento de Memória Dinâmico e Seguro
//==============================================================================

#define CONFIG_BLOCK_SIZE sizeof(Config_Aplicacao_t) // Calcula o tamanho exato do bloco de dados.
#define CONFIG_PAGES_NEEDED ((CONFIG_BLOCK_SIZE / EEPROM_PAGE_SIZE) + 1) // (276 / 32) + 1 = 9 páginas
#define EEPROM_CONFIG_BLOCK_SPACING (CONFIG_PAGES_NEEDED * EEPROM_PAGE_SIZE) // 9 * 32 = 288 bytes

#define ADDR_CONFIG_PRIMARY   0x0000
#define ADDR_CONFIG_BACKUP1   (ADDR_CONFIG_PRIMARY + EEPROM_CONFIG_BLOCK_SPACING)
#define ADDR_CONFIG_BACKUP2   (ADDR_CONFIG_BACKUP1 + EEPROM_CONFIG_BLOCK_SPACING)

//==============================================================================
// Protótipos das Funções Públicas
//==============================================================================

void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc);
bool Gerenciador_Config_Validar_e_Restaurar(void);
bool Gerenciador_Config_Forcar_Restauracao_Padrao(void);
void Carregar_Configuracao_Padrao(void);
bool Gerenciador_Config_Verificar_Bloco(uint16_t address, Config_Aplicacao_t* config_out);
bool Gerenciador_Config_Get_Dados_Grao(uint8_t indice, Config_Grao_t* dados_grao);
bool Gerenciador_Config_Get_Senha(char* buffer, uint8_t tamanho_buffer);
bool Gerenciador_Config_Set_Senha(const char* nova_senha);
uint8_t Gerenciador_Config_Get_Num_Graos(void);
bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice);
bool Gerenciador_Config_Get_Indice_Idioma(uint8_t* indice);
bool Gerenciador_Config_Set_Grao_Ativo(uint8_t novo_indice);
bool Gerenciador_Config_Get_Grao_Ativo(uint8_t* indice_ativo);
bool Gerenciador_Config_Get_Cal_A(float* gain, float* zero);
uint16_t Gerenciador_Config_Get_NR_Repetition(void);
uint16_t Gerenciador_Config_Get_NR_Decimals(void);
bool Gerenciador_Config_Get_Serial(char* serial, uint8_t tamanho_buffer);
bool Gerenciador_Config_Set_Serial(const char* novo_serial);
bool Gerenciador_Config_Get_Usuario(char* usuario, uint8_t tamanho_usuario);
bool Gerenciador_Config_Set_Usuario(const char* novo_usuario);
bool Gerenciador_Config_Get_Company(char* empresa, uint8_t tamanho_empresa);
bool Gerenciador_Config_Set_Company(const char* nova_empresa);
bool Gerenciador_Config_Set_NR_Repetitions(uint16_t nr_repetitions);
bool Gerenciador_Config_Set_NR_Decimals(uint16_t nr_decimals);
bool Gerenciador_Config_Set_Cal_A(float gain, float zero);
void Gerenciador_Config_Get_Config_Snapshot(Config_Aplicacao_t* config_out);
void Gerenciador_Config_Run_FSM(void);

#endif // GERENCIADOR_CONFIGURACOES_H