#ifndef CONFIG_STORAGE_H
#define CONFIG_STORAGE_H

#include "gerenciador_configuracoes.h"
#include "stm32c0xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @file config_storage.h
 * @brief Configuration Storage Management
 * @details Handles EEPROM storage operations with 3-copy redundancy and CRC validation
 */

/**
 * @brief Initialize the configuration storage system
 * @param hcrc Pointer to CRC handle for validation
 */
void Config_Storage_Init(CRC_HandleTypeDef* hcrc);

/**
 * @brief Run the storage FSM (called from main loop)
 * @details Handles asynchronous EEPROM writes
 */
void Config_Storage_RunFSM(void);

/**
 * @brief Load and validate configuration from EEPROM
 * @param config_out Pointer to configuration structure to fill
 * @return true if valid configuration loaded, false otherwise
 */
bool Config_Storage_Load(Config_Aplicacao_t* config_out);

/**
 * @brief Request asynchronous save of configuration
 * @param config Pointer to configuration data to save
 * @details Sets dirty flag for FSM to process
 */
void Config_Storage_SaveAsync(const Config_Aplicacao_t* config);

/**
 * @brief Check if storage system is currently busy
 * @return true if save operation in progress
 */
bool Config_Storage_IsBusy(void);

/**
 * @brief Validate configuration block at specific address
 * @param address EEPROM address to check
 * @param config_out Buffer to read configuration into
 * @return true if configuration is valid
 */
bool Config_Storage_ValidateBlock(uint16_t address, Config_Aplicacao_t* config_out);

#endif // CONFIG_STORAGE_H