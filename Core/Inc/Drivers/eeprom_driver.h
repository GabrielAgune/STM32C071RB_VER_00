#ifndef EEPROM_DRIVER_H
#define EEPROM_DRIVER_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#define EEPROM_PAGE_SIZE        32

/**
 * @brief Initializes the EEPROM driver with the I2C handle.
 * @param hi2c Pointer to the I2C handle.
 */
void EEPROM_Driver_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief Checks if the EEPROM device is ready on the I2C bus.
 * @return true if the device acknowledges, false otherwise.
 */
bool EEPROM_Driver_IsReady(void);

/**
 * @brief Writes a block of data to the EEPROM.
 * @param addr The starting memory address to write to.
 * @param data Pointer to the data buffer to write.
 * @param size The number of bytes to write.
 * @return true on success, false on failure.
 */
bool EEPROM_Driver_Write(uint16_t addr, const uint8_t *data, uint16_t size);

/**
 * @brief Reads a block of data from the EEPROM.
 * @param addr The starting memory address to read from.
 * @param data Pointer to the buffer to store the read data.
 * @param size The number of bytes to read.
 * @return true on success, false on failure.
 */
bool EEPROM_Driver_Read(uint16_t addr, uint8_t *data, uint16_t size);

#endif // EEPROM_DRIVER_H
