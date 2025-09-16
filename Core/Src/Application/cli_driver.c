/*******************************************************************************
 * @file        cli_driver.c
 * @brief       Driver CLI Não-Bloqueante (Arquitetura V8.2 - Correção de Typo)
 * @version     5.3 (Refatorado por Dev STM)
 * @details     Usa SW FIFO (1K) + DMA Pump (Main Loop).
 * CORRIGIDO V8.2: Typo s_cli_tx_fifo_tail -> s_tx_fifo_tail.
 ******************************************************************************/

#include "cli_driver.h"
#include "dwin_driver.h"
#include "app_manager.h" 
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

//================================================================================
// Definições
//================================================================================
#define CLI_RX_BUFFER_SIZE      128
#define CLI_TX_FIFO_SIZE        1024 // AUMENTADO PARA SUPORTAR MENUS DE AJUDA LONGOS
#define CLI_TX_DMA_BUFFER_SIZE  64  

//================================================================================
// Protótipos Privados e Typedefs
//================================================================================

// Tabela de Comandos Principal
typedef struct { 
    const char* name; 
    void (*handler)(char* args); 
} cli_command_t;

// Tabela de Subcomandos DWIN
typedef struct { 
    const char* name; 
    void (*handler)(char* args); 
} dwin_subcommand_t;

static void Process_Command(void);
static void Cmd_Help(char* args);
static void Cmd_Dwin(char* args);
static void Cmd_GetPeso(char* args); // <-- Modificado
static void Cmd_GetTemp(char* args);
static void Cmd_GetFreq(char* args);
static void Handle_Dwin_PIC(char* sub_args);
static void Handle_Dwin_INT(char* sub_args);
static void Handle_Dwin_INT32(char* sub_args);
static void Handle_Dwin_RAW(char* sub_args);
static uint8_t hex_char_to_value(char c);

//================================================================================
// Variáveis Estáticas
//================================================================================
static UART_HandleTypeDef* s_huart_debug = NULL;

// --- RX (IT 1-byte) ---
static uint8_t s_cli_rx_byte; 
static char s_cli_rx_buffer[CLI_RX_BUFFER_SIZE]; 
static uint16_t s_cli_rx_index = 0;
static volatile bool s_command_ready = false;

// --- TX (Software FIFO + DMA) ---
static uint8_t s_cli_tx_fifo[CLI_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0;
static volatile uint16_t s_tx_fifo_tail = 0;
static uint8_t s_cli_tx_dma_buffer[CLI_TX_DMA_BUFFER_SIZE]; 
static volatile bool s_dma_tx_busy = false; 


// --- Tabelas de Comando (Instâncias) ---
static const cli_command_t s_command_table[] = {
    { "HELP", Cmd_Help }, { "?", Cmd_Help }, { "DWIN", Cmd_Dwin },
    { "PESO", Cmd_GetPeso }, { "TEMP", Cmd_GetTemp }, { "FREQ", Cmd_GetFreq },
};
static const size_t NUM_COMMANDS = sizeof(s_command_table) / sizeof(s_command_table[0]);

static const dwin_subcommand_t s_dwin_table[] = {
    { "PIC", Handle_Dwin_PIC }, { "INT", Handle_Dwin_INT },
    { "INT32", Handle_Dwin_INT32 }, { "RAW", Handle_Dwin_RAW }
};
static const size_t NUM_DWIN_SUBCOMMANDS = sizeof(s_dwin_table) / sizeof(s_dwin_table[0]);

static const char HELP_TEXT[] =
    "========================== CLI de Diagnostico (V8.2) ======================|\r\n"
    "| HELP ou ?                | Mostra esta ajuda.                            |\r\n"
    "| PESO                     | Mostra a leitura atual da balanca.            |\r\n"
    "| TEMP                     | Mostra a leitura do sensor de temperatura.    |\r\n"
    "| FREQ                     | Mostra a ultima leitura de frequencia.        |\r\n"
    "| DWIN PIC <id>            | Muda a tela (ex: DWIN PIC 1).                 |\r\n"
    "| DWIN INT <addr_h> <val>  | Escreve int16 no VP (ex: DWIN INT 2190 1234).  |\r\n"
    "| DWIN RAW <bytes_hex>     | Envia bytes crus para o DWIN (ex: 5AA5...).   |\r\n"
    "===========================================================================|\r\n";

//================================================================================
// Funções Públicas (Init, Processadores de Loop)
//================================================================================

void CLI_Init(UART_HandleTypeDef* debug_huart) {
    s_huart_debug = debug_huart;
    if(HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1) != HAL_OK) {
        Error_Handler();
    }
    printf("\r\nCLI Pronta. Digite 'HELP' para comandos.\r\n> ");
}

void CLI_Process(void) {
    if (s_command_ready) {
        printf("\r\n"); 
        Process_Command();
        memset(s_cli_rx_buffer, 0, CLI_RX_BUFFER_SIZE);
        s_cli_rx_index = 0;
        s_command_ready = false; 
        printf("\r\n> "); 
    }
}

/**
 * @brief (V8.1) Bomba de TX do CLI (chamada no super-loop).
 */
void CLI_TX_Pump(void)
{
    if (s_dma_tx_busy || (s_tx_fifo_head == s_tx_fifo_tail)) {
        return;
    }

    // --- Seção Crítica --- 
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    
    if (s_dma_tx_busy) // Dupla verificação
    {
        HAL_NVIC_EnableIRQ(USART1_IRQn);
        HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
        return;
    }
    
    s_dma_tx_busy = true;
    
    uint16_t bytes_to_send = 0;
    while ((s_tx_fifo_tail != s_tx_fifo_head) && (bytes_to_send < CLI_TX_DMA_BUFFER_SIZE))
    {
        s_cli_tx_dma_buffer[bytes_to_send] = s_cli_tx_fifo[s_tx_fifo_tail];
        
        // **** CORREÇÃO V8.2 (Correção do Typo) ****
        s_tx_fifo_tail = (s_tx_fifo_tail + 1) % CLI_TX_FIFO_SIZE; // Corrigido de s_cli_tx_fifo_tail
        // **** FIM DA CORREÇÃO ****
        
        bytes_to_send++;
    }

    HAL_NVIC_EnableIRQ(USART1_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    // --- Fim da Seção Crítica ---

    if (HAL_UART_Transmit_DMA(s_huart_debug, s_cli_tx_dma_buffer, bytes_to_send) != HAL_OK)
    {
        s_dma_tx_busy = false; 
    }
}

//================================================================================
// FUNÇÕES DE TRANSMISSÃO E RECEPÇÃO (Callbacks e Helpers)
//================================================================================

/**
 * @brief Adiciona um char ao FIFO de TX (chamado pelo fputc).
 * CORRIGIDO V8.1: Usa NVIC_DisableIRQ específico em vez de __disable_irq() global.
 */
void CLI_Printf_Transmit(uint8_t ch)
{
    // Usa interrupções específicas do periférico em vez de desabilitar globalmente.
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


/**
 * @brief (Callback da ISR de RX) Chamado por HAL_UART_RxCpltCallback (ISR Context).
 * (Corrigido V8.1: Ecoa via FIFO, não bloqueante).
 */
void CLI_HandleRxCplt(UART_HandleTypeDef *huart)
{
    if (s_command_ready) {
        // Ignora byte. Main loop ainda não processou o comando anterior.
    }
    else if (s_cli_rx_byte == '\r' || s_cli_rx_byte == '\n') {
        if (s_cli_rx_index > 0) {
            s_cli_rx_buffer[s_cli_rx_index] = '\0'; 
            s_command_ready = true; 
        } else {
            CLI_Printf_Transmit('\r'); // Enfileira o eco
            CLI_Printf_Transmit('\n');
            CLI_Printf_Transmit('>');
            CLI_Printf_Transmit(' ');
        }
    } 
    else if (s_cli_rx_byte == '\b' || s_cli_rx_byte == 127) // Backspace
    {
        if (s_cli_rx_index > 0) {
            s_cli_rx_index--;
            CLI_Printf_Transmit('\b'); 
            CLI_Printf_Transmit(' ');
            CLI_Printf_Transmit('\b');
        }
    } 
    else if (s_cli_rx_index < (CLI_RX_BUFFER_SIZE - 1) && isprint(s_cli_rx_byte)) 
    {
        s_cli_rx_buffer[s_cli_rx_index++] = (char)s_cli_rx_byte;
        CLI_Printf_Transmit(s_cli_rx_byte); // Enfileira o eco (NÃO BLOQUEANTE)
    }
    
    // Rearma a interrupção de 1 byte
    if (HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1) != HAL_OK)
    {
         HAL_UART_AbortReceive_IT(s_huart_debug);
         HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1);
    }
}

/**
 * @brief (Callback da ISR de TX) Chamado por HAL_UART_TxCpltCallback (ISR Context DMA).
 * Apenas libera a flag. O Pump no main loop fará o resto.
 */
void CLI_HandleTxCplt(UART_HandleTypeDef *huart)
{
    s_dma_tx_busy = false; 
}

/**
 * @brief (Callback da ISR de Erro) Chamado por HAL_UART_ErrorCallback.
 */
void CLI_HandleError(UART_HandleTypeDef *huart)
{
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE)) {
        (void)huart->Instance->RDR; 
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
    }
    
    HAL_UART_AbortReceive_IT(s_huart_debug); 
    HAL_UART_Receive_IT(s_huart_debug, &s_cli_rx_byte, 1); // Reinicia RX IT
}

//================================================================================
// Implementação dos Comandos (Handlers)
//================================================================================

static void Process_Command(void) {
    char* command_str = s_cli_rx_buffer;
    char* args = NULL;
    while (isspace((unsigned char)*command_str)) command_str++;
    args = strchr(command_str, ' ');
    if (args != NULL) { *args = '\0'; args++; while (isspace((unsigned char)*args)) args++; if (*args == '\0') args = NULL; }
    if (*command_str == '\0') return;
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (strcasecmp(command_str, s_command_table[i].name) == 0) {
            s_command_table[i].handler(args);
            return;
        }
    }
    printf("Comando desconhecido: \"%s\".", command_str);
}

static void Cmd_Help(char* args) { 
    printf("%s", HELP_TEXT); 
}

/**
 * @brief (ATUALIZADO V8.2) Usa a nova struct App_ScaleData_t
 */
static void Cmd_GetPeso(char* args) {
    App_ScaleData_t data; // <-- USA A NOVA STRUCT (de app_manager.h)
    App_Manager_GetScaleData(&data);
    
    printf("Dados da Balanca:\r\n");
    printf("  - Peso: %.2f g\r\n", data.grams_display);
    printf("  - Estavel: %s\r\n", data.is_stable ? "SIM" : "NAO");
    printf("  - ADC Counts (mediana): %.0f\r\n", data.raw_counts_median);
}

static void Cmd_GetTemp(char* args) {
    float temperatura = App_Manager_GetTemperature();
    printf("Temperatura interna do MCU: %.2f C\r\n", temperatura);
}

static void Cmd_GetFreq(char* args) {
    FreqData_t data;
    App_Manager_GetFreqData(&data);
    printf("Dados de Frequencia:\r\n");
    printf("  - Pulsos (em 1s): %lu\r\n", (unsigned long)data.pulsos);
    printf("  - Escala A (calc): %.2f\r\n", data.escala_a);
}

static void Cmd_Dwin(char* args) {
    if (args == NULL) { printf("Subcomando DWIN faltando. Use 'HELP'."); return; }
    char* sub_cmd = args;
    char* sub_args = NULL;
    sub_args = strchr(sub_cmd, ' ');
    if (sub_args != NULL) { *sub_args = '\0'; sub_args++; while (isspace((unsigned char)*sub_args)) sub_args++; if (*sub_args == '\0') sub_args = NULL; }
    for (size_t i = 0; i < NUM_DWIN_SUBCOMMANDS; i++) {
        if (strcasecmp(sub_cmd, s_dwin_table[i].name) == 0) {
            s_dwin_table[i].handler(sub_args);
            return;
        }
    }
    printf("Subcomando DWIN desconhecido: \"%s\"", sub_cmd);
}

static void Handle_Dwin_PIC(char* sub_args) {
    if (sub_args == NULL) { printf("Uso: DWIN PIC <id>"); return; }
    DWIN_Driver_SetScreen(atoi(sub_args));
    printf("Comando DWIN PIC enfileirado.");
}

static void Handle_Dwin_INT(char* sub_args) {
    if (sub_args == NULL) { printf("Uso: DWIN INT <addr_hex> <valor>"); return; }
    char* val_str = NULL;
    char* addr_str = sub_args;
    val_str = strchr(addr_str, ' ');
    if (val_str == NULL) { printf("Valor faltando."); return; }
    *val_str = '\0'; val_str++;
    uint16_t vp = strtol(addr_str, NULL, 16);
    int16_t val = atoi(val_str);
    DWIN_Driver_WriteInt(vp, val);
    printf("Enfileirado (int16) %d em 0x%04X", val, vp);
}

static void Handle_Dwin_INT32(char* sub_args) {
     if (sub_args == NULL) { printf("Uso: DWIN INT32 <addr_hex> <valor>"); return; }
    char* val_str = NULL;
    char* addr_str = sub_args;
    val_str = strchr(addr_str, ' ');
    if (val_str == NULL) { printf("Valor faltando."); return; }
    *val_str = '\0'; val_str++;
    uint16_t vp = strtol(addr_str, NULL, 16);
    int32_t val = atol(val_str);
    DWIN_Driver_WriteInt32(vp, val);
    printf("Enfileirado (int32) %ld em 0x%04X", (long)val, vp);
}

static uint8_t hex_char_to_value(char c) {
    c = toupper((unsigned char)c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0xFF; 
}

static void Handle_Dwin_RAW(char* sub_args) {
    if (sub_args == NULL) { printf("Uso: DWIN RAW <byte_hex> ..."); return; }
    uint8_t raw_buffer[CLI_RX_BUFFER_SIZE / 2];
    int byte_count = 0;
    char* ptr = sub_args;
    while (*ptr != '\0' && byte_count < (CLI_RX_BUFFER_SIZE / 2)) {
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') break;
        char high_c = *ptr++;
        if (*ptr == '\0' || isspace((unsigned char)*ptr)) { printf("\nErro: Numero impar de caracteres hex."); return; }
        char low_c = *ptr++;
        uint8_t high_v = hex_char_to_value(high_c);
        uint8_t low_v = hex_char_to_value(low_c);
        if (high_v == 0xFF || low_v == 0xFF) { printf("\nErro: Caractere invalido na string hex."); return; }
        raw_buffer[byte_count++] = (high_v << 4) | low_v;
    }
    printf("Enfileirando %d bytes para DWIN:", byte_count);
    for(int i = 0; i < byte_count; i++) printf(" %02X", raw_buffer[i]);
    
    DWIN_Driver_WriteRawBytes(raw_buffer, byte_count);
}