/*******************************************************************************
 * @file        dwin_driver.c
 * @brief       Driver DWIN (UART2) NÃO-BLOQUEANTE (Arquitetura V6.0 - DMA TX/RX)
 * @version     6.0 (Refatoração por Dev STM)
 * @details     RX: Usa HAL_UARTEx_ReceiveToIdle_DMA + Debounce de Software (20ms).
 * Isso é eficiente (baixo uso de CPU) e robusto contra senders lentos
 * (corrige o bug da senha 'senh').
 * TX: Usa SW FIFO + DMA Pump no Main Loop.
 ******************************************************************************/

#include "dwin_driver.h"
#include <string.h>
#include "main.h"
#include <stdio.h> 

//================================================================================
// Definições do Driver
//================================================================================
#define DWIN_RX_BUFFER_SIZE     64  // Buffer de entrada (Linear, para DMA)
#define DWIN_TX_FIFO_SIZE       128 // Buffer circular de saída (Software FIFO)
#define DWIN_TX_DMA_BUFFER_SIZE 64  // Bloco linear de TX para o DMA ler

/**
 * @brief Debounce de software (em ms) para o RX. 
 * O DWIN é lento. Esperamos este tempo após um evento IDLE para garantir que o 
 * pacote (ex: "senha") está realmente completo antes de processá-lo.
 */
#define DWIN_RX_PACKET_TIMEOUT_MS 20 

//================================================================================
// Variáveis Estáticas (Privadas do Módulo)
//================================================================================

static UART_HandleTypeDef* s_huart = NULL;
static dwin_rx_callback_t s_rx_callback = NULL;

// --- Buffers e Flags de RX (DMA + Idle-Line IT + SW Debounce) ---
static uint8_t s_rx_dma_buffer[DWIN_RX_BUFFER_SIZE]; // DMA escreve aqui
static volatile bool s_rx_pending_data = false;    // Flag: ISR -> Main (Temos dados, aguardando debounce)
static volatile uint16_t s_received_len = 0;     // Tamanho dos dados pendentes
static volatile uint32_t s_last_rx_event_tick = 0; // Tick de quando o último byte/idle chegou


// --- Buffers e Flags de TX (Software FIFO + DMA Pump) ---
static uint8_t s_tx_fifo[DWIN_TX_FIFO_SIZE];
static volatile uint16_t s_tx_fifo_head = 0;
static volatile uint16_t s_tx_fifo_tail = 0;
static uint8_t s_tx_dma_buffer[DWIN_TX_DMA_BUFFER_SIZE]; // DMA lê daqui
static volatile bool s_dma_tx_busy = false; // Flag: DMA está transferindo ATIVAMENTE
static volatile bool s_rx_needs_reset = false; 
static uint32_t s_rx_error_cooldown_tick = 0; 

// Protótipo interno
static void DWIN_Start_Listening(void);
static void DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size);

//================================================================================
// Funções de Inicialização e Processamento (Super-loop)
//================================================================================

/**
 * @brief Inicia a escuta de RX (DMA + Idle Line).
 */
static void DWIN_Start_Listening(void)
{
    // Inicia o RX via DMA com detecção de IDLE
    if (HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK)
    {
        HAL_UART_AbortReceive_IT(s_huart);
        if (HAL_UARTEx_ReceiveToIdle_DMA(s_huart, s_rx_dma_buffer, DWIN_RX_BUFFER_SIZE) != HAL_OK)
        {
            Error_Handler(); // Falha crítica
        }
    }
}

void DWIN_Driver_Init(UART_HandleTypeDef *huart, dwin_rx_callback_t callback) {
    s_huart = huart;
    s_rx_callback = callback;
    
    s_dma_tx_busy = false;
    s_rx_pending_data = false;
    s_tx_fifo_head = 0;
    s_tx_fifo_tail = 0;
    
    DWIN_Start_Listening(); // Inicia a escuta RX
}

/**
 * @brief [V8.1] Processador de RX (Chamado no super-loop).
 * Implementa o "debounce de software" (20ms) para pacotes DWIN lentos/quebrados.
 * Isso corrige o bug da senha "senh".
 */
void DWIN_Driver_Process(void) {
		
		if (s_rx_error_cooldown_tick != 0)
    {
        // Verifica se o tempo de 100ms já passou
        if (HAL_GetTick() - s_rx_error_cooldown_tick < 100)
        {
            return; // Ainda em cooldown, não faz nada
        }
        
        // Cooldown de 100ms terminou.
        s_rx_error_cooldown_tick = 0; // Limpa o timer
        s_rx_needs_reset = true;    // Agora sim, sinaliza para o reset
    }
		
		if (s_rx_needs_reset) {
        s_rx_needs_reset = false;    // Consome o flag
        s_rx_pending_data = false; // Descarta quaisquer dados parciais
        
        printf("[WARN] DWIN UART RX Resetado apos erro.\r\n"); // <-- Agora é seguro imprimir
        
        // Aborta e reinicia a escuta (agora é seguro fazer isso)
        HAL_UART_AbortReceive_IT(s_huart);
        DWIN_Start_Listening();
        return; // Retorna e espera o próximo ciclo do loop principal
    }
		
    if (!s_rx_pending_data) {
        return;
    }

    if (HAL_GetTick() - s_last_rx_event_tick < DWIN_RX_PACKET_TIMEOUT_MS) {
        return;
    }
		
		// --- DEBUG: Verificar se o buffer DMA tem dados antes da cópia ---
		printf("[DEBUG] Conteudo do buffer DMA (s_received_len = %d): ", s_received_len);
		for (uint16_t i = 0; i < s_received_len; i++) {
				printf("%02X ", s_rx_dma_buffer[i]);
		}
		printf("\r\n");

    // --- Filtro rápido de pacote ACK "OK" diretamente do buffer DMA ---
    if (s_received_len == 6 &&
        s_rx_dma_buffer[0] == 0x5A &&
        s_rx_dma_buffer[1] == 0xA5 &&
        s_rx_dma_buffer[2] == 0x03 &&
        s_rx_dma_buffer[3] == 0x82 &&
        s_rx_dma_buffer[4] == 0x4F &&
        s_rx_dma_buffer[5] == 0x4B)
    {
        s_rx_pending_data = false;
        s_received_len = 0;
        printf("ACK 'OK' descartado imediatamente (DMA)\r\n");
				DWIN_Start_Listening();
        return;
    }

    // --- Cópia dos dados para buffer local (válido) ---
    uint8_t local_buffer[DWIN_RX_BUFFER_SIZE];
    uint16_t local_len;

    __disable_irq();
    local_len = s_received_len;
    memcpy(local_buffer, s_rx_dma_buffer, local_len);
    s_rx_pending_data = false;
    s_received_len = 0;
    __enable_irq();
		
		DWIN_Start_Listening();
		memset(s_rx_dma_buffer, 0, DWIN_RX_BUFFER_SIZE);

    // --- Validação e encaminhamento ---
    if (local_len >= 4 &&
				local_buffer[0] == 0x5A &&
				local_buffer[1] == 0xA5)
		{
				uint8_t payload_len = local_buffer[2];
				uint8_t declared_len = 3 + payload_len;

				if (local_len >= declared_len)
				{
						// OK: pacote contém pelo menos o necessário (possui padding extra? tudo bem)
						if (s_rx_callback != NULL) {
								s_rx_callback(local_buffer, declared_len);  // Só passa o necessário
						}
				}
				else
				{
						printf("Pacote truncado: recebido=%d, esperado (min)=%d\r\n", local_len, declared_len);
				}
		}
		else
		{
				printf("Pacote invalido ou sem prefixo esperado - descartado (tamanho: %d): ", local_len);
				for (uint16_t i = 0; i < local_len; i++) {
						printf("%02X ", local_buffer[i]);
				}
				printf("\r\n");
		}
}

/**
 * @brief (V8.1) "Bomba" de TX do DWIN (chamada no super-loop).
 * Gerencia o envio do FIFO de S/W para o DMA.
 */
void DWIN_TX_Pump(void)
{
    // Se DMA está ocupado OU o FIFO está vazio, não faz nada.
    if (s_dma_tx_busy || (s_tx_fifo_head == s_tx_fifo_tail)) {
        return;
    }

    // --- Seção Crítica --- 
    // Protege contra a ISR de TX Cplt (que mexe no s_dma_tx_busy)
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);
    
    if (s_dma_tx_busy) // Dupla verificação (safety check)
    {
        HAL_NVIC_EnableIRQ(USART2_IRQn);
        HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);
        return;
    }
    
    // Marca como ocupado ANTES de preparar o buffer
    s_dma_tx_busy = true;
    
    // Prepara o buffer de DMA (linear) a partir do nosso FIFO (circular)
    uint16_t bytes_to_send = 0;
    while ((s_tx_fifo_tail != s_tx_fifo_head) && (bytes_to_send < DWIN_TX_DMA_BUFFER_SIZE))
    {
        s_tx_dma_buffer[bytes_to_send] = s_tx_fifo[s_tx_fifo_tail];
        s_tx_fifo_tail = (s_tx_fifo_tail + 1) % DWIN_TX_FIFO_SIZE;
        bytes_to_send++;
    }

    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);
    // --- Fim da Seção Crítica ---

    // Inicia a transmissão DMA (CPU está livre).
    // HAL_UART_TxCpltCallback será chamado pelo Handler da ISR do DMA quando terminar.
    if (HAL_UART_Transmit_DMA(s_huart, s_tx_dma_buffer, bytes_to_send) != HAL_OK)
    {
        s_dma_tx_busy = false; // Falha no DMA, libera a flag para o Pump tentar de novo
    }
}

//================================================================================
// Funções de Controle de Estado de TX (Refatoradas para DMA FIFO)
//================================================================================

/**
 * @brief Adiciona uma sequência de bytes ao FIFO de TX. Função ATÔMICA.
 * (Corrigido V8.1: Usa bloqueio NVIC específico em vez de __disable_irq() global)
 */
static void DWIN_TX_Queue_Send_Bytes(const uint8_t* data, uint16_t size)
{
    if (data == NULL || size == 0) return;

    // Seção Crítica Específica (impede que a ISR de TX e as IRQs da UART2 colidam)
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    HAL_NVIC_DisableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);

    uint16_t free_space;
    if (s_tx_fifo_head >= s_tx_fifo_tail) {
        free_space = DWIN_TX_FIFO_SIZE - (s_tx_fifo_head - s_tx_fifo_tail) - 1;
    } else {
        free_space = (s_tx_fifo_tail - s_tx_fifo_head) - 1;
    }

    if (size > free_space) {
        // Buffer cheio. Comando descartado. (Poderíamos logar isso no CLI)
        HAL_NVIC_EnableIRQ(USART2_IRQn);
        HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);
        return;
    }

    // Copia o frame inteiro para o FIFO
    for (uint16_t i = 0; i < size; i++) {
        s_tx_fifo[s_tx_fifo_head] = data[i];
        s_tx_fifo_head = (s_tx_fifo_head + 1) % DWIN_TX_FIFO_SIZE;
    }

    // Reabilita IRQs
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_EnableIRQ(DMAMUX1_DMA1_CH4_5_IRQn);
    
    // NÃO inicia o DMA daqui. O DWIN_TX_Pump() no main loop é o único mestre do DMA.
}


bool DWIN_Driver_IsTxBusy(void) {
    // Está "ocupado" se o DMA estiver ativo OU se o FIFO de S/W ainda tiver dados para a bomba pegar.
    return (s_dma_tx_busy || (s_tx_fifo_head != s_tx_fifo_tail));
}

//================================================================================
// Handlers de Callbacks da ISR (Chamados pelo HAL)
//================================================================================

/**
 * @brief (Callback da ISR de TX) Chamado por HAL_UART_TxCpltCallback (ISR Context DMA).
 * APENAS libera a flag. O Pump no main loop fará o resto.
 */
void DWIN_Driver_HandleTxCplt(UART_HandleTypeDef *huart)
{
    s_dma_tx_busy = false; // O DMA está livre.
}


/**
 * @brief (Callback da ISR de RX) Chamado por HAL_UARTEx_RxEventCallback (ISR Context, IDLE+DMA).
 * Esta é a lógica V6.0/V8.1 (corrige o bug da senha com debounce).
 */
void DWIN_Driver_HandleRxEvent(UART_HandleTypeDef *huart, uint16_t size)
{
    if (huart->Instance != USART2) return;

    if (size > 0 && size <= DWIN_RX_BUFFER_SIZE) {
        s_received_len = size;
        s_rx_pending_data = true;           
        s_last_rx_event_tick = HAL_GetTick(); 
    }
}


/**
 * @brief Manipulador de erros da UART (Chamado por HAL_UART_ErrorCallback).
 */
void DWIN_Driver_HandleError(UART_HandleTypeDef *huart) {
    // Apenas limpa os flags de erro
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_NEF | UART_CLEAR_FEF);

    // NÃO define s_rx_needs_reset aqui.
    // Em vez disso, inicia o timer de cooldown.
    s_rx_error_cooldown_tick = HAL_GetTick(); 
    s_rx_needs_reset = false; // Garante que o reset não seja acionado
    s_rx_pending_data = false; // Descarta dados
}


//================================================================================
// Funções de Escrita (API Pública) - (V8.1)
//================================================================================

void DWIN_Driver_SetScreen(uint16_t screen_id) {
    const uint16_t VP_ADDR_PIC_ID = 0x0084;
    uint8_t cmd_buffer[] = { 
        0x5A, 0xA5, 0x07, 0x82,
        (uint8_t)(VP_ADDR_PIC_ID >> 8), (uint8_t)(VP_ADDR_PIC_ID & 0xFF),
        0x5A, 0x01,
        (uint8_t)(screen_id >> 8), (uint8_t)(screen_id & 0xFF)
    };
    DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

void DWIN_Driver_WriteInt(uint16_t vp_address, int16_t value) {
    uint8_t cmd_buffer[] = { 
        0x5A, 0xA5, 0x05, 0x82,
        (uint8_t)(vp_address >> 8), (uint8_t)(vp_address & 0xFF),
        (uint8_t)(value >> 8), (uint8_t)(value & 0xFF)
    };
    DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

void DWIN_Driver_WriteInt32(uint16_t vp_address, int32_t value) {
    uint8_t cmd_buffer[] = { 
        0x5A, 0xA5, 0x07, 0x82,
        (uint8_t)(vp_address >> 8), (uint8_t)(vp_address & 0xFF),
        (uint8_t)((value >> 24) & 0xFF), (uint8_t)((value >> 16) & 0xFF),
        (uint8_t)((value >> 8) & 0xFF),  (uint8_t)(value & 0xFF)
    };
    DWIN_TX_Queue_Send_Bytes(cmd_buffer, sizeof(cmd_buffer));
}

void DWIN_Driver_WriteString(uint16_t vp_address, const char* text, uint16_t max_len) {
    if (s_huart == NULL || text == NULL || max_len == 0) return;
    size_t text_len = strlen(text);
    if (text_len > max_len) text_len = max_len; 
    uint8_t frame_payload_len = 3 + text_len; 
    uint16_t total_frame_size = 3 + frame_payload_len; 
    uint8_t temp_frame_buffer[total_frame_size];
    temp_frame_buffer[0] = 0x5A;
    temp_frame_buffer[1] = 0xA5;
    temp_frame_buffer[2] = frame_payload_len;
    temp_frame_buffer[3] = 0x82;
    temp_frame_buffer[4] = (uint8_t)(vp_address >> 8);
    temp_frame_buffer[5] = (uint8_t)(vp_address & 0xFF);
    memcpy(&temp_frame_buffer[6], text, text_len);
    DWIN_TX_Queue_Send_Bytes(temp_frame_buffer, total_frame_size);
}

void DWIN_Driver_WriteRawBytes(const uint8_t* data, uint16_t size) {
    if (s_huart != NULL && data != NULL && size > 0) {
        DWIN_TX_Queue_Send_Bytes(data, size);
    }
}