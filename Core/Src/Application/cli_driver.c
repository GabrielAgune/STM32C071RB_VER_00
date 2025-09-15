/*******************************************************************************
 * @file        cli_driver.c
 * @brief       Driver de Linha de Comando (CLI) não-bloqueante.
 * @version     2.2 (Refatorado por Dev STM - TX FIFO agora é Atômico/Interrupt-Safe)
 * @details     Garante que múltiplas tarefas chamando printf() (do main loop ou
 * de callbacks de evento) não corrompam o stream do FIFO de TX.
 ******************************************************************************/

#include "cli_driver.h"
#include "dwin_driver.h"
#include "app_manager.h" 
#include "scale_filter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

//================================================================================
// Definições do Driver CLI
//================================================================================

#define CLI_RX_BUFFER_SIZE      128
#define CLI_TX_FIFO_SIZE        256 // Buffer circular para printf

// Protótipos de Funções de Comando (Privadas)
static void Process_Command(void);
static void Cmd_Help(char* args);
static void Cmd_Dwin(char* args);
static void Cmd_GetPeso(char* args);
static void Cmd_GetTemp(char* args);
static void Cmd_GetFreq(char* args);
static void Handle_Dwin_PIC(char* sub_args);
static void Handle_Dwin_INT(char* sub_args);
static void Handle_Dwin_INT32(char* sub_args);
static void Handle_Dwin_RAW(char* sub_args);
static uint8_t hex_char_to_value(char c);

//================================================================================
// Variáveis Estáticas (Privadas)
//================================================================================

static UART_HandleTypeDef* s_huart_debug = NULL;

// --- Estado de RX ---
static char s_cli_rx_buffer[CLI_RX_BUFFER_SIZE];
static uint16_t s_cli_rx_index = 0;
static volatile bool s_command_ready = false; // Flag setada pela ISR de RX

// --- ESTADO DE TX (FIFO NÃO-BLOQUEANTE) ---
static uint8_t s_cli_tx_fifo[CLI_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0;
static volatile uint16_t s_tx_fifo_tail = 0;
static volatile bool s_tx_is_busy = false; // Flag: A ISR da UART está ativamente enviando
static uint8_t s_tx_temp_byte; // Buffer de 1 byte para o HAL_UART_Transmit_IT

// --- Tabela de Comandos Principal (COMANDOS RESTAURADOS) ---
typedef struct { const char* name; void (*handler)(char* args); } cli_command_t;
static const cli_command_t s_command_table[] = {
    { "HELP", Cmd_Help }, 
    { "?", Cmd_Help }, 
    { "DWIN", Cmd_Dwin },
    { "PESO", Cmd_GetPeso }, 
    { "TEMP", Cmd_GetTemp }, 
    { "FREQ", Cmd_GetFreq },
};
static const size_t NUM_COMMANDS = sizeof(s_command_table) / sizeof(s_command_table[0]);

// --- Tabela de Subcomandos DWIN ---
typedef struct { const char* name; void (*handler)(char* args); } dwin_subcommand_t;
static const dwin_subcommand_t s_dwin_table[] = {
    { "PIC", Handle_Dwin_PIC }, 
    { "INT", Handle_Dwin_INT },
    { "INT32", Handle_Dwin_INT32 }, 
    { "RAW", Handle_Dwin_RAW }
};
static const size_t NUM_DWIN_SUBCOMMANDS = sizeof(s_dwin_table) / sizeof(s_dwin_table[0]);

// --- Texto de Ajuda (ATUALIZADO) ---
static const char HELP_TEXT[] =
    "========================== CLI de Diagnostico =============================|\r\n"
    "| HELP ou ?                | Mostra esta ajuda.                            |\r\n"
    "| PESO                     | Mostra a leitura atual da balanca.            |\r\n"
    "| TEMP                     | Mostra a leitura do sensor de temperatura.    |\r\n"
    "| FREQ                     | Mostra a ultima leitura de frequencia.        |\r\n"
    "| DWIN PIC <id>            | Muda a tela (ex: DWIN PIC 1).                 |\r\n"
    "| DWIN INT <addr_h> <val>  | Escreve int16 no VP (ex: DWIN INT 2190 1234).  |\r\n"
    "| DWIN RAW <bytes_hex>     | Envia bytes crus para o DWIN (ex: 5AA5...).   |\r\n"
    "===========================================================================|\r\n";

//================================================================================
// Funções Públicas (Init, Process, RX)
//================================================================================

void CLI_Init(UART_HandleTypeDef* debug_huart) {
    s_huart_debug = debug_huart;
    printf("\r\nCLI Pronta. Digite 'HELP' para comandos.\r\n> ");
}

void CLI_Process(void) {
    if (s_command_ready) {
        printf("\r\n"); 
        Process_Command();
        
        memset(s_cli_rx_buffer, 0, CLI_RX_BUFFER_SIZE);
        s_cli_rx_index = 0;
        s_command_ready = false;
        
        printf("\r\n> "); // Envia o prompt
    }
}

// Lógica de RX original mantida (está correta)
void CLI_Receive_Char(uint8_t received_char) {
    if (s_command_ready) return; // Buffer de comando ainda sendo processado pelo main loop

    if (received_char == '\r' || received_char == '\n') {
        if (s_cli_rx_index > 0) {
            s_cli_rx_buffer[s_cli_rx_index] = '\0'; // Finaliza a string
            s_command_ready = true; // Sinaliza ao CLI_Process()
        } else {
            printf("\r\n> "); // Linha vazia, reimprime prompt
        }
    } 
    else if (received_char == '\b' || received_char == 127) // Backspace
    {
        if (s_cli_rx_index > 0) {
            s_cli_rx_index--;
            printf("\b \b"); // Ecoa backspace (usando o FIFO de TX assíncrono)
        }
    } 
    else if (s_cli_rx_index < (CLI_RX_BUFFER_SIZE - 1) && isprint(received_char)) 
    {
        s_cli_rx_buffer[s_cli_rx_index++] = (char)received_char;
        
        CLI_Printf_Transmit((uint8_t)received_char); 
    }
}

//================================================================================
// FUNÇÕES DE TRANSMISSÃO ASSÍNCRONA (TX FIFO) - (REFATORADAS PARA ATOMICIDADE)
//================================================================================

/**
 * @brief Adiciona um único caractere ao FIFO de TX.
 * Esta função deve ser chamada dentro de uma seção crítica (IRQ desabilitada).
 */
static void CLI_FIFO_Push(uint8_t ch)
{
    uint16_t next_head = (s_tx_fifo_head + 1) % CLI_TX_FIFO_SIZE;

    // Se o buffer estiver cheio (head vai colidir com tail), descarta o caractere
    if (next_head == s_tx_fifo_tail) {
        return; 
    }
    
    s_cli_tx_fifo[s_tx_fifo_head] = ch;
    s_tx_fifo_head = next_head;
}


/**
 * @brief Função principal para enfileirar dados do PRINTF (chamada por fputc).
 * Esta função agora garante atomicidade ao acessar o FIFO.
 */
void CLI_Printf_Transmit(uint8_t ch)
{
    // --- INÍCIO DA SEÇÃO CRÍTICA ---
    // Precisamos desabilitar a IRQ da UART1. Se a ISR de TX Cplt tentasse
    // acessar o FIFO (para pegar o próximo byte) ao mesmo tempo que
    // esta função adiciona um byte, os índices (head/tail) poderiam ser corrompidos.
    HAL_NVIC_DisableIRQ(USART1_IRQn);

    // Adiciona o caractere (ou CR+LF) ao FIFO
    if (ch == '\n') {
        CLI_FIFO_Push('\r');
    }
    CLI_FIFO_Push(ch);

    // --- Kickstart da Transmissão ---
    // Se a ISR da UART TX não estiver ocupada (ou seja, ela esvaziou o FIFO e parou),
    // devemos "acordá-la" manualmente aqui, enviando o primeiro byte da fila.
    if (!s_tx_is_busy) 
    {
        s_tx_is_busy = true; // Marcamos como ocupada
        
        // Pega o primeiro byte disponível no tail
        s_tx_temp_byte = s_cli_tx_fifo[s_tx_fifo_tail];
        s_tx_fifo_tail = (s_tx_fifo_tail + 1) % CLI_TX_FIFO_SIZE;
        
        // Inicia a transmissão de 1 BYTE por interrupção.
        // A partir daqui, a ISR (CLI_HandleTxCplt) assume o controle
        // até o FIFO ficar vazio.
        HAL_UART_Transmit_IT(s_huart_debug, &s_tx_temp_byte, 1);
    }

    // --- FIM DA SEÇÃO CRÍTICA ---
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/**
 * @brief (Função Chave) Callback chamado de HAL_UART_TxCpltCallback (Contexto de ISR).
 * Esta é a "engine" de esvaziamento do FIFO de TX. 
 */
void CLI_HandleTxCplt(void)
{
    // (Já estamos em contexto de ISR, não precisamos desabilitar IRQs aqui)

    // Verifica se o FIFO está vazio
    if (s_tx_fifo_head == s_tx_fifo_tail) {
        // FIFO vazio. A transmissão para.
        s_tx_is_busy = false;
        return;
    }

    // Ainda há dados. Pega o próximo byte do tail.
    s_tx_temp_byte = s_cli_tx_fifo[s_tx_fifo_tail];
    s_tx_fifo_tail = (s_tx_fifo_tail + 1) % CLI_TX_FIFO_SIZE;
    
    // Envia o próximo byte. Isso irá disparar este mesmo callback novamente quando terminar.
    // (A flag s_tx_is_busy permanece true)
    HAL_UART_Transmit_IT(s_huart_debug, &s_tx_temp_byte, 1);
}

bool CLI_IsTxBusy(void)
{
    // O driver está "ocupado" se a flag da ISR estiver ativa OU se o FIFO ainda não estiver vazio
    return (s_tx_is_busy || (s_tx_fifo_head != s_tx_fifo_tail));
}


//================================================================================
// Implementação dos Comandos (RESTABELECIDOS)
//================================================================================

static void Process_Command(void) {
    char* command_str = s_cli_rx_buffer;
    char* args = NULL;

    while (isspace((unsigned char)*command_str)) command_str++;

    args = strchr(command_str, ' ');
    if (args != NULL) {
        *args = '\0'; 
        args++;       
        while (isspace((unsigned char)*args)) args++;
        if (*args == '\0') args = NULL; 
    }

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
 * @brief (RESTAURADO) Comando CLI para ler dados da balança.
 */
static void Cmd_GetPeso(char* args) {
    ScaleFilterOut data;
    App_Manager_GetScaleData(&data); // Chama o getter seguro do app_manager
    printf("Dados da Balanca:\r\n");
    printf("  - Peso: %.2f g\r\n", data.avg_grams);
    printf("  - Estavel: %s\r\n", data.is_stable ? "SIM" : "NAO");
    printf("  - Sigma: %.2f g\r\n", data.sigma_grams);
    printf("  - ADC Counts (avg): %.1f\r\n", data.avg_counts);
}

/**
 * @brief (RESTAURADO) Comando CLI para ler temperatura do MCU.
 */
static void Cmd_GetTemp(char* args) {
    float temperatura = App_Manager_GetTemperature(); // Chama o getter seguro
    printf("Temperatura interna do MCU: %.2f C\r\n", temperatura);
}

/**
 * @brief (RESTAURADO) Comando CLI para ler dados de frequência.
 */
static void Cmd_GetFreq(char* args) {
    FreqData_t data;
    App_Manager_GetFreqData(&data); // Chama o getter seguro
    printf("Dados de Frequencia:\r\n");
    printf("  - Pulsos: %lu\r\n", (unsigned long)data.pulsos);
    printf("  - Escala A: %.2f\r\n", data.escala_a);
}

// --- Handlers DWIN (Lógica original mantida, mas agora verifica DWIN_IsTxBusy) ---

static void Cmd_Dwin(char* args) {
    if (args == NULL) { printf("Subcomando DWIN faltando. Use 'HELP'."); return; }
    
    char* sub_cmd = args;
    char* sub_args = NULL;
    
    sub_args = strchr(sub_cmd, ' ');
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
    printf("Subcomando DWIN desconhecido: \"%s\"", sub_cmd);
}

static void Handle_Dwin_PIC(char* sub_args) {
    if (sub_args == NULL) { printf("Uso: DWIN PIC <id>"); return; }
    
    if (DWIN_Driver_IsTxBusy()) {
        printf("ERRO: Driver DWIN (UART2) ocupado. Tente novamente.");
        return;
    }
    DWIN_Driver_SetScreen(atoi(sub_args));
    printf("Comando DWIN PIC enviado.");
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
    
    if (DWIN_Driver_IsTxBusy()) { printf("ERRO: Driver DWIN (UART2) ocupado."); return; }
    DWIN_Driver_WriteInt(vp, val);
    printf("Escrevendo (int16) %d em 0x%04X", val, vp);
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
    
    if (DWIN_Driver_IsTxBusy()) { printf("ERRO: Driver DWIN (UART2) ocupado."); return; }
    DWIN_Driver_WriteInt32(vp, val);
    printf("Escrevendo (int32) %ld em 0x%04X", (long)val, vp);
}

static uint8_t hex_char_to_value(char c) {
    c = toupper((unsigned char)c);
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return 0xFF; // Erro
}

static void Handle_Dwin_RAW(char* sub_args) {
    if (sub_args == NULL) { 
        printf("Uso: DWIN RAW <byte_hex> ..."); 
        return; 
    }
    
    // CORREÇÃO AQUI: Deve usar CLI_RX_BUFFER_SIZE
    uint8_t raw_buffer[CLI_RX_BUFFER_SIZE / 2];
    int byte_count = 0;
    char* ptr = sub_args;

    // CORREÇÃO AQUI: Deve usar CLI_RX_BUFFER_SIZE
    while (*ptr != '\0' && byte_count < (CLI_RX_BUFFER_SIZE / 2)) {
        while (isspace((unsigned char)*ptr)) ptr++;
        if (*ptr == '\0') break;
        
        char high_c = *ptr++;
        if (*ptr == '\0' || isspace((unsigned char)*ptr)) { 
            printf("\nErro: Numero impar de caracteres hex."); 
            return; 
        }
        char low_c = *ptr++;
        
        uint8_t high_v = hex_char_to_value(high_c);
        uint8_t low_v = hex_char_to_value(low_c);
        
        if (high_v == 0xFF || low_v == 0xFF) { 
            printf("\nErro: Caractere invalido na string hex."); 
            return; 
        }
        
        raw_buffer[byte_count++] = (high_v << 4) | low_v;
    }
    
    printf("Enviando %d bytes:", byte_count);
    for(int i = 0; i < byte_count; i++) printf(" %02X", raw_buffer[i]);
    
    if (DWIN_Driver_IsTxBusy()) { 
        printf("\nERRO: Driver DWIN (UART2) ocupado."); 
        return; 
    }
    
    DWIN_Driver_WriteRawBytes(raw_buffer, byte_count);
}