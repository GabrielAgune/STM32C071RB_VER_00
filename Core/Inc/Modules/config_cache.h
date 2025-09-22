#ifndef CONFIG_CACHE_H
#define CONFIG_CACHE_H

#include "gerenciador_configuracoes.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @file config_cache.h  
 * @brief Configuration Cache Management
 * @details Manages in-RAM configuration cache for fast access
 */

/**
 * @brief Initialize the configuration cache with default values
 */
void Config_Cache_Init(void);

/**
 * @brief Load configuration into cache
 * @param config Configuration data to cache
 */
void Config_Cache_Load(const Config_Aplicacao_t* config);

/**
 * @brief Get a copy of the current cached configuration
 * @param config_out Buffer to copy configuration to
 */
void Config_Cache_Get(Config_Aplicacao_t* config_out);

/**
 * @brief Update the cache and trigger async save
 * @param config New configuration data
 */
void Config_Cache_Update(const Config_Aplicacao_t* config);

/**
 * @brief Set language index in cache
 * @param indice Language index
 * @return true if successful
 */
bool Config_Cache_SetLanguage(uint8_t indice);

/**
 * @brief Get language index from cache
 * @param indice Pointer to store language index
 * @return true if successful
 */
bool Config_Cache_GetLanguage(uint8_t* indice);

/**
 * @brief Set system password in cache
 * @param nova_senha New password string
 * @return true if successful
 */
bool Config_Cache_SetPassword(const char* nova_senha);

/**
 * @brief Get system password from cache
 * @param buffer Buffer to store password
 * @param tamanho_buffer Buffer size
 * @return true if successful
 */
bool Config_Cache_GetPassword(char* buffer, uint8_t tamanho_buffer);

/**
 * @brief Set active grain index in cache
 * @param novo_indice Grain index
 * @return true if successful
 */
bool Config_Cache_SetActiveGrain(uint8_t novo_indice);

/**
 * @brief Get active grain index from cache
 * @param indice_ativo Pointer to store grain index
 * @return true if successful
 */
bool Config_Cache_GetActiveGrain(uint8_t* indice_ativo);

/**
 * @brief Set calibration factors in cache
 * @param gain Calibration gain
 * @param zero Calibration zero offset
 * @return true if successful
 */
bool Config_Cache_SetCalibration(float gain, float zero);

/**
 * @brief Get calibration factors from cache
 * @param gain Pointer to store gain value
 * @param zero Pointer to store zero offset
 * @return true if successful
 */
bool Config_Cache_GetCalibration(float* gain, float* zero);

/**
 * @brief Get grain data from cache
 * @param indice Grain index
 * @param dados_grao Pointer to store grain data
 * @return true if successful
 */
bool Config_Cache_GetGrainData(uint8_t indice, Config_Grao_t* dados_grao);

#endif // CONFIG_CACHE_H