/*******************************************************************************
 * @file        cli_driver.c
 * @brief       Driver CLI Não-Bloqueante (Arquitetura V8.2 - Refatorado)
 * @version     6.0 (Refatorado por Gemini com base em "Código Limpo")
 * @details     Usa SW FIFO (1K) + DMA Pump (Main Loop).
 * Aplicados princípios de "Código Limpo" para melhor clareza,
 * manutenibilidade e robustez.
 ******************************************************************************/

#include "cli_driver.h"
#include "dwin_driver.h"
#include "app_manager.h"
#include "medicao_handler.h"
#include "relato.h"
#include "rtc_driver.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

//================================================================================
// Definições e Constantes
//================================================================================
#define CLI_RX_BUFFER_SIZE 128
#define CLI_TX_FIFO_SIZE 1024
#define CLI_TX_DMA_BUFFER_SIZE 64

//================================================================================
// Tipos de Dados e Estruturas
//================================================================================
typedef void (*CommandHandler)(char* args);

typedef struct {
    const char* name;
    CommandHandler handler;
} CliCommand_t;

//================================================================================
// Protótipos de Funções Privadas
//================================================================================
static void ProcessReceivedCommand(void);
static void ShowPrompt(void);

// --- Handlers de Comando ---
static void Cmd_Help(char* args);
static void Cmd_Dwin(char* args);
static void Cmd_GetPeso(char* args);
static void Cmd_GetTemp(char* args);
static void Cmd_GetFreq(char* args);
static void Cmd_Service(char* args);
static void Cmd_SetTime(char* args);
static void Cmd_WhoAmI(char* args);
static void Cmd_SetDate(char* args);

// --- Handlers de Subcomando DWIN ---
static void Handle_Dwin_PIC(char* sub_args);
static void Handle_Dwin_INT(char* sub_args);
static void Handle_Dwin_INT32(char* sub_args);
static void Handle_Dwin_RAW(char* sub_args);

// --- Funções Auxiliares ---
static uint8_t HexCharToValue(char c);
static void TokenizeCommand(char* buffer, char** command, char** args);
static void FindAndExecuteCommand(const CliCommand_t* command_table, size_t table_size,
                                  const char* command_name, char* args, const char* unknown_msg);

//================================================================================
// Variáveis Estáticas
//================================================================================
static UART_HandleTypeDef* s_huart_debug = NULL;

// --- Buffers de Recepção (RX) ---
static uint8_t s_cli_rx_byte;
static char s_cli_rx_buffer[CLI_RX_BUFFER_SIZE];
static uint16_t s_cli_rx_index = 0;
static volatile bool s_is_command_ready = false;

// --- Buffers e Controle de Transmissão (TX) ---
static uint8_t s_cli_tx_fifo[CLI_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0;
static volatile uint16_t s_tx_fifo_tail = 0;
static uint8_t s_cli_tx_dma_buffer[CLI_TX_DMA_BUFFER_SIZE];
static volatile bool s_is_dma_tx_busy = false;

// --- Tabela de Comandos ---
static const CliCommand_t s_command_table[] = {
    {"HELP", Cmd_Help},    {"?", Cmd_Help},       {"DWIN", Cmd_Dwin},
    {"PESO", Cmd_GetPeso}, {"TEMP", Cmd_GetTemp}, {"FREQ", Cmd_GetFreq},
    {"SERVICE", Cmd_Service}, {"WHO_AM_I", Cmd_WhoAmI}, {"TIME", Cmd_SetTime},
    {"DATE", Cmd_SetDate},
};
static const size_t NUM_COMMANDS = sizeof(s_command_table) / sizeof(s_command_table[0]);

static const CliCommand_t s_dwin_subcommand_table[] = {
    {"PIC", Handle_Dwin_PIC},
    {"INT", Handle_Dwin_INT},
    {"INT32", Handle_Dwin_INT32},
    {"RAW", Handle_Dwin_RAW}
};
static const size_t NUM_DWIN_SUBCOMMANDS = sizeof(s_dwin_subcommand_table) / sizeof(s_dwin_subcommand_table[0]);

static const char HELP_TEXT[] =
    "============================= CLI de Diagnostico ==========================|\r\n"
    "| HELP ou ?                | Mostra esta ajuda.                            |\r\n"
    "| WHO_AM_I                 | Exibe especificacoes do sistema.              |\r\n"
    "| TIME HH:MM:SS            | Define a hora do sistema.                     |\r\n"
    "| DATE DD/MM/AA            | Define a data do sistema.                     |\r\n"
    "| PESO                     | Mostra a leitura atual da balanca.            |\r\n"
    "| TEMP                     | Mostra a leitura do sensor de temperatura.    |\r\n"
    "| FREQ                     | Mostra a ultima leitura de frequencia.        |\r\n"
    "| SERVICE                  | Entra na tela de servico.                     |\r\n"
    "| DWIN PIC <id>            | Muda a tela (ex: DWIN PIC 1).                 |\r\n"
    "| DWIN INT <addr_h> <val>  | Escreve int16 no VP (ex: DWIN INT 2190 1234). |\r\n"
    "| DWIN RAW <bytes_hex>     | Envia bytes crus para o DWIN (ex: 5AA5...).   |\r\n"
    "===========================================================================|\r\n";

//================================================================================
// Funções Públicas
//================================================================================

void CLI_Init(UART_HandleTypeDef* debug_huart) {
    s_huart_debug = debug_huart;
    if (HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1) != HAL_OK) {
        Error_Handler();
    }
    printf("\r\nCLI Pronta. Digite 'HELP' para comandos.\r\n");
    ShowPrompt();
}

void CLI_Process(void) {
    if (s_is_command_ready) {
        ProcessReceivedCommand();
        memset(s_cli_rx_buffer, 0, CLI_RX_BUFFER_SIZE);
        s_cli_rx_index = 0;
        s_is_command_ready = false;
        ShowPrompt();
    }
}

void CLI_TX_Pump(void) {
    if (s_is_dma_tx_busy || (s_tx_fifo_head == s_tx_fifo_tail)) {
        return;
    }

    // --- Seção Crítica ---
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);

    if (s_is_dma_tx_busy) { // Dupla verificação para segurança
        HAL_NVIC_EnableIRQ(USART1_IRQn);
        HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
        return;
    }
    s_is_dma_tx_busy = true;

    uint16_t bytes_to_send = 0;
    while ((s_tx_fifo_tail != s_tx_fifo_head) && (bytes_to_send < CLI_TX_DMA_BUFFER_SIZE)) {
        s_cli_tx_dma_buffer[bytes_to_send++] = s_cli_tx_fifo[s_tx_fifo_tail];
        s_tx_fifo_tail = (s_tx_fifo_tail + 1) % CLI_TX_FIFO_SIZE;
    }

    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    // --- Fim da Seção Crítica ---

    if (bytes_to_send > 0 && HAL_UART_Transmit_DMA(s_huart_debug, s_cli_tx_dma_buffer, bytes_to_send) != HAL_OK) {
        s_is_dma_tx_busy = false;
    }
}

//================================================================================
// Funções de Callback e de Interrupção
//================================================================================

void CLI_Printf_Transmit(uint8_t ch) {
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);

    if (ch == '\n') {
        uint16_t next_head = (s_tx_fifo_head + 1) % CLI_TX_FIFO_SIZE;
        if (next_head != s_tx_fifo_tail) {
            s_cli_tx_fifo[s_tx_fifo_head] = '\r';
            s_tx_fifo_head = next_head;
        }
    }

    uint16_t next_head = (s_tx_fifo_head + 1) % CLI_TX_FIFO_SIZE;
    if (next_head != s_tx_fifo_tail) {
        s_cli_tx_fifo[s_tx_fifo_head] = ch;
        s_tx_fifo_head = next_head;
    }

    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
}

void CLI_HandleRxCplt(UART_HandleTypeDef *huart) {
    if (s_is_command_ready) {
        return; // Ignora bytes enquanto o comando anterior não for processado
    }

    if (s_cli_rx_byte == '\r' || s_cli_rx_byte == '\n') {
        if (s_cli_rx_index > 0) {
            s_cli_rx_buffer[s_cli_rx_index] = '\0';
            s_is_command_ready = true;
            printf("\r\n");
        } else {
            ShowPrompt();
        }
    } else if (s_cli_rx_byte == '\b' || s_cli_rx_byte == 127) { // Backspace
        if (s_cli_rx_index > 0) {
            s_cli_rx_index--;
            CLI_Printf_Transmit('\b');
            CLI_Printf_Transmit(' ');
            CLI_Printf_Transmit('\b');
        }
    } else if (isprint(s_cli_rx_byte) && s_cli_rx_index < (CLI_RX_BUFFER_SIZE - 1)) {
        s_cli_rx_buffer[s_cli_rx_index++] = (char)s_cli_rx_byte;
        CLI_Printf_Transmit(s_cli_rx_byte); // Eco não bloqueante
    }

    if (HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1) != HAL_OK) {
        HAL_UART_AbortReceive_IT(s_huart_debug);
        HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1);
    }
}

void CLI_HandleTxCplt(UART_HandleTypeDef *huart) {
    s_is_dma_tx_busy = false;
}

void CLI_HandleError(UART_HandleTypeDef *huart) {
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE)) {
        (void)huart->Instance->RDR;
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
    }

    HAL_UART_AbortReceive_IT(s_huart_debug);
    HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1);
}

//================================================================================
// Implementação dos Handlers de Comando
//================================================================================

static void Cmd_Help(char* args) {
    printf("%s", HELP_TEXT);
}

static void Cmd_Service(char* args) {
    DWIN_Driver_SetScreen(TELA_SERVICO);
}

static void Cmd_GetPeso(char* args) {
    DadosMedicao_t dados_atuais;
    Medicao_Get_UltimaMedicao(&dados_atuais);
    printf("Dados da Balanca:\r\n  - Peso: %.2f g\r\n", dados_atuais.Peso);
}

static void Cmd_GetTemp(char* args) {
    float temperatura = TempSensor_GetTemperature();
    printf("Temperatura interna do MCU: %.2f C\r\n", temperatura);
}

static void Cmd_GetFreq(char* args) {
    DadosMedicao_t dados_atuais;
    Medicao_Get_UltimaMedicao(&dados_atuais);
    printf("Dados de Frequencia:\r\n");
    printf("  - Pulsos (em 1s): %.1f\r\n", dados_atuais.Frequencia);
    printf("  - Escala A (calc): %.2f\r\n", dados_atuais.Escala_A);
}

static void Cmd_Dwin(char* args) {
    if (args == NULL) {
        printf("Subcomando DWIN faltando. Use 'HELP'.");
        return;
    }
    char* sub_command;
    char* sub_args;
    TokenizeCommand(args, &sub_command, &sub_args);

    FindAndExecuteCommand(s_dwin_subcommand_table, NUM_DWIN_SUBCOMMANDS,
                          sub_command, sub_args, "Subcomando DWIN desconhecido");
}

static void Cmd_SetTime(char* args) {
    if (args == NULL) {
        printf("Erro: Faltam argumentos. Uso: TIME HH:MM:SS\r\n");
        return;
    }
    uint8_t h, m, s;
    if (sscanf(args, "%hhu:%hhu:%hhu", &h, &m, &s) == 3 && h < 24 && m < 60 && s < 60) {
        if (RTC_Driver_SetTime(h, m, s)) {
            printf("OK. RTC atualizado para %02u:%02u:%02u\r\n", h, m, s);
        } else {
            printf("Erro: Falha ao setar a hora no hardware do RTC.\r\n");
        }
    } else {
        printf("Erro: Formato ou valores invalidos. Uso: TIME HH(0-23):MM(0-59):SS(0-59).\r\n");
    }
}

static void Cmd_SetDate(char* args) {
    if (args == NULL) {
        printf("Erro: Faltam argumentos. Uso: DATE DD/MM/AA\r\n");
        return;
    }
    uint8_t d, m, a;
    if (sscanf(args, "%hhu/%hhu/%hhu", &d, &m, &a) == 3 && d >= 1 && d <= 31 && m >= 1 && m <= 12) {
        if (RTC_Driver_SetDate(d, m, a)) {
            printf("OK. RTC atualizado para %02u/%02u/%02u\r\n", d, m, a);
        } else {
            printf("Erro: Falha ao setar a data no hardware do RTC.\r\n");
        }
    } else {
        printf("Erro: Formato ou valores invalidos. Uso: DATE DD(1-31)/MM(1-12)/AA(00-99).\r\n");
    }
}

static void Cmd_WhoAmI(char* args) {
    Who_am_i();
}

//================================================================================
// Implementação dos Handlers de Subcomando DWIN
//================================================================================

static void Handle_Dwin_PIC(char* sub_args) {
    if (sub_args == NULL) {
        printf("Uso: DWIN PIC <id>");
        return;
    }
    DWIN_Driver_SetScreen(atoi(sub_args));
    printf("Comando DWIN PIC enfileirado.");
}

static void Handle_Dwin_INT(char* sub_args) {
    if (sub_args == NULL) {
        printf("Uso: DWIN INT <addr_hex> <valor>");
        return;
    }
    char* val_str;
    char* addr_str;
    TokenizeCommand(sub_args, &addr_str, &val_str);

    if (val_str == NULL) {
        printf("Valor faltando.");
        return;
    }
    uint16_t vp = strtol(addr_str, NULL, 16);
    int16_t val = atoi(val_str);
    DWIN_Driver_WriteInt(vp, val);
    printf("Enfileirado (int16) %d em 0x%04X", val, vp);
}

static void Handle_Dwin_INT32(char* sub_args) {
    if (sub_args == NULL) {
        printf("Uso: DWIN INT32 <addr_hex> <valor>");
        return;
    }
    char* val_str;
    char* addr_str;
    TokenizeCommand(sub_args, &addr_str, &val_str);

    if (val_str == NULL) {
        printf("Valor faltando.");
        return;
    }
    uint16_t vp = strtol(addr_str, NULL, 16);
    int32_t val = atol(val_str);
    DWIN_Driver_WriteInt32(vp, val);
    printf("Enfileirado (int32) %ld em 0x%04X", (long)val, vp);
}

static void Handle_Dwin_RAW(char* sub_args) {
    if (sub_args == NULL) {
        printf("Uso: DWIN RAW <byte_hex> ...");
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
            printf("\nErro: Numero impar de caracteres hex.");
            return;
        }
        char low_c = *ptr++;
        uint8_t high_v = HexCharToValue(high_c);
        uint8_t low_v = HexCharToValue(low_c);

        if (high_v == 0xFF || low_v == 0xFF) {
            printf("\nErro: Caractere invalido na string hex.");
            return;
        }
        raw_buffer[byte_count++] = (high_v << 4) | low_v;
    }

    printf("Enfileirando %d bytes para DWIN:", byte_count);
    for (int i = 0; i < byte_count; i++) {
        printf(" %02X", raw_buffer[i]);
    }
    DWIN_Driver_WriteRawBytes(raw_buffer, byte_count);
}

//================================================================================
// Funções Auxiliares e de Processamento
//================================================================================

static void ProcessReceivedCommand(void) {
    char* command;
    char* args;
    TokenizeCommand(s_cli_rx_buffer, &command, &args);

    if (*command == '\0') return;

    FindAndExecuteCommand(s_command_table, NUM_COMMANDS,
                          command, args, "Comando desconhecido");
}

static void ShowPrompt(void) {
    printf("\r\n> ");
}

static uint8_t HexCharToValue(char c) {
    c = toupper((unsigned char)c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0xFF;
}

static void TokenizeCommand(char* buffer, char** command, char** args) {
    // Pula espaços em branco no início
    while (isspace((unsigned char)*buffer)) buffer++;
    *command = buffer;

    // Encontra o fim do comando e o início dos argumentos
    *args = strchr(buffer, ' ');
    if (*args != NULL) {
        *(*args) = '\0'; // Termina a string do comando
        (*args)++;
        // Pula espaços em branco no início dos argumentos
        while (isspace((unsigned char)**args)) (*args)++;
        if (**args == '\0') {
            *args = NULL; // Argumentos são apenas espaços em branco
        }
    }
}

static void FindAndExecuteCommand(const CliCommand_t* command_table, size_t table_size,
                                  const char* command_name, char* args, const char* unknown_msg) {
    for (size_t i = 0; i < table_size; i++) {
        if (strcasecmp(command_name, command_table[i].name) == 0) {
            command_table[i].handler(args);
            return;
        }
    }
    printf("%s: \"%s\".", unknown_msg, command_name);
}