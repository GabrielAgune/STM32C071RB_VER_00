// Core/Inc/Application/app_manager.h

#ifndef APP_MANAGER_H
#define APP_MANAGER_H

// Includes dos Periféricos Gerados pelo CubeMX
#include "main.h"
#include "i2c.h"
#include "usart.h"
#include "crc.h"
#include "rtc.h"
#include "tim.h"

// Includes dos Nossos Módulos e Drivers
#include "dwin_driver.h"
#include "eeprom_driver.h"
#include "ads1232_driver.h"
#include "pwm_servo_driver.h"
#include "pcb_frequency.h"
#include "temp_sensor.h"
#include "gerenciador_configuracoes.h"
#include "servo_controle.h"
#include "scale_filter.h"
#include "interface_usuario.h"
#include "cli_driver.h"

typedef struct {
    uint32_t pulsos;
    float escala_a;
} FreqData_t;


/**
 * @brief Inicializa todos os módulos da aplicação em uma sequência controlada.
 * Deve ser chamada uma única vez no início do main().
 */
void App_Manager_Init(void);

/**
 * @brief Executa o loop de processamento principal da aplicação.
 * Esta função contém a lógica do super-loop não-bloqueante e
 * deve ser chamada continuamente dentro do while(1) do main().
 */
void App_Manager_Process(void);

// Funções de Callback para serem chamadas pela UI
void App_Manager_Handle_Start_Process(void);
void App_Manager_Handle_New_Password(const char* new_password);

void App_Manager_GetScaleData(ScaleFilterOut* data);
void App_Manager_GetFreqData(FreqData_t* data);
float App_Manager_GetTemperature(void);

#endif // APP_MANAGER_H
