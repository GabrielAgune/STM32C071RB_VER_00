/*******************************************************************************
 * @file        cli_commands.c
 * @brief       CLI Command Processing Module
 * @version     8.3 (Extracted from cli_driver.c during refactoring)
 * @details     Handles all CLI command parsing and execution logic.
 *              Separated from transport layer for better modularity.
 ******************************************************************************/

#include "cli_commands.h"
#include "dwin_driver.h"
#include "app_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

//================================================================================
// Definitions
//================================================================================
#define CLI_RX_BUFFER_SIZE      128  // Needed for raw buffer sizing

//================================================================================
// Private Function Prototypes
//================================================================================
static void Cmd_Help(char* args);
static void Cmd_GetPeso(char* args);
static void Cmd_GetTemp(char* args);
static void Cmd_GetFreq(char* args);
static void Cmd_Dwin(char* args);
static void Handle_Dwin_PIC(char* sub_args);
static void Handle_Dwin_INT(char* sub_args);
static void Handle_Dwin_INT32(char* sub_args);
static void Handle_Dwin_RAW(char* sub_args);
static uint8_t hex_char_to_value(char c);

//================================================================================
// Command Tables
//================================================================================
static const cli_command_t s_command_table[] = {
    { "HELP", Cmd_Help }, 
    { "?", Cmd_Help }, 
    { "DWIN", Cmd_Dwin },
    { "PESO", Cmd_GetPeso }, 
    { "TEMP", Cmd_GetTemp }, 
    { "FREQ", Cmd_GetFreq },
};
static const size_t NUM_COMMANDS = sizeof(s_command_table) / sizeof(s_command_table[0]);

static const dwin_subcommand_t s_dwin_table[] = {
    { "PIC", Handle_Dwin_PIC }, 
    { "INT", Handle_Dwin_INT },
    { "INT32", Handle_Dwin_INT32 }, 
    { "RAW", Handle_Dwin_RAW }
};
static const size_t NUM_DWIN_SUBCOMMANDS = sizeof(s_dwin_table) / sizeof(s_dwin_table[0]);

static const char HELP_TEXT[] =
    "========================== CLI de Diagnostico (V8.3) ======================|\r\n"
    "| HELP ou ?                | Mostra esta ajuda.                            |\r\n"
    "| PESO                     | Mostra a leitura atual da balanca.            |\r\n"
    "| TEMP                     | Mostra a leitura do sensor de temperatura.    |\r\n"
    "| FREQ                     | Mostra a ultima leitura de frequencia.        |\r\n"
    "| DWIN PIC <id>            | Muda a tela (ex: DWIN PIC 1).                 |\r\n"
    "| DWIN INT <addr_h> <val>  | Escreve int16 no VP (ex: DWIN INT 2190 1234).  |\r\n"
    "| DWIN RAW <bytes_hex>     | Envia bytes crus para o DWIN (ex: 5AA5...).   |\r\n"
    "===========================================================================|\r\n";

//================================================================================
// Public Functions
//================================================================================

void CLI_Commands_Process(char* command_buffer)
{
    if (command_buffer == NULL || strlen(command_buffer) == 0) {
        return;
    }

    // Parse command and arguments
    char* args = strchr(command_buffer, ' ');
    if (args != NULL) {
        *args = '\0';
        args++;
        while (isspace((unsigned char)*args)) args++;
        if (*args == '\0') args = NULL;
    }

    // Find and execute command
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcasecmp(command_buffer, s_command_table[i].name) == 0) {
            s_command_table[i].handler(args);
            return;
        }
    }
    
    printf("Comando desconhecido: \"%s\". Digite 'HELP' para ajuda.\r\n", command_buffer);
}

const cli_command_t* CLI_Commands_GetTable(void)
{
    return s_command_table;
}

size_t CLI_Commands_GetCount(void)
{
    return NUM_COMMANDS;
}

//================================================================================
// Private Command Implementations
//================================================================================

static void Cmd_Help(char* args) 
{ 
    printf("%s", HELP_TEXT); 
}

static void Cmd_GetPeso(char* args) 
{
    App_ScaleData_t data;
    App_Manager_GetScaleData(&data);
    
    printf("Dados da Balanca:\r\n");
    printf("  - Peso: %.2f g\r\n", data.grams_display);
    printf("  - Estavel: %s\r\n", data.is_stable ? "SIM" : "NAO");
    printf("  - ADC Counts (mediana): %.0f\r\n", data.raw_counts_median);
}

static void Cmd_GetTemp(char* args) 
{
    float temperatura = App_Manager_GetTemperature();
    printf("Temperatura interna do MCU: %.2f C\r\n", temperatura);
}

static void Cmd_GetFreq(char* args) 
{
    FreqData_t data;
    App_Manager_GetFreqData(&data);
    printf("Dados de Frequencia:\r\n");
    printf("  - Pulsos (em 1s): %lu\r\n", (unsigned long)data.pulsos);
    printf("  - Escala A (calc): %.2f\r\n", data.escala_a);
}

static void Cmd_Dwin(char* args) 
{
    if (args == NULL) { 
        printf("Subcomando DWIN faltando. Use 'HELP'.\r\n"); 
        return; 
    }
    
    char* sub_cmd = args;
    char* sub_args = strchr(sub_cmd, ' ');
    if (sub_args != NULL) { 
        *sub_args = '\0'; 
        sub_args++; 
        while (isspace((unsigned char)*sub_args)) sub_args++; 
        if (*sub_args == '\0') sub_args = NULL; 
    }
    
    for (size_t i = 0; i < NUM_DWIN_SUBCOMMANDS; i++) {
        if (strcasecmp(sub_cmd, s_dwin_table[i].name) == 0) {
            s_dwin_table[i].handler(sub_args);
            return;
        }
    }
    printf("Subcomando DWIN desconhecido: \"%s\"\r\n", sub_cmd);
}

//================================================================================
// DWIN Subcommand Implementations
//================================================================================

static void Handle_Dwin_PIC(char* sub_args) 
{
    if (sub_args == NULL) { 
        printf("Uso: DWIN PIC <id>\r\n"); 
        return; 
    }
    DWIN_Driver_SetScreen(atoi(sub_args));
    printf("Comando DWIN PIC enfileirado.\r\n");
}

static void Handle_Dwin_INT(char* sub_args) 
{
    if (sub_args == NULL) { 
        printf("Uso: DWIN INT <addr_hex> <valor>\r\n"); 
        return; 
    }
    
    char* addr_str = sub_args;
    char* val_str = strchr(addr_str, ' ');
    if (val_str == NULL) { 
        printf("Valor faltando.\r\n"); 
        return; 
    }
    *val_str = '\0'; 
    val_str++;
    
    uint16_t vp = strtol(addr_str, NULL, 16);
    int16_t val = atoi(val_str);
    DWIN_Driver_WriteInt(vp, val);
    printf("Enfileirado (int16) %d em 0x%04X\r\n", val, vp);
}

static void Handle_Dwin_INT32(char* sub_args) 
{
    if (sub_args == NULL) { 
        printf("Uso: DWIN INT32 <addr_hex> <valor>\r\n"); 
        return; 
    }
    
    char* addr_str = sub_args;
    char* val_str = strchr(addr_str, ' ');
    if (val_str == NULL) { 
        printf("Valor faltando.\r\n"); 
        return; 
    }
    *val_str = '\0'; 
    val_str++;
    
    uint16_t vp = strtol(addr_str, NULL, 16);
    int32_t val = atol(val_str);
    DWIN_Driver_WriteInt32(vp, val);
    printf("Enfileirado (int32) %ld em 0x%04X\r\n", (long)val, vp);
}

static void Handle_Dwin_RAW(char* sub_args) 
{
    if (sub_args == NULL) { 
        printf("Uso: DWIN RAW <byte_hex> ...\r\n"); 
        return; 
    }
    
    uint8_t raw_buffer[CLI_RX_BUFFER_SIZE / 2];
    int byte_count = 0;
    char* ptr = sub_args;
    
    while (*ptr != '\0' && byte_count < (CLI_RX_BUFFER_SIZE / 2)) {
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') break;
        
        char high_c = *ptr++;
        if (*ptr == '\0' || isspace((unsigned char)*ptr)) { 
            printf("\nErro: Numero impar de caracteres hex.\r\n"); 
            return; 
        }
        char low_c = *ptr++;
        
        uint8_t high_v = hex_char_to_value(high_c);
        uint8_t low_v = hex_char_to_value(low_c);
        if (high_v == 0xFF || low_v == 0xFF) { 
            printf("\nErro: Caractere invalido na string hex.\r\n"); 
            return; 
        }
        raw_buffer[byte_count++] = (high_v << 4) | low_v;
    }
    
    printf("Enfileirando %d bytes para DWIN:", byte_count);
    for(int i = 0; i < byte_count; i++) {
        printf(" %02X", raw_buffer[i]);
    }
    printf("\r\n");
    
    DWIN_Driver_WriteRawBytes(raw_buffer, byte_count);
}

//================================================================================
// Utility Functions
//================================================================================

static uint8_t hex_char_to_value(char c) 
{
    c = toupper((unsigned char)c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0xFF; 
}