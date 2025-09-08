/*******************************************************************************
 * @file        eeprom_driver.c
 * @brief       Driver para EEPROM I2C (interface gen�rica).
 * @details     Este driver fornece fun��es para inicializar, verificar o estado,
 * ler e escrever em uma mem�ria EEPROM externa via I2C.
 * Ele gerencia a escrita em p�ginas para otimizar a grava��o.
 ******************************************************************************/

#include "eeprom_driver.h"
#include <stddef.h>

//==============================================================================
// Defini��es de Hardware da EEPROM
//==============================================================================

#define EEPROM_I2C_ADDR         (0x50 << 1)   // Endere�o I2C da EEPROM para alinhar com formato da HAL 
#define EEPROM_I2C_TIMEOUT      100           // Timeout padr�o para opera��es I2C.

//==============================================================================
// Vari�veis Est�ticas (File-Scope)
//==============================================================================

static I2C_HandleTypeDef *s_i2c_handle = NULL;

//==============================================================================
// Fun��es P�blicas
//==============================================================================


// Inicializa o driver da EEPROM com um handle I2C.
void EEPROM_Driver_Init(I2C_HandleTypeDef *hi2c)
{
    s_i2c_handle = hi2c;
}

// Verifica se a EEPROM est� presente e pronta para comunicar.
bool EEPROM_Driver_IsReady(void)
{
    if (s_i2c_handle == NULL)
    {
        return false;
    }
		
    return (HAL_I2C_IsDeviceReady(s_i2c_handle, EEPROM_I2C_ADDR, 1, EEPROM_I2C_TIMEOUT) == HAL_OK);
}

// Escreve um bloco de dados na EEPROM.
bool EEPROM_Driver_Write(uint16_t addr, const uint8_t *data, uint16_t size)
{
    if (s_i2c_handle == NULL || data == NULL)
    {
        return false;
    }

    uint16_t bytes_written = 0;

    while (bytes_written < size)
    {
        // Calcula quantos bytes podem ser escritos na p�gina atual da EEPROM
        uint16_t chunk_size = EEPROM_PAGE_SIZE - (addr % EEPROM_PAGE_SIZE);

        // Garante que o tamanho do bloco n�o exceda os dados restantes a serem escritos
        if (chunk_size > (size - bytes_written))
        {
            chunk_size = size - bytes_written;
        }

        // Escreve o bloco de dados na mem�ria da EEPROM
        if (HAL_I2C_Mem_Write(s_i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)(data + bytes_written), chunk_size, EEPROM_I2C_TIMEOUT) != HAL_OK)
        {
            return false; // Falha na escrita
        }

        HAL_Delay(5);

        addr += chunk_size;
        bytes_written += chunk_size;
    }

    return true;
}

// L� um bloco de dados da EEPROM.
bool EEPROM_Driver_Read(uint16_t addr, uint8_t *data, uint16_t size)
{
    if (s_i2c_handle == NULL || data == NULL)
    {
        return false;
    }

    // L� um bloco cont�nuo de mem�ria da EEPROM.
    return (HAL_I2C_Mem_Read(s_i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, data, size, EEPROM_I2C_TIMEOUT) == HAL_OK);
}