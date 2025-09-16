/*******************************************************************************
 * @file        eeprom_driver.c
 * @brief       Driver N�O-BLOQUEANTE para EEPROM I2C (AT24C series).
 * @version     8.2 (Refatorado por Dev STM - Arquitetura Ass�ncrona DMA)
 * @details     Fornece duas APIs:
 * 1. Read_Blocking: Para uso no Boot (carregamento de config).
 * 2. Write_Async: API de FSM n�o-bloqueante para uso no superloop.
 * Usa DMA I2C e um timer de software interno para o atraso de p�gina de 5ms.
 ******************************************************************************/

#include "eeprom_driver.h"
#include "stm32c0xx_hal_i2c.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>

//==============================================================================
// Defini��es de Hardware da EEPROM
//==============================================================================

#define EEPROM_I2C_ADDR         (0x50 << 1)   // Endere�o I2C da EEPROM
#define EEPROM_I2C_TIMEOUT_MS   100           // Timeout para leituras de boot

//==============================================================================
// Vari�veis Est�ticas (File-Scope)
//==============================================================================

static I2C_HandleTypeDef *s_i2c_handle = NULL;

// Vari�veis de estado da FSM de Escrita Ass�ncrona
typedef enum {
    ASYNC_IDLE,
    ASYNC_WRITING_PAGE,     // DMA I2C est� transferindo dados
    ASYNC_WAIT_PAGE_DELAY   // DMA terminou, esperando 5ms para a EEPROM escrever internamente
} AsyncWriteState_t;

static struct {
    AsyncWriteState_t   state;
    const uint8_t* p_data;             // Ponteiro para o buffer de dados total (ex: a struct de config)
    uint16_t            total_size;         // Tamanho total a ser escrito
    uint16_t            current_addr;       // Endere�o de mem�ria EEPROM atual
    uint16_t            bytes_remaining;    // Bytes restantes a serem escritos
    uint32_t            page_delay_start_tick; // Tick de in�cio para o delay de 5ms
} s_fsm;

// Flags de ISR para DMA
static volatile bool s_i2c_dma_tx_cplt = false;
static volatile bool s_i2c_error = false;

//==============================================================================
// Fun��es de Inicializa��o e Verifica��o de Status
//==============================================================================

void EEPROM_Driver_Init(I2C_HandleTypeDef *hi2c)
{
    s_i2c_handle = hi2c;
    s_fsm.state = ASYNC_IDLE;
    s_fsm.p_data = NULL;
    s_i2c_dma_tx_cplt = false;
    s_i2c_error = false;
}

// Verifica se a EEPROM est� presente (fun��o bloqueante, OK para boot)
bool EEPROM_Driver_IsReady(void)
{
    if (s_i2c_handle == NULL) return false;
    return (HAL_I2C_IsDeviceReady(s_i2c_handle, EEPROM_I2C_ADDR, 2, EEPROM_I2C_TIMEOUT) == HAL_OK);
}

// Retorna true se a FSM de escrita ass�ncrona estiver ocupada.
bool EEPROM_Driver_IsBusy(void)
{
    return (s_fsm.state != ASYNC_IDLE);
}

//==============================================================================
// API de Leitura (Bloqueante - Apenas para Boot)
//==============================================================================

// L� um bloco de dados (BLOQUEANTE). Usado SOMENTE pelo gerenciador de config no BOOT.
bool EEPROM_Driver_Read_Blocking(uint16_t addr, uint8_t *data, uint16_t size)
{
    if (s_i2c_handle == NULL || data == NULL) return false;

    // Garante que o dispositivo esteja pronto antes de ler
    if (!EEPROM_Driver_IsReady()) {
        printf("EEPROM_Read: Dispositivo nao esta pronto.\r\n");
        return false;
    }

    return (HAL_I2C_Mem_Read(s_i2c_handle, EEPROM_I2C_ADDR, addr, I2C_MEMADD_SIZE_16BIT, data, size, EEPROM_I2C_TIMEOUT) == HAL_OK);
}

//==============================================================================
// API de Escrita (ASS�NCRONA - V8.2)
//==============================================================================

/**
 * @brief Inicia uma sequ�ncia de escrita ass�ncrona de grande volume.
 */
bool EEPROM_Driver_Write_Async_Start(uint16_t addr, const uint8_t *data, uint16_t size)
{
    if (EEPROM_Driver_IsBusy() || s_i2c_handle == NULL || data == NULL || size == 0)
    {
        return false; // Driver j� est� ocupado com uma escrita anterior
    }

    // Configura os ponteiros da FSM
    s_fsm.p_data = data;
    s_fsm.total_size = size;
    s_fsm.current_addr = addr;
    s_fsm.bytes_remaining = size;
    s_i2c_error = false;
    s_i2c_dma_tx_cplt = false;

    // Inicia o primeiro bloco
    s_fsm.state = ASYNC_WRITING_PAGE;
    
    // Calcula o tamanho do primeiro bloco (chunk) para caber na p�gina da EEPROM
    uint16_t chunk_size = EEPROM_PAGE_SIZE - (s_fsm.current_addr % EEPROM_PAGE_SIZE);
    if (chunk_size > s_fsm.bytes_remaining)
    {
        chunk_size = s_fsm.bytes_remaining;
    }

    // Inicia a primeira escrita DMA
    if (HAL_I2C_Mem_Write_DMA(s_i2c_handle, EEPROM_I2C_ADDR, s_fsm.current_addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)s_fsm.p_data, chunk_size) != HAL_OK)
    {
        s_fsm.state = ASYNC_IDLE; // Falha ao iniciar DMA
        return false;
    }

    return true; // Escrita iniciada, a FSM assume a partir daqui
}

/**
 * @brief (FSM Poll) Processa o pr�ximo passo da escrita ass�ncrona.
 * Deve ser chamado repetidamente pelo superloop (via Gerenciador_Config_Run_FSM).
 * @return true se a sequ�ncia completa de escrita terminou, false se ainda est� ocupada.
 */
bool EEPROM_Driver_Write_Async_Poll(void)
{
    if (s_fsm.state == ASYNC_IDLE)
    {
        return true; // N�o estava fazendo nada
    }

    // Verifica erros de I2C/DMA (sinalizados pela ISR de Erro)
    if (s_i2c_error)
    {
        printf("EEPROM ASYNC: Erro de I2C/DMA detectado!\r\n");
        HAL_I2C_Master_Abort_IT(s_i2c_handle, EEPROM_I2C_ADDR);
        s_fsm.state = ASYNC_IDLE; // Aborta a FSM
        s_i2c_error = false;
        return true; // Sinaliza "terminado" (com falha)
    }

    switch (s_fsm.state)
    {
        case ASYNC_WRITING_PAGE:
            // Espera pelo flag da ISR de DMA TC (Transfer Complete)
            if (s_i2c_dma_tx_cplt)
            {
                s_i2c_dma_tx_cplt = false; // Consome o flag
                s_fsm.state = ASYNC_WAIT_PAGE_DELAY;
                s_fsm.page_delay_start_tick = HAL_GetTick(); // Inicia o timer de software de 5ms
            }
            break;

        case ASYNC_WAIT_PAGE_DELAY:
            // Espera N�O-BLOQUEANTE pelo tempo de escrita da p�gina (5ms)
            if (HAL_GetTick() - s_fsm.page_delay_start_tick >= EEPROM_WRITE_TIME_MS)
            {
                // Delay terminou. Atualiza ponteiros.
                uint16_t last_chunk_size = EEPROM_PAGE_SIZE - (s_fsm.current_addr % EEPROM_PAGE_SIZE);
                if (last_chunk_size > s_fsm.bytes_remaining)
                {
                    last_chunk_size = s_fsm.bytes_remaining;
                }
                
                s_fsm.current_addr += last_chunk_size;
                s_fsm.p_data += last_chunk_size;
                s_fsm.bytes_remaining -= last_chunk_size;

                // Verifica se terminamos
                if (s_fsm.bytes_remaining == 0)
                {
                    // ------- SEQU�NCIA COMPLETA --------
                    s_fsm.state = ASYNC_IDLE;
                    return true; // SINALIZA CONCLUS�O
                }
                else
                {
                    // Ainda h� dados, envia a pr�xima p�gina
                    uint16_t next_chunk_size = (s_fsm.bytes_remaining > EEPROM_PAGE_SIZE) ? EEPROM_PAGE_SIZE : s_fsm.bytes_remaining;
                    
                    s_fsm.state = ASYNC_WRITING_PAGE;
                    if (HAL_I2C_Mem_Write_DMA(s_i2c_handle, EEPROM_I2C_ADDR, s_fsm.current_addr, I2C_MEMADD_SIZE_16BIT, (uint8_t*)s_fsm.p_data, next_chunk_size) != HAL_OK)
                    {
                         printf("EEPROM ASYNC: Falha ao iniciar DMA da proxima pagina!\r\n");
                         s_i2c_error = true; // A FSM tratar� disso no pr�ximo ciclo
                    }
                }
            }
            break;

        case ASYNC_IDLE:
        default:
            return true;
    }

    return false; // Ainda ocupado
}

//==============================================================================
// Handlers de Callbacks da ISR (Chamados pelo HAL)
//==============================================================================

// Coloque em stm32c0xx_it.c -> HAL_I2C_MasterTxCpltCallback
void EEPROM_Driver_HandleTxCplt(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        s_i2c_dma_tx_cplt = true;
    }
}

// Coloque em stm32c0xx_it.c -> HAL_I2C_ErrorCallback
void EEPROM_Driver_HandleError(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        s_i2c_error = true;
    }
}