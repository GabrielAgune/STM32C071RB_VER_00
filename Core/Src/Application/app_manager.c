/*******************************************************************************
 * @file        app_manager.c
 * @brief       Gerenciador central da aplicação (Arquitetura V8.6 - Proposta de Otimização)
 * @version     8.6 (Refatorado por Dev STM)
 * @details     Implementa a proposta do usuário V8.6:
 * 1. Na tela do Monitor, a FSM roda a cada 1s.
 * 2. Freq/Reset são lidos a cada 1s (para cálculo correto).
 * 3. A leitura bloqueante do ADC (Temp) só ocorre a cada 5s (via sub-contador),
 * enquanto Freq/Escala A são enviados a cada 1s.
 ******************************************************************************/

#include "app_manager.h"
#include "rtc_driver.h"
#include "dwin_driver.h"   
#include "controller.h" 
#include "servo_controle.h"
#include "ads1232_driver.h"
#include "pcb_frequency.h"
#include "temp_sensor.h"
#include "gerenciador_configuracoes.h"
#include "medicao_handler.h"
#include <stdio.h>
#include <string.h>
#include <math.h>   
#include <stdbool.h>

//================================================================================
// Variáveis de Estado Globais do Módulo
//================================================================================

//static App_ScaleData_t s_scale_output; 
//static FreqData_t s_freq_data;
//static float s_temperatura_mcu = 0.0f;

extern volatile bool g_ads_data_ready; 

//================================================================================
// Definições da FSM de Atualização do Display
//================================================================================
typedef enum {
    TASK_DISPLAY_IDLE,
    TASK_DISPLAY_CHECK_SCREEN, 
} TaskDisplay_State_t;

static TaskDisplay_State_t s_display_state = TASK_DISPLAY_IDLE;
static uint32_t s_display_last_tick = 0;
static const uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000;
static uint8_t s_display_temp_counter = 0; 
static uint32_t s_freq_last_tick = 0;
static const uint32_t FREQ_UPDATE_INTERVAL_MS = 1000;

//================================================================================
// Protótipos das Tarefas (Funções Privadas)
//================================================================================
static void Task_Handle_High_Frequency_Polling(void);
static void Task_Handle_Scale(void); 
static void Task_Update_Display_FSM(void);
static void Task_Update_Frequency(void);
static void Task_Update_Clock(void);
static float Calcular_Escala_A(uint32_t frequencia_hz);
static bool Check_Stability(float new_grams); 

//================================================================================
// Implementação da Função de Inicialização
//================================================================================
void App_Manager_Init(void)
{
		// Etapa 1: Inicializa módulos essenciais de comunicação e software
    CLI_Init(&huart1);
    DWIN_Driver_Init(&huart2, Controller_DwinCallback);
    EEPROM_Driver_Init(&hi2c1);
    Gerenciador_Config_Init(&hcrc);
    RTC_Driver_Init(&hrtc);
    Medicao_Init();

    // Etapa 2: Inicializa os drivers de hardware
    Servos_Init();   
    Frequency_Init(); 
    ADS1232_Init();

    // Etapa 3: Carrega configurações (ainda é bloqueante, mas necessário no boot)
    printf("Sistema Integrado - Log de Inicializacao:\r\n");
    printf("1. Modulos de Software e Drivers... OK\r\n");

    printf("2. Gerenciador de Configuracoes... ");
    if (!Gerenciador_Config_Validar_e_Restaurar()) {
        printf("[AVISO] Restaurado com configs de fabrica.\r\n");
    } else {
        printf("[OK]\r\n");
    }

    // Pré-configura alguns valores iniciais no handler de medição
    Medicao_Set_Densidade(71.0);
    Medicao_Set_Umidade(25.73);
		char nr_serial_buffer[17]; // Cria um buffer local para receber o dado
		Gerenciador_Config_Get_Serial(nr_serial_buffer, sizeof(nr_serial_buffer));
		
		DWIN_Driver_Init(&huart2, Controller_DwinCallback);
		DWIN_Driver_WriteString(VP_HARDWARE, HARDWARE, strlen(HARDWARE));
		DWIN_Driver_WriteString(VP_FIRMWARE, FIRMWARE, strlen(FIRMWARE));
		DWIN_Driver_WriteString(VP_FIRM_IHM, FIRM_IHM, strlen(FIRM_IHM));
		DWIN_Driver_WriteString(VP_SERIAL, nr_serial_buffer, sizeof(nr_serial_buffer));
		
		printf("Enviando informacoes iniciais para o display...\r\n");
    while (DWIN_Driver_IsTxBusy())
    {
        DWIN_TX_Pump(); // Chama repetidamente até a bandeja de saída ficar vazia
    }
    printf("... informacoes enviadas. Inicializacao completa.\r\n");
  
}

bool App_Manager_Run_Self_Diagnostics(uint8_t return_tela)
{
    printf("\r\n>>> INICIANDO AUTODIAGNOSTICO <<<\r\n");

    // Tela de Logo inicial
    DWIN_Driver_SetScreen(LOGO);
		DWIN_TX_Pump(); 
    HAL_Delay(3000); // Delay menor para a logo

    // 1. Teste dos Servos
    printf("Diagnostico: Verificando Servos...\r\n");
    DWIN_Driver_SetScreen(BOOT_CHECK_SERVOS);
    DWIN_TX_Pump(); 
    // Futuramente, você pode adicionar um pequeno movimento nos servos aqui para um teste real
    HAL_Delay(1200);

    // 2. Teste do Medidor de Capacitância (Frequência)
    printf("Diagnostico: Verificando Medidor de Frequencia...\r\n");
    DWIN_Driver_SetScreen(BOOT_CHECK_CAPACI);
    DWIN_TX_Pump(); 
    HAL_Delay(1200);

    // 3. Teste da Balança
    printf("Diagnostico: Verificando Balanca...\r\n");
    DWIN_Driver_SetScreen(BOOT_BALANCE);
    DWIN_TX_Pump(); 
    ADS1232_Tare(); // A própria tara já é um bom teste de diagnóstico
    HAL_Delay(1000);

    // 4. Teste do Termômetro Interno
    printf("Diagnostico: Verificando Termometro...\r\n");
    DWIN_Driver_SetScreen(BOOT_THERMOMETER);
    DWIN_TX_Pump(); 
    float temp_inicial = TempSensor_GetTemperature(); 
    Medicao_Set_Temp_Instru(temp_inicial);
    printf("   ... Temperatura: %.2f C\r\n", temp_inicial);
    HAL_Delay(1000);

    // 5. Teste de Memória EEPROM
    printf("Diagnostico: Verificando Memoria EEPROM...\r\n");
    DWIN_Driver_SetScreen(BOOT_MEMORY);
    DWIN_TX_Pump(); 
    if (!EEPROM_Driver_IsReady()) {
        printf("   ... FALHA! EEPROM nao responde.\r\n");
        DWIN_Driver_SetScreen(MSG_ERROR); // Uma tela de erro específica para EEPROM seria ideal
        DWIN_TX_Pump();
        return false; // Falha no diagnóstico
    }
    printf("   ... EEPROM OK.\r\n");
    HAL_Delay(1100);

    // 6. Teste do RTC
    printf("Diagnostico: Verificando RTC...\r\n");
    DWIN_Driver_SetScreen(BOOT_CLOCK);
    DWIN_TX_Pump(); 
    // O RTC já foi inicializado, aqui apenas mostramos a tela
    HAL_Delay(1100);

    printf(">>> AUTODIAGNOSTICO COMPLETO <<<\r\n\r\n");
    DWIN_Driver_SetScreen(return_tela);
    DWIN_TX_Pump(); 
    
    return true; // Todos os testes passaram
}

//================================================================================
// Implementação do Despachante de Tarefas (Super-Loop) - (V8.2)
//================================================================================
void App_Manager_Process(void)
{
    Task_Handle_High_Frequency_Polling();
    
		Task_Update_Frequency();
		
    Task_Handle_Scale();
    
    Task_Update_Display_FSM();
    
    Task_Update_Clock();
    
    Gerenciador_Config_Run_FSM(); 
}

//================================================================================
// Implementação das Tarefas
//================================================================================

static void Task_Handle_High_Frequency_Polling(void)
{
    CLI_TX_Pump();   
    DWIN_TX_Pump();  
    DWIN_Driver_Process(); 
    CLI_Process();         
    Servos_Process();      
}

static bool Check_Stability(float new_grams)
{
    static float stable_grams_ref = 0.0f;
    static int stable_counter = 0;
    const float STABILITY_THRESHOLD_G = 0.05f; 
    const int STABLE_COUNT_TARGET = 3;         

    if (fabsf(new_grams - stable_grams_ref) < STABILITY_THRESHOLD_G)
    {
        stable_counter++;
        if (stable_counter >= STABLE_COUNT_TARGET)
        {
            stable_counter = STABLE_COUNT_TARGET; 
            return true; 
        }
    }
    else
    {
        stable_counter = 0;
        stable_grams_ref = new_grams;
    }
    return false;
}

static void Task_Handle_Scale(void)
{
    if (g_ads_data_ready) 
    {
        g_ads_data_ready = false;
        int32_t leitura_adc_mediana = ADS1232_Read_Median_of_3(); 
        float gramas = ADS1232_ConvertToGrams(leitura_adc_mediana); 
        
        Medicao_Set_Peso(gramas);
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
 * @brief (V8.6) FSM de atualização dos VPs (Freq 1s, ADC 5s)
 */
static void Task_Update_Display_FSM(void)
{
    uint32_t tick_atual = HAL_GetTick();

    if (s_display_state == TASK_DISPLAY_IDLE)
    {
        if (tick_atual - s_display_last_tick < DISPLAY_UPDATE_INTERVAL_MS) return;
        s_display_last_tick = tick_atual; 
        if (DWIN_Driver_IsTxBusy()) return; 
        s_display_state = TASK_DISPLAY_CHECK_SCREEN; 
    }

    if (s_display_state == TASK_DISPLAY_CHECK_SCREEN)
    {
        uint16_t tela_atual = Controller_GetCurrentScreen();

        if (tela_atual == TELA_MONITOR_SYSTEM || tela_atual == TELA_ADJUST_CAPA)
        {
            // MODIFICADO: Busca os dados mais recentes do handler de medição
            DadosMedicao_t dados_atuais;
            Medicao_Get_UltimaMedicao(&dados_atuais);

            // Envia os dados para o display
            int32_t frequencia_para_dwin = (int32_t)((dados_atuais.Frequencia / 1000.0f) * 10.0f);
            DWIN_Driver_WriteInt32(FREQUENCIA, frequencia_para_dwin); 
            
            int32_t escala_a_para_dwin = (int32_t)(dados_atuais.Escala_A * 10.0f);
            DWIN_Driver_WriteInt32(ESCALA_A, escala_a_para_dwin); 

            // Lógica da temperatura (atualiza o valor no handler)
            s_display_temp_counter++;
            if (s_display_temp_counter >= 5) {
                s_display_temp_counter = 0;
                float temp_mcu = TempSensor_GetTemperature();
                Medicao_Set_Temp_Instru(temp_mcu); // Atualiza no handler
                
                // Envia para o display
                int16_t temperatura_para_dwin = (int16_t)(temp_mcu * 10.0f);
                DWIN_Driver_WriteInt(TEMP_INSTRU, temperatura_para_dwin); 
            }
        }
        else
        {
             s_display_temp_counter = 0;
        }

        s_display_state = TASK_DISPLAY_IDLE; 
    }
}

static void Task_Update_Frequency(void)
{
    if (HAL_GetTick() - s_freq_last_tick >= FREQ_UPDATE_INTERVAL_MS)
    {
        s_freq_last_tick = HAL_GetTick();

        uint32_t pulsos = Frequency_Get_Pulse_Count(); 
        Frequency_Reset(); 
        
        Medicao_Set_Frequencia((float)pulsos);
        
        float escala_a = Calcular_Escala_A(pulsos);
        
        Medicao_Set_Escala_A(escala_a);
    }
}

static void Task_Update_Clock(void) {
    static uint32_t last_tick = 0;
    if (HAL_GetTick() - last_tick < 1000) return;
    last_tick = HAL_GetTick();

    if (Controller_GetCurrentScreen() == PRINCIPAL) {
        char time_buf[9];
        char date_buf[9];
        uint8_t h, m, s, d, mo, y;
        if (RTC_Driver_GetTime(&h, &m, &s) && RTC_Driver_GetDate(&d, &mo, &y)) {
            sprintf(time_buf, "%02d:%02d:%02d", h, m, s);
            sprintf(date_buf, "%02d/%02d/%02d", d, mo, y);
            DWIN_Driver_WriteString(HORA_SISTEMA, time_buf, 8);
            DWIN_Driver_WriteString(DATA_SISTEMA, date_buf, 8);
        }
    }
}

//================================================================
// Implementação dos Handlers (chamados pela UI)
//================================================================

void App_Manager_Handle_Start_Process(void) {
    printf("APP: Comando para iniciar processo recebido.\r\n");
    Servos_Start_Sequence(); 
}

void App_Manager_Handle_New_Password(const char* new_password) {
    Gerenciador_Config_Set_Senha(new_password); 
    printf("APP: Nova senha definida (na RAM, pendente de salvamento).\r\n");
}


// Definindo o flag g_ads_data_ready que será usado externamente
volatile bool g_ads_data_ready = false;