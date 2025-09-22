/*******************************************************************************
 * @file        gerenciador_configuracoes.c
 * @brief       Configuration Manager Coordinator (Refactored V8.3)
 * @version     8.3 (Refactored for better modularity)
 * @details     Thin coordinator layer that orchestrates config cache and storage.
 *              Core functionality moved to config_cache.c and config_storage.c.
 ******************************************************************************/

#include "gerenciador_configuracoes.h"
#include "config_cache.h"
#include "config_storage.h"
#include <stdio.h>

//================================================================================
// Public Functions
//================================================================================

void Gerenciador_Config_Init(CRC_HandleTypeDef* hcrc)
{
    Config_Storage_Init(hcrc);
    Config_Cache_Init(); // Initialize with default values
}

bool Gerenciador_Config_Validar_e_Restaurar(void)
{
    Config_Aplicacao_t loaded_config;
    
    if (Config_Storage_Load(&loaded_config)) {
        // Successfully loaded valid configuration
        Config_Cache_Load(&loaded_config);
        return true;
    } else {
        // All copies corrupted - cache already has defaults
        printf("EEPROM Manager: FATAL ERROR! All copies corrupted. Loading Factory defaults.\r\n");
        // Trigger save of default values to EEPROM
        Config_Aplicacao_t default_config;
        Config_Cache_Get(&default_config);
        Config_Storage_SaveAsync(&default_config);
        return false;
    }
}

bool Gerenciador_Config_Forcar_Restauracao_Padrao(void)
{
    Config_Cache_Init(); // Reload defaults
    Config_Aplicacao_t default_config;
    Config_Cache_Get(&default_config);
    Config_Storage_SaveAsync(&default_config);
    return true;
}

bool Gerenciador_Config_Verificar_Bloco(uint16_t address, Config_Aplicacao_t* config_out)
{
    return Config_Storage_ValidateBlock(address, config_out);
}

void Gerenciador_Config_Run_FSM(void)
{
    Config_Storage_RunFSM();
}

uint8_t Gerenciador_Config_Get_Num_Graos(void) 
{ 
    return MAX_GRAOS; 
}

//================================================================================
// Wrapper Functions for Cache Operations
//================================================================================

bool Gerenciador_Config_Set_Indice_Idioma(uint8_t novo_indice)
{
    return Config_Cache_SetLanguage(novo_indice);
}

bool Gerenciador_Config_Get_Indice_Idioma(uint8_t* indice)
{
    return Config_Cache_GetLanguage(indice);
}

bool Gerenciador_Config_Set_Senha(const char* nova_senha)
{
    return Config_Cache_SetPassword(nova_senha);
}

bool Gerenciador_Config_Get_Senha(char* buffer, uint8_t tamanho_buffer)
{
    return Config_Cache_GetPassword(buffer, tamanho_buffer);
}

bool Gerenciador_Config_Set_Grao_Ativo(uint8_t novo_indice)
{
    return Config_Cache_SetActiveGrain(novo_indice);
}

bool Gerenciador_Config_Get_Grao_Ativo(uint8_t* indice_ativo)
{
    return Config_Cache_GetActiveGrain(indice_ativo);
}

bool Gerenciador_Config_Set_Cal_A(float gain, float zero)
{
    return Config_Cache_SetCalibration(gain, zero);
}

bool Gerenciador_Config_Get_Cal_A(float* gain, float* zero)
{
    return Config_Cache_GetCalibration(gain, zero);
}

bool Gerenciador_Config_Get_Dados_Grao(uint8_t indice, Config_Grao_t* dados_grao)
{
    return Config_Cache_GetGrainData(indice, dados_grao);
}