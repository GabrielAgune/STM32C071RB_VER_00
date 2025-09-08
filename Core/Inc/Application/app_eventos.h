#ifndef APP_EVENTOS_H
#define APP_EVENTOS_H

#include <stdint.h>

// Enumeração de todos os tipos de eventos possíveis no sistema
typedef enum {
    EV_NONE,
    EV_UI_START_BUTTON_PRESSED,
    EV_UI_PASSWORD_ENTERED,
    EV_UI_NEW_PASSWORD_SET,
    EV_UI_DATETIME_ENTERED,
    EV_PROCESS_STARTED,
    EV_PROCESS_FINISHED,
    EV_AUTH_SUCCESS,
    EV_AUTH_FAILURE,
    EV_SETTINGS_APPLIED,
    EV_SERVOS_SEQUENCE_STEP_CHANGED,
    EV_SERVOS_SEQUENCE_FINISHED,
    EV_SYSTEM_TICK_1S,
} Tipo_Evento_t;

// --- Estruturas de Dados (Payloads) para os Eventos ---
typedef struct { char value[11]; } StringPayload_t;
typedef struct { uint8_t day, month, year, hour, minute, second; } DateTimePayload_t;

// CORRIGIDO: Adicionado o estado SERVO_STEP_FINISHED
typedef enum {
    SERVO_STEP_FUNNEL,
    SERVO_STEP_SCRAPER,
    SERVO_STEP_IDLE,
    SERVO_STEP_FINISHED 
} ServoStep_t;

// Estrutura principal de um evento
typedef struct {
    Tipo_Evento_t type;
    void* payload;
} Evento_t;

// Definição do tipo de função para os "handlers" de eventos
typedef void (*Funcao_Handler_Evento_t)(Evento_t event);

#endif // APP_EVENTOS_H
