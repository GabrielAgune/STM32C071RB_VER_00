/*******************************************************************************
 * @file        config_storage.c
 * @brief       Configuration Storage Management
 * @version     8.3 (Extracted from gerenciador_configuracoes.c during refactoring)
 * @details     Handles EEPROM storage operations with 3-copy redundancy and CRC validation
 ******************************************************************************/

#include "config_storage.h"
#include "eeprom_driver.h"
#include <stdio.h>
#include <string.h>

//================================================================================
// Definitions
//================================================================================
#define FSM_ERROR_COOLDOWN_MS 5000

//================================================================================
// Types
//================================================================================
typedef enum {
    FSM_STORE_IDLE,
    FSM_STORE_START_WRITE_PRIMARY,
    FSM_STORE_WAIT_WRITE_PRIMARY,
    FSM_STORE_START_WRITE_BKP1,
    FSM_STORE_WAIT_WRITE_BKP1,
    FSM_STORE_START_WRITE_BKP2,
    FSM_STORE_WAIT_WRITE_BKP2,
    FSM_STORE_ERROR
} StorageFsmState_t;

//================================================================================
// Static Variables
//================================================================================
static CRC_HandleTypeDef *s_crc_handle = NULL;
static Config_Aplicacao_t s_save_buffer;  // Copy for async save operations

static struct {
    StorageFsmState_t state;
    bool dirty;
    bool is_saving;
    uint32_t error_retry_tick;
} s_storage_fsm = { FSM_STORE_IDLE, false, false, 0 };

//================================================================================
// Private Function Prototypes
//================================================================================
static void RecalculateAndUpdateCRC(Config_Aplicacao_t* config);
static bool TryLoadFromAddress(uint16_t address, Config_Aplicacao_t* config);
static bool LoadFirstValidConfig(Config_Aplicacao_t* config_out);

//================================================================================
// Public Functions
//================================================================================

void Config_Storage_Init(CRC_HandleTypeDef* hcrc)
{
    s_crc_handle = hcrc;
    s_storage_fsm.state = FSM_STORE_IDLE;
    s_storage_fsm.dirty = false;
    s_storage_fsm.is_saving = false;
}

void Config_Storage_RunFSM(void)
{
    // FSM only runs if dirty flag is set AND we're not already in a save cycle
    if (s_storage_fsm.state == FSM_STORE_IDLE && s_storage_fsm.dirty) {
        if (EEPROM_Driver_IsBusy()) {
            return; // Wait for I2C/DMA driver to be free
        }

        // Check if we're in error cooldown
        if (HAL_GetTick() - s_storage_fsm.error_retry_tick < FSM_ERROR_COOLDOWN_MS) {
             return; // Not time to retry yet
        }

        // Mark as busy and clear dirty flag (start the process)
        s_storage_fsm.is_saving = true;
        s_storage_fsm.dirty = false; 

        // Recalculate CRC before starting write
        RecalculateAndUpdateCRC(&s_save_buffer); 

        printf("Storage FSM: Dirty flag detected. Starting async save of 3 copies...\r\n");
        s_storage_fsm.state = FSM_STORE_START_WRITE_PRIMARY;
    }

    if (!s_storage_fsm.is_saving) {
        return; // Nothing to do
    }

    // Process async write FSM
    switch (s_storage_fsm.state) {
        case FSM_STORE_START_WRITE_PRIMARY:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_PRIMARY, (const uint8_t*)&s_save_buffer, sizeof(Config_Aplicacao_t))) {
                printf("Storage FSM: Failed to START primary write!\r\n");
                s_storage_fsm.state = FSM_STORE_ERROR;
            } else {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_PRIMARY;
            }
            break;

        case FSM_STORE_WAIT_WRITE_PRIMARY:
            if (EEPROM_Driver_Write_Async_Poll()) {
                if (EEPROM_Driver_GetAndClearErrorFlag()) {
                    printf("Storage FSM: Driver error writing Primary Block.\r\n");
                    s_storage_fsm.state = FSM_STORE_ERROR;
                } else {
                    printf("Storage FSM: Primary Block OK.\r\n");
                    s_storage_fsm.state = FSM_STORE_START_WRITE_BKP1;
                }
            }
            break;

        case FSM_STORE_START_WRITE_BKP1:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_BACKUP1, (const uint8_t*)&s_save_buffer, sizeof(Config_Aplicacao_t))) {
                printf("Storage FSM: Failed to START BKP1 write!\r\n");
                s_storage_fsm.state = FSM_STORE_ERROR;
            } else {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_BKP1;
            }
            break;

        case FSM_STORE_WAIT_WRITE_BKP1:
            if (EEPROM_Driver_Write_Async_Poll()) {
                if (EEPROM_Driver_GetAndClearErrorFlag()) {
                    printf("Storage FSM: Driver error writing BKP1 Block.\r\n");
                    s_storage_fsm.state = FSM_STORE_ERROR;
                } else {
                    printf("Storage FSM: BKP1 Block OK.\r\n");
                    s_storage_fsm.state = FSM_STORE_START_WRITE_BKP2;
                }
            }
            break;

        case FSM_STORE_START_WRITE_BKP2:
            if (!EEPROM_Driver_Write_Async_Start(ADDR_CONFIG_BACKUP2, (const uint8_t*)&s_save_buffer, sizeof(Config_Aplicacao_t))) {
                printf("Storage FSM: Failed to START BKP2 write!\r\n");
                s_storage_fsm.state = FSM_STORE_ERROR;
            } else {
                s_storage_fsm.state = FSM_STORE_WAIT_WRITE_BKP2;
            }
            break;

        case FSM_STORE_WAIT_WRITE_BKP2:
            if (EEPROM_Driver_Write_Async_Poll()) {
                if (EEPROM_Driver_GetAndClearErrorFlag()) {
                    printf("Storage FSM: Driver error writing BKP2 Block.\r\n");
                    s_storage_fsm.state = FSM_STORE_ERROR;
                } else {
                    printf("Storage FSM: BKP2 Block OK. Save complete.\r\n");
                    s_storage_fsm.is_saving = false;
                    s_storage_fsm.state = FSM_STORE_IDLE;
                }
            }
            break;

        case FSM_STORE_IDLE:
        case FSM_STORE_ERROR:
        default:
            // Error state - stop FSM and retry later
            s_storage_fsm.is_saving = false; 
            s_storage_fsm.dirty = true; // Mark dirty again to retry save
            s_storage_fsm.state = FSM_STORE_IDLE;
            s_storage_fsm.error_retry_tick = HAL_GetTick(); // Activate cooldown timer
            printf("Storage FSM: ERROR DURING ASYNC WRITE! Retrying in %dms...\r\n", FSM_ERROR_COOLDOWN_MS);
            break;
    }
}

bool Config_Storage_Load(Config_Aplicacao_t* config_out)
{
    if (s_crc_handle == NULL || config_out == NULL) return false;

    printf("Config Storage: Checking data integrity...\r\n");

    if (TryLoadFromAddress(ADDR_CONFIG_PRIMARY, config_out)) {
        printf("Config Storage: Data integrity OK (Primary)!\r\n");
        return true; 
    }
    
    printf("Config Storage: Primary corrupted. Trying Backup 1...\r\n");
    if (TryLoadFromAddress(ADDR_CONFIG_BACKUP1, config_out)) {
        printf("Config Storage: Restored from Backup 1. Marking for re-save...\r\n");
        s_storage_fsm.dirty = true; // Mark to rewrite all slots
        return true;
    }
     
    printf("Config Storage: Backup 1 corrupted. Trying Backup 2...\r\n");
    if (TryLoadFromAddress(ADDR_CONFIG_BACKUP2, config_out)) {
        printf("Config Storage: Restored from Backup 2. Marking for re-save...\r\n");
        s_storage_fsm.dirty = true; // Mark to rewrite all slots
        return true;
    }

    printf("Config Storage: FATAL ERROR! All copies corrupted.\r\n");
    return false;
}

void Config_Storage_SaveAsync(const Config_Aplicacao_t* config)
{
    if (config == NULL) return;
    
    // Copy configuration to save buffer
    memcpy(&s_save_buffer, config, sizeof(Config_Aplicacao_t));
    
    // Mark for async save
    s_storage_fsm.dirty = true;
}

bool Config_Storage_IsBusy(void)
{
    return s_storage_fsm.is_saving;
}

bool Config_Storage_ValidateBlock(uint16_t address, Config_Aplicacao_t* config_out)
{
    return TryLoadFromAddress(address, config_out);
}

//================================================================================
// Private Functions
//================================================================================

static void RecalculateAndUpdateCRC(Config_Aplicacao_t* config)
{
    if (s_crc_handle == NULL || config == NULL) return;

    // Clear existing CRC
    config->crc = 0;
    
    // Calculate new CRC over the struct (excluding the CRC field itself)
    size_t data_size = sizeof(Config_Aplicacao_t) - sizeof(config->crc);
    config->crc = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)config, data_size / 4);
}

static bool TryLoadFromAddress(uint16_t address, Config_Aplicacao_t* config)
{
    Config_Aplicacao_t temp_config;
    
    // Blocking read from EEPROM
    if (!EEPROM_Driver_Read(address, (uint8_t*)&temp_config, sizeof(Config_Aplicacao_t))) {
        return false; // I2C communication error
    }
    
    // Store original CRC and clear field for validation
    uint32_t original_crc = temp_config.crc;
    temp_config.crc = 0;
    
    // Calculate CRC over data (excluding CRC field)
    size_t data_size = sizeof(Config_Aplicacao_t) - sizeof(temp_config.crc);
    uint32_t calculated_crc = HAL_CRC_Calculate(s_crc_handle, (uint32_t*)&temp_config, data_size / 4);
    
    // Restore original CRC and compare
    temp_config.crc = original_crc;
    
    if (calculated_crc == original_crc) {
        // CRC valid - copy to output
        if (config != NULL) {
            memcpy(config, &temp_config, sizeof(Config_Aplicacao_t));
        }
        return true;
    }
    
    return false; // CRC mismatch
}