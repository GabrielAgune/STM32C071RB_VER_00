/*******************************************************************************
 * @file        app_manager.c
 * @brief       Gerenciador central da aplicação com arquitetura de tarefas.
 * @version     2.0 (Refatorado para multitarefa cooperativa explícita)
 * @details     Este módulo orquestra a inicialização e o processamento de
 * todas as outras partes do sistema de forma não-bloqueante.
 ******************************************************************************/

#include "app_manager.h"
#include "rtc_driver.h"
#include <stdio.h>
#include <string.h>

//================================================================================
// Variáveis de Estado Globais do Módulo
//================================================================================
static ScaleFilter s_scale_filter;
static ScaleFilterOut s_scale_output;
static FreqData_t s_freq_data;
static float s_temperatura_mcu = 0.0f;

//================================================================================
// Protótipos das Tarefas (Funções Privadas)
//================================================================================
static void Task_Handle_High_Frequency_Polling(void);
static void Task_Handle_Scale(void);
static void Task_Handle_Frequency(void);
static void Task_Handle_Temperature(void);
static void Task_Handle_UI_Periodic(void);

//================================================================================
// Implementação da Função de Inicialização
//================================================================================
void App_Manager_Init(void)
{
    // Drivers
    CLI_Init(&huart1);
		printf("Sistema Integrado - Log de Inicializacao:\r\n");
    printf("1. CLI/Debug UART... OK\r\n");
		
		EEPROM_Driver_Init(&hi2c1);
    RTC_Driver_Init();
		printf("2. Drivers I2C e RTC... OK\r\n");
    
    
    // Módulos de Lógica
    Gerenciador_Config_Init(&hcrc);
    printf("3. Gerenciador de Configuracoes... ");
    if (!Gerenciador_Config_Validar_e_Restaurar()) {
        // Se a escrita falhar, a mensagem de erro já é impressa dentro do módulo
        printf("[FALHA]\r\nERRO FATAL: Nao foi possivel carregar/restaurar configuracoes.\r\n");
        // O sistema continua, mas pode não funcionar como esperado.
    } else {
        printf("[OK]\r\n");
    }
		
		// Módulos de Hardware
    ADS1232_Init();
    Frequency_Init();
    Servos_Init();
		printf("4. Modulos de Hardware (ADC, Servos, Frequencia)... OK\r\n");
		
    
    // Inicializa o filtro da balança com uma tara inicial
    printf("5. Executando tara da balanca (pode demorar alguns segundos)...\r\n");
    int32_t offset_inicial = ADS1232_Tare(); // Esta função é bloqueante
    ScaleFilter_Init(&s_scale_filter, offset_inicial);
    printf("   ... Tara concluida.\r\n");
    
		s_temperatura_mcu = TempSensor_GetTemperature();
    printf("Temperatura inicial: %.2f C\r\n", s_temperatura_mcu);
    // A UI gere a sua própria inicialização não-bloqueante
    UI_Init();

    printf("6. Interface de Usuario... Iniciando sequencia de splash.\r\n");

    printf("\r\n>>> INICIALIZACAO COMPLETA <<<\r\n\r\n");
}

//================================================================================
// Implementação do Despachante de Tarefas (Super-Loop)
//================================================================================
void App_Manager_Process(void)
{
    // O nosso super-loop agora apenas chama as tarefas.
    // Cada tarefa é responsável por gerir o seu próprio tempo.
    Task_Handle_High_Frequency_Polling();
    Task_Handle_Scale();
    Task_Handle_Frequency();
    Task_Handle_UI_Periodic();
}

//================================================================================
// Implementação das Tarefas
//================================================================================

/**
 * @brief Tarefa para módulos que precisam ser chamados em todas as iterações do loop.
 */
static void Task_Handle_High_Frequency_Polling(void)
{
    DWIN_Driver_Process(); // Processa bytes recebidos da UART do display
    CLI_Process();         // Processa comandos da interface de linha de comando
    Servos_Process();      // Atualiza a máquina de estados dos servos
    UI_Process();          // Executa a máquina de estados da UI (splash screen, etc.)
}

/**
 * @brief Tarefa que gere a lógica da balança. Executa a cada 100ms.
 */
static void Task_Handle_Scale(void)
{
    static uint32_t ultimo_tick = 0;
    if (HAL_GetTick() - ultimo_tick >= 100)
    {
        ultimo_tick = HAL_GetTick();
        
        int32_t leitura_adc = ADS1232_Read_Median_of_3();
        ScaleFilter_Push(&s_scale_filter, leitura_adc, &s_scale_output);

        // TODO: Implementar máquina de estados da balança (estável, instável, auto-zero)
        
        /*printf("Peso: %.2f g | Estavel: %d | Sigma: %.2f\r\n", 
               s_scale_output.avg_grams, 
               s_scale_output.is_stable,
               s_scale_output.sigma_grams);*/
    }
}

/**
 * @brief Tarefa que gere a medição de frequência e temperatura. Executa a cada 500ms.
 */
static void Task_Handle_Frequency(void)
{
    static uint32_t ultimo_tick = 0;
    if (HAL_GetTick() - ultimo_tick >= 500)
    {
        ultimo_tick = HAL_GetTick();
        
        uint32_t contagem_pulsos = Frequency_Get_Pulse_Count();
        Frequency_Reset();



        float escala_a = 0.0f;
        if (s_temperatura_mcu > 0) {
            escala_a = (float)contagem_pulsos / s_temperatura_mcu;
        }
        
        /*printf("Pulsos/500ms: %lu | Temp: %.2f C | Escala A: %.2f\r\n",
               (unsigned long)contagem_pulsos, temperatura, escala_a);*/
    }
}

/**
 * @brief Tarefa que atualiza periodicamente a UI. Executa a cada 1 segundo.
 */
static void Task_Handle_UI_Periodic(void)
{
    static uint32_t ultimo_tick = 0;
    if (HAL_GetTick() - ultimo_tick >= 1000)
    {
        ultimo_tick = HAL_GetTick();
        UI_Update_Periodic();
    }
}

static void Task_Handle_Temperature(void) {
    static uint32_t ultimo_tick = 0;
    // Lê a temperatura a uma frequência diferente para não sobrecarregar o ADC
    if (HAL_GetTick() - ultimo_tick >= 1000) { 
        ultimo_tick = HAL_GetTick();
        s_temperatura_mcu = TempSensor_GetTemperature();
    }
}

//================================================================================
// Implementação dos Handlers (chamados pela UI)
//================================================================================
void App_Manager_Handle_Start_Process(void) {
    printf("APP: Comando para iniciar processo recebido.\r\n");
    Servos_Start_Sequence();
}

void App_Manager_Handle_Password_Input(const char* password) {
    char senha_armazenada[MAX_SENHA_LEN + 1];
    if (Gerenciador_Config_Get_Senha(senha_armazenada, sizeof(senha_armazenada))) {
        if (strcmp(password, senha_armazenada) == 0) {
            UI_On_Auth_Success();
        } else {
            UI_On_Auth_Failure();
        }
    }
}

void App_Manager_Handle_New_Password(const char* new_password) {
    Gerenciador_Config_Set_Senha(new_password);
    printf("APP: Nova senha definida.\r\n");
}

void App_Manager_Handle_DateTime_Input(uint8_t d, uint8_t m, uint8_t y, uint8_t h, uint8_t min, uint8_t s) {
    RTC_TimeTypeDef sTime = { .Hours = h, .Minutes = min, .Seconds = s };
    RTC_DateTypeDef sDate = { .Date = d, .Month = m, .Year = y };
    RTC_Driver_SetDateTime(&sTime, &sDate);
    printf("APP: Data e Hora atualizadas.\r\n");
}

void App_Manager_GetScaleData(ScaleFilterOut* data) {
    if (data != NULL) {
        *data = s_scale_output; // Copia os dados mais recentes
    }
}

void App_Manager_GetFreqData(FreqData_t* data) {
    if (data != NULL) {
        *data = s_freq_data; // Copia os dados mais recentes
    }
}

float App_Manager_GetTemperature(void) {
    return TempSensor_GetTemperature();
}