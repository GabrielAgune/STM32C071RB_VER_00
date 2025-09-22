#ifndef CLI_COMMANDS_H
#define CLI_COMMANDS_H

#include <stdint.h>
#include <stddef.h>

/**
 * @file cli_commands.h
 * @brief CLI Command Processing Module
 * @details Handles all CLI command parsing and execution logic
 */

/**
 * @brief Structure for CLI command definitions
 */
typedef struct {
    const char* name;
    void (*handler)(char* args);
} cli_command_t;

/**
 * @brief Structure for DWIN subcommand definitions
 */
typedef struct {
    const char* name;
    void (*handler)(char* args);
} dwin_subcommand_t;

/**
 * @brief Process a complete command string
 * @param command_buffer Null-terminated command string to process
 */
void CLI_Commands_Process(char* command_buffer);

/**
 * @brief Get the command table
 * @return Pointer to the command table
 */
const cli_command_t* CLI_Commands_GetTable(void);

/**
 * @brief Get the number of commands in the table
 * @return Number of available commands
 */
size_t CLI_Commands_GetCount(void);

#endif // CLI_COMMANDS_H