/*******************************************************************************
 * @file        app_manager.c
 * @brief       Gerenciador central da aplica��o com arquitetura de tarefas.
 * @version     2.1 (Refatorado para FSM cooperativa de TX de display por Dev STM)
 * @details     Este m�dulo orquestra a inicializa��o e o processamento de
 * todas as outras partes do sistema de forma n�o-bloqueante.
 * A atualiza��o de dados do display (Freq, Temp, Escala) � feita por uma
 * m�quina de estados (FSM) para serializar comandos TX ass�ncronos.
 ******************************************************************************/

#include "app_manager.h"
#include "rtc_driver.h"
#include "dwin_driver.h"
#include "controller.h"
#include <stdio.h>
#include <string.h>

//================================================================================
// Vari�veis de Estado Globais do M�dulo
//================================================================================
static ScaleFilter s_scale_filter;
static ScaleFilterOut s_scale_output;
static FreqData_t s_freq_data;
static float s_temperatura_mcu = 0.0f;

//================================================================================
// Defini��es da FSM de Atualiza��o do Display (NOVO)
//================================================================================

/**
 * @brief Estados da m�quina de estados (FSM) de atualiza��o do display.
 * Gerencia o envio sequencial e n�o-bloqueante de dados para a UART.
 */
typedef enum {
    TASK_DISPLAY_IDLE,              // 0: Ocioso, aguardando timer de 1000ms
    TASK_DISPLAY_CALC_ALL,          // 1: Timer estourou, calcular todos os valores (Temp, Freq, Escala A)
    TASK_DISPLAY_SEND_FREQ,         // 2: Aguardando UART livre para enviar Frequ�ncia
    TASK_DISPLAY_SEND_ESCALA_A,     // 3: Aguardando UART livre para enviar Escala A
    TASK_DISPLAY_SEND_TEMP          // 4: Aguardando UART livre para enviar Temperatura
} TaskDisplay_State_t;

static TaskDisplay_State_t s_display_state = TASK_DISPLAY_IDLE;
static uint32_t s_display_last_tick = 0;
static const uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000;

//================================================================================
// Prot�tipos das Tarefas (Fun��es Privadas)
//================================================================================

static void Task_Handle_High_Frequency_Polling(void);
static void Task_Handle_Scale(void);
static void Task_Update_Display_FSM(void); // NOVA FSM (substitui Freq e Temp)

// Prot�tipo da fun��o auxiliar (movida para cima)
static float Calcular_Escala_A(uint32_t frequencia_hz);


//================================================================================
// Implementa��o da Fun��o de Inicializa��o
//================================================================================

// ... (Fun��o App_Manager_Init() permanece exatamente como no seu original) ...
void App_Manager_Init(void)
{
    // Drivers
    CLI_Init(&huart1);
		printf("Sistema Integrado - Log de Inicializacao:\r\n");
    printf("1. CLI/Debug UART... OK\r\n");
		
		EEPROM_Driver_Init(&hi2c1);
    RTC_Driver_Init(&hrtc);
		printf("2. Drivers I2C e RTC... OK\r\n");
    
    
    // M�dulos de L�gica
    Gerenciador_Config_Init(&hcrc);
    printf("3. Gerenciador de Configuracoes... ");
    if (!Gerenciador_Config_Validar_e_Restaurar()) {
        printf("[FALHA]\r\nERRO FATAL: Nao foi possivel carregar/restaurar configuracoes.\r\n");
    } else {
        printf("[OK]\r\n");
    }
		
		// M�dulos de Hardware
    ADS1232_Init();
    Frequency_Init();
    Servos_Init();
		printf("4. Modulos de Hardware (ADC, Servos, Frequencia)... OK\r\n");
		
    
    // Inicializa o filtro da balan�a com uma tara inicial
    printf("5. Executando tara da balanca (pode demorar alguns segundos)...\r\n");
    int32_t offset_inicial = ADS1232_Tare(); // Esta fun��o � bloqueante (OK, � na INIT)
    ScaleFilter_Init(&s_scale_filter, offset_inicial);
    printf("   ... Tara concluida.\r\n");
    
		s_temperatura_mcu = TempSensor_GetTemperature(); // Leitura inicial
    printf("Temperatura inicial: %.2f C\r\n", s_temperatura_mcu);
		
		DWIN_Driver_Init(&huart2, Controller_DwinCallback);

    printf("6. Interface de Usuario... Iniciando sequencia de splash.\r\n");

    printf("\r\n>>> INICIALIZACAO COMPLETA <<<\r\n\r\n");
}


//================================================================================
// Implementa��o do Despachante de Tarefas (Super-Loop) - REATORADO
//================================================================================

/**
 * @brief Loop de processo principal. Chama todas as tarefas cooperativas.
 * Esta fun��o NUNCA deve bloquear.
 */
void App_Manager_Process(void)
{
    // 1. Tarefas de alta frequ�ncia (processamento de buffers de ISR e l�gica r�pida)
    Task_Handle_High_Frequency_Polling();
    
    // 2. Tarefa da Balan�a (leitura de ADC) - 100ms
    Task_Handle_Scale();
    
    // 3. FSM de Atualiza��o de Display (Freq, Escala A, Temp) - 1000ms
    Task_Update_Display_FSM();
    
    // 4. Outras tarefas peri�dicas (ex: RTC)
    RTC_Driver_Process();
}

//================================================================================
// Implementa��o das Tarefas
//================================================================================

/**
 * @brief Tarefa para m�dulos que precisam ser chamados em todas as itera��es do loop.
 * (Fun��o original mantida)
 */
static void Task_Handle_High_Frequency_Polling(void)
{
    DWIN_Driver_Process(); // Processa bytes recebidos da UART do display (RX)
    CLI_Process();         // Processa comandos da interface de linha de comando (UART1 RX)
    Servos_Process();      // Atualiza a m�quina de estados dos servos
    Process_Controller();  // Executa a m�quina de estados da UI (splash screen, etc.)
}

/**
 * @brief Tarefa que gere a l�gica da balan�a. Executa a cada 100ms.
 * (Fun��o original mantida - esta tarefa n�o usa TX, � segura)
 */
static void Task_Handle_Scale(void)
{
    static uint32_t ultimo_tick = 0;
    if (HAL_GetTick() - ultimo_tick >= 100)
    {
        ultimo_tick = HAL_GetTick();
        
        int32_t leitura_adc = ADS1232_Read_Median_of_3();
        ScaleFilter_Push(&s_scale_filter, leitura_adc, &s_scale_output);

    }
}

/**
 * @brief Fun��o auxiliar para c�lculo (Fun��o original mantida)
 */
static float Calcular_Escala_A(uint32_t frequencia_hz)
{
    float escala_a;
    float freq_corr = (float)frequencia_hz; 
    escala_a = (-0.00014955f * freq_corr) + 396.85f;

    float gain = 1.0f;
    float zero = 0.0f;
    Gerenciador_Config_Get_Cal_A(&gain, &zero);
    escala_a = (escala_a * gain) + zero;
    
    return escala_a;
}


/**
 * @brief [NOVA FSM] Tarefa de atualiza��o dos VPs do Display (Freq, Escala, Temp).
 * Substitui as antigas Task_Handle_Frequency() e Task_Handle_Temperature().
 * Esta FSM garante que os comandos UART TX sejam enviados um de cada vez,
 * cooperando com o driver DWIN e nunca bloqueando o loop principal.
 */
static void Task_Update_Display_FSM(void)
{
    if (s_display_state == TASK_DISPLAY_IDLE)
    {
        if (HAL_GetTick() - s_display_last_tick < DISPLAY_UPDATE_INTERVAL_MS)
        {
            return; // Ainda n�o � hora
        }

        if (DWIN_Driver_IsTxBusy())
        {
             s_display_last_tick = HAL_GetTick(); // Reinicia o timer mesmo se pulamos o ciclo
             return; // Driver ocupado com outra coisa, n�o sobrecarregue.
        }
        
        // Driver livre, podemos iniciar nosso ciclo.
        s_display_last_tick = HAL_GetTick();  
        s_display_state = TASK_DISPLAY_CALC_ALL; 
    }


    if (s_display_state != TASK_DISPLAY_CALC_ALL && s_display_state != TASK_DISPLAY_IDLE)
    {
        if (DWIN_Driver_IsTxBusy())
        {
            // Esperando o comando anterior (FREQ ou ESCALA) terminar de ser enviado pela ISR
            return; 
        }
    }

    // --- Passo 3: Execu��o dos Estados da FSM ---
    switch (s_display_state)
    {
        case TASK_DISPLAY_CALC_ALL:
            // (L�gica de c�lculo permanece a mesma)
            s_freq_data.pulsos = Frequency_Get_Pulse_Count();
            Frequency_Reset(); 
            if (s_temperatura_mcu > 0) {
                s_freq_data.escala_a = Calcular_Escala_A(s_freq_data.pulsos);
            } else {
                s_freq_data.escala_a = 0.0f;
            }
            s_temperatura_mcu = TempSensor_GetTemperature();
            
            s_display_state = TASK_DISPLAY_SEND_FREQ;
            // Fall-through (proposital) para enviar o primeiro comando imediatamente

        case TASK_DISPLAY_SEND_FREQ:
        {
            int32_t frequencia_para_dwin = (int32_t)((s_freq_data.pulsos / 1000.0f) * 10.0f);
            DWIN_Driver_WriteInt32(FREQUENCIA, frequencia_para_dwin); 
            s_display_state = TASK_DISPLAY_SEND_ESCALA_A;
            break; // Retorna ao super-loop (espera o TxCplt)
        }

        case TASK_DISPLAY_SEND_ESCALA_A:
        {
            int32_t escala_a_para_dwin = (int32_t)(s_freq_data.escala_a * 10.0f);
            DWIN_Driver_WriteInt32(ESCALA_A, escala_a_para_dwin);
            s_display_state = TASK_DISPLAY_SEND_TEMP;
            break; // Retorna ao super-loop (espera o TxCplt)
        }

        case TASK_DISPLAY_SEND_TEMP:
        {
            int16_t temperatura_para_dwin = (int16_t)(s_temperatura_mcu * 10.0f);
            DWIN_Driver_WriteInt(TEMP_SAMPLE, temperatura_para_dwin);
            s_display_state = TASK_DISPLAY_IDLE; 
            break; // Retorna ao super-loop (sequ�ncia terminada)
        }
        
        case TASK_DISPLAY_IDLE:
        default:
             break; // Estado ocioso, j� tratado no Passo 1.
    }
}



/* NOTA: As fun��es antigas Task_Handle_Frequency() e Task_Handle_Temperature() 
   foram REMOVIDAS e substitu�das pela FSM Task_Update_Display_FSM() acima.
*/


//================================================================
// Implementa��o dos Handlers (chamados pela UI)
// (Fun��es originais mantidas - Sem altera��o)
//================================================================

void App_Manager_Handle_Start_Process(void) {
    printf("APP: Comando para iniciar processo recebido.\r\n");
    Servos_Start_Sequence();
}

void App_Manager_Handle_New_Password(const char* new_password) {
    Gerenciador_Config_Set_Senha(new_password);
    printf("APP: Nova senha definida.\r\n");
}

void App_Manager_GetScaleData(ScaleFilterOut* data) {
    if (data != NULL) {
        *data = s_scale_output; 
    }
}

void App_Manager_GetFreqData(FreqData_t* data) {
    if (data != NULL) {
        *data = s_freq_data; 
    }
}

float App_Manager_GetTemperature(void) {
    // Retorna o �ltimo valor lido pela FSM, em vez de ler o sensor aqui
    return s_temperatura_mcu; 
}