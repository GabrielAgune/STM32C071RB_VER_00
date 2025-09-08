/*******************************************************************************
 * @file        gerenciador_configuracoes.c
 * @brief       Gerencia o armazenamento e a recuperação de configurações na EEPROM.
 * @details     Este módulo é responsável por salvar, ler e validar a estrutura
 * de configurações da aplicação, usando um CRC32 para garantir a
 * integridade e uma estratégia de 3 cópias (primária + 2 backups)
 * para garantir a redundância e recuperação de dados.
 ******************************************************************************/

#include "gerenciador_configuracoes.h"
#include "eeprom_driver.h"
#include "GXXX_Equacoes.h"
#include "retarget.h"
#include <string.h>
#include <stdio.h>
#include <stddef.h>

static CRC_HandleTypeDef *s_crc_handle = NULL;

static void Recalcular_E_Atualizar_CRC(Config_Aplicacao_t* config);
static bool Salvar_Configuracao_Completa(const Config_Aplicacao_t* config);
static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config);
static bool Carregar_Primeira_Config_Valida(Config_Aplicacao_t* config);

void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc)
{
    s_crc_handle = hcrc;
}

bool Gerenciador_Config_Validar_e_Restaurar(void)
{
    if (s_crc_handle == NULL) return false;
    static Config_Aplicacao_t config_temp;

    printf("EEPROM Manager: Verificando integridade dos dados...\n");

    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_PRIMARY, &config_temp))
    {	
				printf("EEPROM Manager: Integridade dos dados OK!\n\r");
        return true;
    }
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP1, &config_temp))
    {
        printf("EEPROM Manager: Primario corrompido. Restaurando a partir do Backup 1.\n");
        return Salvar_Configuracao_Completa(&config_temp);
    }
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP2, &config_temp))
    {
        printf("EEPROM Manager: Primario e Backup 1 corrompidos. Restaurando a partir do Backup 2.\n");
        return Salvar_Configuracao_Completa(&config_temp);
    }

    printf("EEPROM Manager: ERRO FATAL! Todas as copias de dados estao corrompidas.\n");
    return Gerenciador_Config_Forcar_Restauracao_Padrao();
}

bool Gerenciador_Config_Forcar_Restauracao_Padrao(void)
{
    if (s_crc_handle == NULL) return false;

    static Config_Aplicacao_t config_padrao;
    memset(&config_padrao, 0, sizeof(Config_Aplicacao_t));

    printf("EEPROM Manager: Carregando configuracoes de fabrica...\n");

    config_padrao.versao_struct = 1;
    config_padrao.indice_idioma_selecionado = 0;
    strncpy(config_padrao.senha_sistema, "senha", MAX_SENHA_LEN);
    config_padrao.senha_sistema[MAX_SENHA_LEN] = '\0';
		config_padrao.fat_cal_a_gain = 1.0f;
    config_padrao.fat_cal_a_zero = 0.0f;
		
    for (int i = 0; i < MAX_GRAOS; i++)
    {
        strncpy(config_padrao.graos[i].nome, Produto[i].Nome[0], MAX_NOME_GRAO_LEN);
        config_padrao.graos[i].nome[MAX_NOME_GRAO_LEN] = '\0';
        strncpy(config_padrao.graos[i].validade, "22/06/2028", MAX_VALIDADE_LEN);
        config_padrao.graos[i].validade[MAX_VALIDADE_LEN] = '\0';
        config_padrao.graos[i].id_curva = Produto[i].Nr_Equa;
        config_padrao.graos[i].umidade_min = Produto[i].Um_Min;
        config_padrao.graos[i].umidade_max = Produto[i].Um_Max;
    }

    Recalcular_E_Atualizar_CRC(&config_padrao);
    return Salvar_Configuracao_Completa(&config_padrao);
}

bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice)
{
    if (s_crc_handle == NULL) return false;
    static Config_Aplicacao_t config_temp;
    if (!Carregar_Primeira_Config_Valida(&config_temp)) return false;

    config_temp.indice_idioma_selecionado = novo_indice;
    Recalcular_E_Atualizar_CRC(&config_temp);
    return Salvar_Configuracao_Completa(&config_temp);
}

bool Gerenciador_Config_Set_Senha(const char* nova_senha)
{
    if (s_crc_handle == NULL || nova_senha == NULL) return false;
    static Config_Aplicacao_t config_temp;
    if (!Carregar_Primeira_Config_Valida(&config_temp)) return false;

    strncpy(config_temp.senha_sistema, nova_senha, MAX_SENHA_LEN);
    config_temp.senha_sistema[MAX_SENHA_LEN] = '\0';
    Recalcular_E_Atualizar_CRC(&config_temp);
    return Salvar_Configuracao_Completa(&config_temp);
}

bool Gerenciador_Config_Get_Indice_Idioma(uint8_t* indice)
{
    if (s_crc_handle == NULL || indice == NULL) return false;
    Config_Aplicacao_t config_temp;
    if (!Tentar_Carregar_De_Endereco(ADDR_CONFIG_PRIMARY, &config_temp)) return false;
    *indice = config_temp.indice_idioma_selecionado;
    return true;
}

bool Gerenciador_Config_Get_Dados_Grao(uint8_t indice, Config_Grao_t* dados_grao)
{
    if (indice >= MAX_GRAOS || dados_grao == NULL) return false;
    uint16_t endereco = ADDR_CONFIG_PRIMARY + offsetof(Config_Aplicacao_t, graos) + (indice * sizeof(Config_Grao_t));
    return EEPROM_Driver_Read(endereco, (uint8_t*)dados_grao, sizeof(Config_Grao_t));
}

bool Gerenciador_Config_Get_Senha(char* buffer, uint8_t tamanho_buffer)
{
    if (buffer == NULL || tamanho_buffer == 0) return false;
    uint8_t tamanho_leitura = (tamanho_buffer > MAX_SENHA_LEN + 1) ? (MAX_SENHA_LEN + 1) : tamanho_buffer;
    uint16_t endereco = ADDR_CONFIG_PRIMARY + offsetof(Config_Aplicacao_t, senha_sistema);
    return EEPROM_Driver_Read(endereco, (uint8_t*)buffer, tamanho_leitura);
}

uint8_t Gerenciador_Config_Get_Num_Graos(void) { return MAX_GRAOS; }

bool Gerenciador_Config_Verificar_Bloco(uint16_t address, Config_Aplicacao_t* config_out)
{
    if (s_crc_handle == NULL || config_out == NULL) return false;
    return Tentar_Carregar_De_Endereco(address, config_out);
}

static void Recalcular_E_Atualizar_CRC(Config_Aplicacao_t* config)
{
    if (s_crc_handle == NULL || config == NULL) return;
    uint32_t tamanho_dados_crc = sizeof(Config_Aplicacao_t) - sizeof(uint32_t);
    config->crc = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)config, tamanho_dados_crc / 4);
}

static bool Salvar_Configuracao_Completa(const Config_Aplicacao_t* config)
{
    const uint16_t addresses[] = { ADDR_CONFIG_PRIMARY, ADDR_CONFIG_BACKUP1, ADDR_CONFIG_BACKUP2 };
    const char* names[] = { "Primario", "Backup 1", "Backup 2" };

    for (int i = 0; i < 3; i++)
    {
        if (!EEPROM_Driver_Write(addresses[i], (const uint8_t*)config, sizeof(Config_Aplicacao_t)))
        {
            printf("EEPROM Manager: Falha na operacao de escrita no bloco %s.\n", names[i]);
            return false;
        }

        uint32_t start_tick = HAL_GetTick();
        while (!EEPROM_Driver_IsReady())
        {
            if (HAL_GetTick() - start_tick > 100)
            {
                printf("EEPROM Manager: Timeout esperando EEPROM no bloco %s.\n", names[i]);
                return false;
            }
        }
    }

    printf("EEPROM Manager: Presets salvos com sucesso nas tres localizacoes.\n");
    return true;
}

static bool Tentar_Carregar_De_Endereco(uint16_t address, Config_Aplicacao_t* config)
{
    if (!EEPROM_Driver_Read(address, (uint8_t*)config, sizeof(Config_Aplicacao_t))) { return false; }
    uint32_t crc_armazenado = config->crc;
    Recalcular_E_Atualizar_CRC(config);
    uint32_t crc_calculado = config->crc;
    return (crc_calculado == crc_armazenado);
}

static bool Carregar_Primeira_Config_Valida(Config_Aplicacao_t* config)
{
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_PRIMARY, config)) return true;
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP1, config)) return true;
    if (Tentar_Carregar_De_Endereco(ADDR_CONFIG_BACKUP2, config)) return true;
    
    printf("EEPROM Manager: ERRO CRITICO! Nenhum bloco valido encontrado.\n");
    return false;
}

bool Gerenciador_Config_Set_Grao_Ativo(uint8_t novo_indice)
{
    if (s_crc_handle == NULL || novo_indice >= MAX_GRAOS) return false;
    static Config_Aplicacao_t config_temp;
    if (!Carregar_Primeira_Config_Valida(&config_temp)) return false;
    config_temp.indice_grao_ativo = novo_indice;
    Recalcular_E_Atualizar_CRC(&config_temp);
    return Salvar_Configuracao_Completa(&config_temp);
}

bool Gerenciador_Config_Get_Grao_Ativo(uint8_t* indice_ativo)
{
    if (s_crc_handle == NULL || indice_ativo == NULL) return false;
    
    static Config_Aplicacao_t config_temp;
    if (!Carregar_Primeira_Config_Valida(&config_temp))
    {
        *indice_ativo = 0; 
        return false;
    }

    if (config_temp.indice_grao_ativo < MAX_GRAOS) {
        *indice_ativo = config_temp.indice_grao_ativo;
    } else {
        *indice_ativo = 0;
    }
    
    return true;
}

bool Gerenciador_Config_Get_Cal_A(float* gain, float* zero)
{
    if (gain == NULL || zero == NULL) return false;
    Config_Aplicacao_t config_temp;
    if (!Carregar_Primeira_Config_Valida(&config_temp)) return false;
    
    *gain = config_temp.fat_cal_a_gain;
    *zero = config_temp.fat_cal_a_zero;
    return true;
}

bool Gerenciador_Config_Set_Cal_A(float gain, float zero)
{
    if (s_crc_handle == NULL) return false;
    Config_Aplicacao_t config_temp;
    if (!Carregar_Primeira_Config_Valida(&config_temp)) return false;

    config_temp.fat_cal_a_gain = gain;
    config_temp.fat_cal_a_zero = zero;

    Recalcular_E_Atualizar_CRC(&config_temp);
    return Salvar_Configuracao_Completa(&config_temp);
}