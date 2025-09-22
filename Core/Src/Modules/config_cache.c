/*******************************************************************************
 * @file        config_cache.c
 * @brief       Configuration Cache Management
 * @version     8.3 (Extracted from gerenciador_configuracoes.c during refactoring)
 * @details     Manages in-RAM configuration cache for fast access
 ******************************************************************************/

#include "config_cache.h"
#include "config_storage.h"
#include "GXXX_Equacoes.h"
#include <string.h>
#include <stdio.h>

//================================================================================
// Static Variables
//================================================================================
static Config_Aplicacao_t s_config_cache;

//================================================================================
// Public Functions
//================================================================================

void Config_Cache_Init(void)
{
    memset(&s_config_cache, 0, sizeof(Config_Aplicacao_t));

    s_config_cache.versao_struct = 1;
    s_config_cache.indice_idioma_selecionado = 0;
    strncpy(s_config_cache.senha_sistema, "senha", MAX_SENHA_LEN);
    s_config_cache.senha_sistema[MAX_SENHA_LEN] = '\0';
    s_config_cache.fat_cal_a_gain = 1.0f;
    s_config_cache.fat_cal_a_zero = 0.0f;
		
    for (int i = 0; i < MAX_GRAOS; i++) {
        strncpy(s_config_cache.graos[i].nome, Produto[i].Nome[0], MAX_NOME_GRAO_LEN);
        s_config_cache.graos[i].nome[MAX_NOME_GRAO_LEN] = '\0';
        strncpy(s_config_cache.graos[i].validade, "22/06/2028", MAX_VALIDADE_LEN);
        s_config_cache.graos[i].validade[MAX_VALIDADE_LEN] = '\0';
        s_config_cache.graos[i].id_curva = Produto[i].Nr_Equa;
        s_config_cache.graos[i].umidade_min = Produto[i].Um_Min;
        s_config_cache.graos[i].umidade_max = Produto[i].Um_Max;
    }
}

void Config_Cache_Load(const Config_Aplicacao_t* config)
{
    if (config == NULL) return;
    memcpy(&s_config_cache, config, sizeof(Config_Aplicacao_t));
}

void Config_Cache_Get(Config_Aplicacao_t* config_out)
{
    if (config_out == NULL) return;
    memcpy(config_out, &s_config_cache, sizeof(Config_Aplicacao_t));
}

void Config_Cache_Update(const Config_Aplicacao_t* config)
{
    if (config == NULL) return;
    
    memcpy(&s_config_cache, config, sizeof(Config_Aplicacao_t));
    Config_Storage_SaveAsync(&s_config_cache);
}

bool Config_Cache_SetLanguage(uint8_t indice)
{
    if (Config_Storage_IsBusy()) return false; // Reject if already saving
    
    s_config_cache.indice_idioma_selecionado = indice;
    Config_Storage_SaveAsync(&s_config_cache);
    return true;
}

bool Config_Cache_GetLanguage(uint8_t* indice)
{
    if (indice == NULL) return false;
    *indice = s_config_cache.indice_idioma_selecionado;
    return true;
}

bool Config_Cache_SetPassword(const char* nova_senha)
{
    if (nova_senha == NULL) return false;
    if (Config_Storage_IsBusy()) return false; // Reject if already saving
    
    strncpy(s_config_cache.senha_sistema, nova_senha, MAX_SENHA_LEN);
    s_config_cache.senha_sistema[MAX_SENHA_LEN] = '\0';
    Config_Storage_SaveAsync(&s_config_cache);
    return true;
}

bool Config_Cache_GetPassword(char* buffer, uint8_t tamanho_buffer)
{
    if (buffer == NULL || tamanho_buffer == 0) return false;
    
    strncpy(buffer, s_config_cache.senha_sistema, tamanho_buffer - 1);
    buffer[tamanho_buffer - 1] = '\0'; // Ensure null termination
    return true;
}

bool Config_Cache_SetActiveGrain(uint8_t novo_indice)
{
    if (novo_indice >= MAX_GRAOS) return false;
    if (Config_Storage_IsBusy()) return false; 

    s_config_cache.indice_grao_ativo = novo_indice;
    Config_Storage_SaveAsync(&s_config_cache);
    return true;
}

bool Config_Cache_GetActiveGrain(uint8_t* indice_ativo)
{
    if (indice_ativo == NULL) return false;
    
    if (s_config_cache.indice_grao_ativo < MAX_GRAOS) {
        *indice_ativo = s_config_cache.indice_grao_ativo;
    } else {
        *indice_ativo = 0; // Sanity check
    }
    return true;
}

bool Config_Cache_SetCalibration(float gain, float zero)
{
    if (Config_Storage_IsBusy()) return false; 
    
    s_config_cache.fat_cal_a_gain = gain;
    s_config_cache.fat_cal_a_zero = zero;
    Config_Storage_SaveAsync(&s_config_cache);
    return true;
}

bool Config_Cache_GetCalibration(float* gain, float* zero)
{
    if (gain == NULL || zero == NULL) return false;
    *gain = s_config_cache.fat_cal_a_gain;
    *zero = s_config_cache.fat_cal_a_zero;
    return true;
}

bool Config_Cache_GetGrainData(uint8_t indice, Config_Grao_t* dados_grao)
{
    if (indice >= MAX_GRAOS || dados_grao == NULL) return false;
    
    // Read directly from RAM cache, not from EEPROM
    memcpy(dados_grao, &s_config_cache.graos[indice], sizeof(Config_Grao_t));
    return true;
}