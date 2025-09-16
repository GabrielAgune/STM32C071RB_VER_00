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
#include "dwin_driver.h"   // (V8.3) Para Enums de Tela
#include "controller.h" // (V8.3) Para GetCurrentScreen
#include "servo_controle.h"
#include "ads1232_driver.h"
#include "pcb_frequency.h"
#include "temp_sensor.h"
#include "gerenciador_configuracoes.h"
#include <stdio.h>
#include <string.h>
#include <math.h>   
#include <stdbool.h>

//================================================================================
// Variáveis de Estado Globais do Módulo
//================================================================================

static App_ScaleData_t s_scale_output; 
static FreqData_t s_freq_data;
static float s_temperatura_mcu = 0.0f;

extern volatile bool g_ads_data_ready; 

//================================================================================
// Definições da FSM de Atualização do Display
//================================================================================
typedef enum {
    TASK_DISPLAY_IDLE,
    TASK_DISPLAY_CHECK_SCREEN, // (V8.4) Checa a tela
} TaskDisplay_State_t;

static TaskDisplay_State_t s_display_state = TASK_DISPLAY_IDLE;
static uint32_t s_display_last_tick = 0;
static const uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000;
static uint8_t s_display_temp_counter = 0; // (V8.5) Sub-contador para coleta de dados bloqueante

//================================================================================
// Protótipos das Tarefas (Funções Privadas)
//================================================================================
static void Task_Handle_High_Frequency_Polling(void);
static void Task_Handle_Scale(void); 
static void Task_Update_Display_FSM(void);
static float Calcular_Escala_A(uint32_t frequencia_hz);
static bool Check_Stability(float new_grams); 

//================================================================================
// Implementação da Função de Inicialização
//================================================================================
void App_Manager_Init(void)
{
    // (Sequência de Init V1.0 original, sem alterações)
    CLI_Init(&huart1);
    printf("Sistema Integrado - Log de Inicializacao:\r\n");
    printf("1. CLI/Debug UART... OK\r\n");
    EEPROM_Driver_Init(&hi2c1);
    RTC_Driver_Init(&hrtc);
    printf("2. Drivers I2C e RTC... OK\r\n");
    
    Gerenciador_Config_Init(&hcrc);
    printf("3. Gerenciador de Configuracoes... ");
    if (!Gerenciador_Config_Validar_e_Restaurar()) {
        printf("[FALHA]\r\nERRO FATAL: Nao foi possivel carregar/restaurar configuracoes.\r\n");
    } else {
        printf("[OK]\r\n");
    }
    
    ADS1232_Init();
    Frequency_Init(); // Usa TIM2 Counter Mode
    Servos_Init();    // Usa TIM16/17 PWM
    printf("4. Modulos de Hardware (ADC, Servos, Frequencia)... OK\r\n");
    
    printf("5. Executando tara da balanca (pode demorar alguns segundos)...\r\n");
    ADS1232_Tare();
    
    memset(&s_scale_output, 0, sizeof(s_scale_output));
    printf("   ... Tara concluida.\r\n");
    
    s_temperatura_mcu = TempSensor_GetTemperature(); // Lê uma vez no boot
    printf("Temperatura inicial: %.2f C\r\n", s_temperatura_mcu);
        
    DWIN_Driver_Init(&huart2, Controller_DwinCallback);
    printf("6. Interface de Usuario... Iniciando sequencia de splash.\r\n");
    printf("\r\n>>> INICIALIZACAO COMPLETA (V8.2 Robusta) <<<\r\n\r\n");
}

//================================================================================
// Implementação do Despachante de Tarefas (Super-Loop) - (V8.2)
//================================================================================
void App_Manager_Process(void)
{
    // 1. Tarefas de alta frequência
    Task_Handle_High_Frequency_Polling();
    
    // 2. Tarefa da Balança
    Task_Handle_Scale();
    
    // 3. FSM de Atualização de Display (V8.6)
    Task_Update_Display_FSM();
    
    // 4. Tarefa de atualização do RTC (V8.3)
    RTC_Driver_Process();
    
    // 5. FSM de Armazenamento
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
        s_scale_output.raw_counts_median = (float)leitura_adc_mediana;
        s_scale_output.grams_display = ADS1232_ConvertToGrams(leitura_adc_mediana); 
        s_scale_output.is_stable = Check_Stability(s_scale_output.grams_display);
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
        if (tick_atual - s_display_last_tick < DISPLAY_UPDATE_INTERVAL_MS) {
            return; // Não é hora (1s)
        }
        
        s_display_last_tick = tick_atual; 

        if (DWIN_Driver_IsTxBusy()) 
        {
             return; 
        }
        
        s_display_state = TASK_DISPLAY_CHECK_SCREEN; 
    }

    if (s_display_state == TASK_DISPLAY_CHECK_SCREEN)
    {
        // 1. Verifica a tela PRIMEIRO.
        uint16_t tela_atual = Controller_GetCurrentScreen();

        if (tela_atual == TELA_MONITOR_SYSTEM) // Tela 56
        {
            // *** INÍCIO CORREÇÃO V8.6 (Proposta do Usuário) ***
            
            // 1. ATUALIZAÇÕES RÁPIDAS (A CADA 1 SEGUNDO)
            // Lemos a freq e resetamos o contador a cada 1s para o cálculo ficar correto.
            s_freq_data.pulsos = Frequency_Get_Pulse_Count(); 
            Frequency_Reset(); 
            
            // Usa a temperatura lida anteriormente (s_temperatura_mcu) para o cálculo
            if (s_temperatura_mcu > 0) { 
                s_freq_data.escala_a = Calcular_Escala_A(s_freq_data.pulsos);
            } else {
                s_freq_data.escala_a = 0.0f;
            }

            // Envia dados rápidos (Freq/Escala) a cada 1 segundo
            int32_t frequencia_para_dwin = (int32_t)((s_freq_data.pulsos / 1000.0f) * 10.0f);
            DWIN_Driver_WriteInt32(FREQUENCIA, frequencia_para_dwin); 
            
            int32_t escala_a_para_dwin = (int32_t)(s_freq_data.escala_a * 10.0f);
            DWIN_Driver_WriteInt32(ESCALA_A, escala_a_para_dwin); 

            // 2. ATUALIZAÇÃO LENTA (A CADA 5 SEGUNDOS)
            // O ADC (Temp) bloqueia por 100ms. Execute apenas a cada 5 segundos.
            s_display_temp_counter++;
            if (s_display_temp_counter >= 5) { // Roda a cada 5 chamadas (5 segundos)
                s_display_temp_counter = 0;
                
                s_temperatura_mcu = TempSensor_GetTemperature(); // <-- BLOQUEIO DE 100ms (Agora só a cada 5s)
                
                int16_t temperatura_para_dwin = (int16_t)(s_temperatura_mcu * 10.0f);
                DWIN_Driver_WriteInt(TEMP_SAMPLE, temperatura_para_dwin); 
            }
            // *** FIM CORREÇÃO V8.6 ***
        }
        else
        {
             // Não estamos na tela do monitor. Reseta o contador lento.
             s_display_temp_counter = 0;
        }

        // 3. Sequência completa. Volta para ocioso.
        s_display_state = TASK_DISPLAY_IDLE; 
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

void App_Manager_GetScaleData(App_ScaleData_t* data) {
    if (data != NULL) { 
        *data = s_scale_output; 
    }
}

void App_Manager_GetFreqData(FreqData_t* data) {
    if (data != NULL) { *data = s_freq_data; }
}

float App_Manager_GetTemperature(void) {
    return s_temperatura_mcu;
}

// Definindo o flag g_ads_data_ready que será usado externamente
volatile bool g_ads_data_ready = false;