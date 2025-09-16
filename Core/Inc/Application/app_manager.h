// Core/Inc/Application/app_manager.h
// VERS�O 8.2 (Refatorado por Dev STM)
// REMOVIDO: dependencia de scale_filter.h.
// ADICIONADO: Nova struct App_ScaleData_t.

#ifndef APP_MANAGER_H
#define APP_MANAGER_H

// Includes dos Perif�ricos Gerados pelo CubeMX
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "crc.h"
#include "rtc.h"
#include "tim.h"

// Includes dos Nossos M�dulos e Drivers
#include "dwin_driver.h"
#include "eeprom_driver.h"
#include "ads1232_driver.h"
#include "pwm_servo_driver.h"
#include "pcb_frequency.h"
#include "temp_sensor.h"
#include "gerenciador_configuracoes.h"
#include "servo_controle.h"
#include "controller.h"
#include "cli_driver.h"

/**
 * @brief Nova estrutura de dados de sa�da da balan�a.
 * Substitui a depend�ncia do 'ScaleFilterOut' do scale_filter.h.
 */
typedef struct {
    float    grams_display;     // Valor final em gramas (usado pela UI)
    float    raw_counts_median; // Contagem bruta (resultado da mediana de 3)
    bool     is_stable;         // Flag de estabilidade (l�gica simplificada)
} App_ScaleData_t;


typedef struct {
    uint32_t pulsos;
    float escala_a;
} FreqData_t;


/**
 * @brief Inicializa todos os m�dulos da aplica��o em uma sequ�ncia controlada.
 */
void App_Manager_Init(void);

/**
 * @brief Executa o loop de processamento principal da aplica��o (Super-loop V8.2).
 */
void App_Manager_Process(void);

// Fun��es de Callback para serem chamadas pela UI/Controller
void App_Manager_Handle_Start_Process(void);
void App_Manager_Handle_New_Password(const char* new_password);

void App_Manager_GetScaleData(App_ScaleData_t* data); 
void App_Manager_GetFreqData(FreqData_t* data);
float App_Manager_GetTemperature(void);

#endif // APP_MANAGER_H