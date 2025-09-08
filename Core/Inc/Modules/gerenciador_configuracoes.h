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
#define MAX_NOME_GRAO_LEN 15
#define MAX_SENHA_LEN 10
#define MAX_VALIDADE_LEN 10

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
    uint32_t versao_struct;
    uint8_t indice_idioma_selecionado;
    uint8_t preenchimento[3];
    char senha_sistema[MAX_SENHA_LEN + 2];
    Config_Grao_t graos[MAX_GRAOS];
    uint32_t crc;
} Config_Aplicacao_t;

//==============================================================================
// Mapeamento de Memória Dinâmico e Seguro
//==============================================================================

// 1. Calcula o tamanho exato do nosso bloco de dados.
#define CONFIG_BLOCK_SIZE sizeof(Config_Aplicacao_t) // Aprox. 276 bytes

// 2. Calcula o número de páginas de EEPROM necessárias, arredondando para cima.
#define CONFIG_PAGES_NEEDED ((CONFIG_BLOCK_SIZE / EEPROM_PAGE_SIZE) + 1) // (276 / 32) + 1 = 9 páginas

// 3. Define o espaçamento seguro como um múltiplo do tamanho da página.
//    Isso garante que nunca haverá sobreposição, mesmo se o tamanho da struct mudar.
#define EEPROM_CONFIG_BLOCK_SPACING (CONFIG_PAGES_NEEDED * EEPROM_PAGE_SIZE) // 9 * 32 = 288 bytes

// 4. Define os endereços finais com base no espaçamento seguro.
#define ADDR_CONFIG_PRIMARY   0x0000
#define ADDR_CONFIG_BACKUP1   (ADDR_CONFIG_PRIMARY + EEPROM_CONFIG_BLOCK_SPACING)
#define ADDR_CONFIG_BACKUP2   (ADDR_CONFIG_BACKUP1 + EEPROM_CONFIG_BLOCK_SPACING)

//==============================================================================
// Protótipos das Funções Públicas
//==============================================================================

void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc);
bool Gerenciador_Config_Validar_e_Restaurar(void);
bool Gerenciador_Config_Forcar_Restauracao_Padrao(void);
bool Gerenciador_Config_Verificar_Bloco(uint16_t address, Config_Aplicacao_t* config_out);
bool Gerenciador_Config_Get_Dados_Grao(uint8_t indice, Config_Grao_t* dados_grao);
bool Gerenciador_Config_Get_Senha(char* buffer, uint8_t tamanho_buffer);
bool Gerenciador_Config_Set_Senha(const char* nova_senha);
uint8_t Gerenciador_Config_Get_Num_Graos(void);
bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice);
bool Gerenciador_Config_Get_Indice_Idioma(uint8_t* indice);

#endif // GERENCIADOR_CONFIGURACOES_H