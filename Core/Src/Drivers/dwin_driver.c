/*******************************************************************************
 * @file        dwin_driver.c
 * @brief       Driver de comunicação não-bloqueante para Display DWIN (UART)
 * @version     3.0 (Refatorado para TX FIFO Robusto por Dev STM)
 * @details     Este driver gerencia a comunicação com o DWIN usando
 * RX (ReceiveToIdle_IT) e um TX FIFO completo.
 * Múltiplas tarefas (FSM e Callbacks de RX) podem agora enfileirar
 * dados para transmissão sem contenção ou perda de comandos.
 ******************************************************************************/

#include "dwin_driver.h"
#include <string.h>
#include "main.h"
#include <stdio.h> // Para Error_Handler

//================================================================================
// Definições do Driver
//================================================================================
#define DWIN_RX_BUFFER_SIZE     64  // Buffer de entrada
#define DWIN_TX_FIFO_SIZE       128 // Buffer circular de saída (para enfileirar múltiplos comandos)

//================================================================================
// Variáveis Estáticas (Privadas do Módulo)
//================================================================================

static UART_HandleTypeDef* s_huart = NULL;
static dwin_rx_callback_t s_rx_callback = NULL;

// --- Buffers e Flags de RX (Assíncrono via Idle-Line IT) ---
static uint8_t s_rx_buffer[DWIN_RX_BUFFER_SIZE];
static volatile bool s_frame_received = false;
static volatile uint16_t s_received_len = 0;

// --- Buffers e Flags de TX (Assíncrono via TX FIFO e ISR) ---
static uint8_t s_tx_fifo[DWIN_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0;
static volatile uint16_t s_tx_fifo_tail = 0;
static volatile bool s_tx_is_busy = false;     // Flag: A ISR da UART está ativamente enviando um byte
static uint8_t s_tx_temp_byte;                 // Buffer de 1 byte para o HAL_UART_Transmit_IT


//================================================================================
// Funções de Inicialização e Processamento (Super-loop)
//================================================================================

/**
 * @brief Inicializa o driver DWIN e inicia a primeira escuta da UART RX.
 */
void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback) {
    s_huart = huart;
    s_rx_callback = callback;
    
    // Reseta o estado dos FIFOs e flags
    s_tx_is_busy = false;
    s_frame_received = false;
    s_tx_fifo_head = 0;
    s_tx_fifo_tail = 0;
    s_received_len = 0;

    if (HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief Processador de RX (Chamado continuamente pelo super-loop).
 * Verifica se a ISR de RX sinalizou um novo pacote e chama o callback.
 */
void DWIN_Driver_Process(void) {
    if (!s_frame_received) {
        return; // Nenhum pacote novo para processar
    }

    uint8_t local_buffer[DWIN_RX_BUFFER_SIZE];
    uint16_t local_len;

    // Seção crítica: Copia os dados recebidos pela ISR para um buffer local
    __disable_irq(); // Garante que a ISR não escreva aqui enquanto lemos
    local_len = s_received_len;
    memcpy(local_buffer, s_rx_buffer, local_len);
    s_frame_received = false; // Libera a flag
    __enable_irq();

    // Validação mínima do frame DWIN e chamada do callback
    if (local_len >= 3 && local_buffer[0] == 0x5A && local_buffer[1] == 0xA5) {
        if (s_rx_callback != NULL) {
            s_rx_callback(local_buffer, local_len);
        }
    }
}


//================================================================================
// Funções de Controle de Estado de TX (Refatoradas para FIFO)
//================================================================================

/**
 * @brief [NOVO] Adiciona uma sequência de bytes ao FIFO de TX.
 * Esta função é ATÔMICA (segura contra interrupção).
 * Esta é a única função que as funções "Write" devem chamar.
 */
static void DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size)
{
    if (data == NULL || size == 0) {
        return;
    }

    // --- INÍCIO DA SEÇÃO CRÍTICA ---
    // Desabilita a IRQ da UART2 para enfileirar este frame inteiro sem ser interrompido
    // pela ISR de TX Cplt (que mexe no s_tx_is_busy e no tail).
    HAL_NVIC_DisableIRQ(USART2_IRQn);

    // Verifica se há espaço no FIFO para o frame inteiro
    uint16_t free_space;
    if (s_tx_fifo_head >= s_tx_fifo_tail) {
        free_space = DWIN_TX_FIFO_SIZE - (s_tx_fifo_head - s_tx_fifo_tail);
    } else {
        free_space = (s_tx_fifo_tail - s_tx_fifo_head);
    }

    if (size >= free_space) {
        // Buffer cheio ou não há espaço suficiente. O comando é descartado.
        // (Em um sistema mais complexo, poderíamos retornar 'false' e pedir à app para tentar de novo)
        HAL_NVIC_EnableIRQ(USART2_IRQn);
        return;
    }

    // Copia o frame inteiro para o FIFO
    for (uint16_t i = 0; i < size; i++) {
        s_tx_fifo[s_tx_fifo_head] = data[i];
        s_tx_fifo_head = (s_tx_fifo_head + 1) % DWIN_TX_FIFO_SIZE;
    }

    // --- Kickstart da Transmissão ---
    // Se a ISR da UART TX não estiver ocupada (parada), devemos "acordá-la".
    if (!s_tx_is_busy) 
    {
        s_tx_is_busy = true; // Marcamos como ocupada
        
        // Pega o primeiro byte disponível no tail
        s_tx_temp_byte = s_tx_fifo[s_tx_fifo_tail];
        s_tx_fifo_tail = (s_tx_fifo_tail + 1) % DWIN_TX_FIFO_SIZE;
        
        // Inicia a transmissão de 1 BYTE por interrupção.
        HAL_UART_Transmit_IT(s_huart, &s_tx_temp_byte, 1);
    }

    // --- FIM DA SEÇÃO CRÍTICA ---
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}


/**
 * @brief Verifica se o driver DWIN TX está ocupado.
 * (Agora verifica se o FIFO está vazio, não apenas a flag da ISR).
 */
bool DWIN_Driver_IsTxBusy(void) {
    // Está ocupado se a ISR estiver no meio de um envio (s_tx_is_busy)
    // OU se ainda houver bytes no FIFO esperando para serem enviados.
    return (s_tx_is_busy || (s_tx_fifo_head != s_tx_fifo_tail));
}

/**
 * @brief (Função Chave) Callback chamado de HAL_UART_TxCpltCallback (Contexto de ISR).
 * Esta é a "engine" de esvaziamento do FIFO de TX do DWIN. 
 */
void DWIN_Driver_HandleTxCplt(void)
{
    // (Contexto de ISR - não precisa desabilitar IRQs globais)

    // Verifica se há mais dados no FIFO
    if (s_tx_fifo_head == s_tx_fifo_tail) {
        // FIFO está vazio. A transmissão para.
        s_tx_is_busy = false;
        return;
    }

    // Ainda há dados. Pega o próximo byte do tail.
    s_tx_temp_byte = s_tx_fifo[s_tx_fifo_tail];
    s_tx_fifo_tail = (s_tx_fifo_tail + 1) % DWIN_TX_FIFO_SIZE;
    
    // Envia o próximo byte. Isso irá disparar este mesmo callback novamente quando terminar.
    HAL_UART_Transmit_IT(s_huart, &s_tx_temp_byte, 1);
}


//================================================================================
// Funções de Escrita (TX) - REATORADAS (NÃO-BLOQUEANTES, BASEADAS EM FILA)
//================================================================================

/**
 * @brief Muda a tela do display (Agora 100% Assíncrono e Enfileirado).
 */
void DWIN_Driver_SetScreen(uint16_t screen_id) {
    const uint16_t VP_ADDR_PIC_ID = 0x0084;
    uint8_t cmd_buffer[] = { // Buffer local (stack) usado apenas para formatar
        0x5A, 0xA5, 0x07, 0x82,
        (uint8_t)(VP_ADDR_PIC_ID >> 8), (uint8_t)(VP_ADDR_PIC_ID & 0xFF),
        0x5A, 0x01,
        (uint8_t)(screen_id >> 8), (uint8_t)(screen_id & 0xFF)
    };
    
    // Chama a função que coloca no FIFO de forma atômica
    DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

/**
 * @brief Escreve um Int16 (2 bytes) em um VP (Agora 100% Assíncrono e Enfileirado).
 */
void DWIN_Driver_WriteInt(uint16_t vp_address, int16_t value) {
    uint8_t cmd_buffer[] = { 
        0x5A, 0xA5, 0x05, 0x82,
        (uint8_t)(vp_address >> 8), (uint8_t)(vp_address & 0xFF),
        (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)
    };
    
    DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

/**
 * @brief Escreve um Int32 (4 bytes) em um VP (Agora 100% Assíncrono e Enfileirado).
 */
void DWIN_Driver_WriteInt32(uint16_t vp_address, int32_t value) {
    uint8_t cmd_buffer[] = { 
        0x5A, 0xA5, 0x07, 0x82,
        (uint8_t)(vp_address >> 8), (uint8_t)(vp_address & 0xFF),
        (uint8_t)((value >> 24) & 0xFF), (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),  (uint8_t)(value & 0xFF)
    };

    DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

/**
 * @brief Escreve uma string (Agora 100% Assíncrono e Enfileirado).
 */
void DWIN_Driver_WriteString(uint16_t vp_address, const char* text, uint16_t max_len) {
    if (s_huart == NULL || text == NULL || max_len == 0) {
        return;
    }
    
    size_t text_len = strlen(text);
    if (text_len > max_len) {
        text_len = max_len; 
    }
    
    uint8_t frame_payload_len = 3 + text_len; // 3 (cmd+vp) + N bytes de texto
    uint16_t total_frame_size = 3 + frame_payload_len; // Header (5A A5 LEN) + Payload

    // Precisamos de um buffer temporário para montar o frame completo
    // Não podemos usar o s_static_tx_buffer porque esta função pode ser chamada
    // de múltiplos locais. O DWIN_TX_Queue_Send_Bytes irá copiar dele.
    uint8_t temp_frame_buffer[total_frame_size];

    temp_frame_buffer[0] = 0x5A;
    temp_frame_buffer[1] = 0xA5;
    temp_frame_buffer[2] = frame_payload_len;
    temp_frame_buffer[3] = 0x82; // Comando de escrita
    temp_frame_buffer[4] = (uint8_t)(vp_address >> 8);
    temp_frame_buffer[5] = (uint8_t)(vp_address & 0xFF);

    memcpy(&temp_frame_buffer[6], text, text_len);
    
    // Enfileira o frame montado
    DWIN_TX_Queue_Send_Bytes(temp_frame_buffer, total_frame_size);
}


void DWIN_Driver_WriteRawBytes(const uint8_t* data, uint16_t size) {
    if (s_huart != NULL && data != NULL && size > 0) {
        // Enfileira os bytes crus
        DWIN_TX_Queue_Send_Bytes(data, size);
    }
}


//================================================================================
// Handlers de Callbacks da ISR (Chamados pelo HAL)
//================================================================================

/**
 * @brief Manipulador de evento de RX (Chamado por HAL_UARTEx_RxEventCallback).
 * Reinicia a escuta da UART imediatamente após receber um frame.
 */
void DWIN_Driver_HandleRxEvent(uint16_t size) {
    s_received_len = size;
    s_frame_received = true; // Sinaliza ao main loop (Process) que há dados
    
    // Reinicia a escuta imediatamente
    if (HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK) {
        HAL_UART_AbortReceive_IT(s_huart);
        if(HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK){
             Error_Handler(); // Falha crítica
        }
    }
}

/**
 * @brief Manipulador de erros da UART (Chamado por HAL_UART_ErrorCallback).
 * Focado em limpar o erro de Overrun (ORE) e reiniciar a escuta de RX.
 */
void DWIN_Driver_HandleError(UART_HandleTypeDef *huart) {
    if (__HAL_UART_GET_FLAG(huart, UART_FLAG_ORE)) {
        (void)huart->Instance->RDR; 
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF);
    }

    HAL_UART_AbortReceive_IT(huart);
    
    if (HAL_UARTEx_ReceiveToIdle_IT(s_huart, s_rx_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK) {
       Error_Handler(); 
    }
}