/*******************************************************************************
 * @file        app_manager.c
 * @brief       Gerenciador central da aplica��o com arquitetura de tarefas.
 * @version     2.0 (Refatorado para multitarefa cooperativa expl�cita)
 * @details     Este m�dulo orquestra a inicializa��o e o processamento de
 * todas as outras partes do sistema de forma n�o-bloqueante.
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
// Prot�tipos das Tarefas (Fun��es Privadas)
//================================================================================
static void Task_Handle_High_Frequency_Polling(void);
static void Task_Handle_Scale(void);
static void Task_Handle_Frequency(void);
static void Task_Handle_Temperature(void);


//================================================================================
// Implementa��o da Fun��o de Inicializa��o
//================================================================================
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
        // Se a escrita falhar, a mensagem de erro j� � impressa dentro do m�dulo
        printf("[FALHA]\r\nERRO FATAL: Nao foi possivel carregar/restaurar configuracoes.\r\n");
        // O sistema continua, mas pode n�o funcionar como esperado.
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
    int32_t offset_inicial = ADS1232_Tare(); // Esta fun��o � bloqueante
    ScaleFilter_Init(&s_scale_filter, offset_inicial);
    printf("   ... Tara concluida.\r\n");
    
		s_temperatura_mcu = TempSensor_GetTemperature();
    printf("Temperatura inicial: %.2f C\r\n", s_temperatura_mcu);
		
		DWIN_Driver_Init(&huart2, Controller_DwinCallback);

    printf("6. Interface de Usuario... Iniciando sequencia de splash.\r\n");

    printf("\r\n>>> INICIALIZACAO COMPLETA <<<\r\n\r\n");
}

//================================================================================
// Implementa��o do Despachante de Tarefas (Super-Loop)
//================================================================================
void App_Manager_Process(void)
{
    Task_Handle_High_Frequency_Polling();
    Task_Handle_Scale();
    Task_Handle_Frequency();
		Task_Handle_Temperature();
    RTC_Driver_Process();
}

//================================================================================
// Implementa��o das Tarefas
//================================================================================

/**
 * @brief Tarefa para m�dulos que precisam ser chamados em todas as itera��es do loop.
 */
static void Task_Handle_High_Frequency_Polling(void)
{
    DWIN_Driver_Process(); // Processa bytes recebidos da UART do display
    CLI_Process();         // Processa comandos da interface de linha de comando
    Servos_Process();      // Atualiza a m�quina de estados dos servos
    Process_Controller();         // Executa a m�quina de estados da UI (splash screen, etc.)
}

/**
 * @brief Tarefa que gere a l�gica da balan�a. Executa a cada 100ms.
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
 * @brief Tarefa que gere a medi��o de frequ�ncia e temperatura. Executa a cada 500ms.
 */
static void Task_Handle_Frequency(void)
{
    static uint32_t ultimo_tick = 0;
    if (HAL_GetTick() - ultimo_tick >= 1000)
    {
        ultimo_tick = HAL_GetTick();
        

        uint32_t contagem_pulsos = Frequency_Get_Pulse_Count();
        Frequency_Reset(); 

        float escala_a = 0.0f;
        if (s_temperatura_mcu > 0) {
            escala_a = Calcular_Escala_A(contagem_pulsos);
        }

        s_freq_data.pulsos = contagem_pulsos;
        s_freq_data.escala_a = escala_a;
				
        int32_t frequencia_para_dwin = (int32_t)((s_freq_data.pulsos / 1000.0f) * 10.0f);
        DWIN_Driver_WriteInt32(FREQUENCIA, frequencia_para_dwin);

        int32_t escala_a_para_dwin = (int32_t)(s_freq_data.escala_a * 10.0f);
        DWIN_Driver_WriteInt32(ESCALA_A, escala_a_para_dwin);
        
	}
}


static void Task_Handle_Temperature(void) {
    static uint32_t ultimo_tick = 0;
    if (HAL_GetTick() - ultimo_tick >= 1000) { 
        ultimo_tick = HAL_GetTick();
        s_temperatura_mcu = TempSensor_GetTemperature();

        // Converte o float para um inteiro com uma casa decimal (ex: 25.7f se torna 257)
        int16_t temperatura_para_dwin = (int16_t)(s_temperatura_mcu * 10.0f);
        
        // Envia o valor inteiro para o display
        DWIN_Driver_WriteInt(TEMP_SAMPLE, temperatura_para_dwin);
    }
}

//================================================================
// Implementa��o dos Handlers (chamados pela UI)
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
    return TempSensor_GetTemperature();
}