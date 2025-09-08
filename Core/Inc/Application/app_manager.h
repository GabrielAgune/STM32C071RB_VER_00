// Core/Inc/Application/app_manager.h

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
#include "scale_filter.h"
#include "interface_usuario.h"
#include "cli_driver.h"

typedef struct {
    uint32_t pulsos;
    float escala_a;
} FreqData_t;


/**
 * @brief Inicializa todos os m�dulos da aplica��o em uma sequ�ncia controlada.
 * Deve ser chamada uma �nica vez no in�cio do main().
 */
void App_Manager_Init(void);

/**
 * @brief Executa o loop de processamento principal da aplica��o.
 * Esta fun��o cont�m a l�gica do super-loop n�o-bloqueante e
 * deve ser chamada continuamente dentro do while(1) do main().
 */
void App_Manager_Process(void);

// Fun��es de Callback para serem chamadas pela UI
void App_Manager_Handle_Start_Process(void);
void App_Manager_Handle_New_Password(const char* new_password);

void App_Manager_GetScaleData(ScaleFilterOut* data);
void App_Manager_GetFreqData(FreqData_t* data);
float App_Manager_GetTemperature(void);

#endif // APP_MANAGER_H
