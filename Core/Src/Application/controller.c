/*******************************************************************************
 * @file        Controller.c
 * @brief       Controlador do Sistema em uma arquitetura MVC.
 * @version     Revisado v1.0
 ******************************************************************************/

#include "controller.h"

//================================================================================
// Definições, Enums, Variáveis Estáticas e Protótipos
//================================================================================


// Variáveis de estado para a lógica LOCAL (Seleção de Grão)
static int8_t s_indice_grao_selecionado = 0;
static bool s_em_tela_de_selecao = false;
static int16_t received_value = 0;
static uint16_t s_current_screen_id = PRINCIPAL; 
static void Handle_Escape_Navigation(void);


//================================================================================
// Funções Públicas (Getters/Setters)
//================================================================================

/**
 * @brief (V8.3) Retorna a tela que o controlador acredita estar ativa.
 */
uint16_t Controller_GetCurrentScreen(void)
{
    return s_current_screen_id;
}

/**
 * @brief Wrapper PÚBLICO para DWIN_Driver_SetScreen que rastreia a tela atual.
 */
void Controller_SetScreen(uint16_t screen_id)
{
    s_current_screen_id = screen_id;
}


//================================================================================
// Função de Callback
//================================================================================
void Controller_DwinCallback(const uint8_t* data, uint16_t len)
{
    if (len < 6 || data[0] != 0x5A || data[1] != 0xA5) {
        return; 
    }

    if (data[3] == 0x83) { 
        uint16_t vp_address = (data[4] << 8) | data[5];
        
        if (len >= 8) {
            if (vp_address != SENHA_CONFIG && vp_address != SET_SENHA && vp_address != SET_TIME)
            {
                 uint8_t payload_len = data[2]; 
                 if (len >= (3 + payload_len)) {
                    received_value = (data[3 + payload_len - 2] << 8) | data[3 + payload_len - 1];
                 }
            } else {
                 received_value = 0; 
            }
        }
        //================================================================
        // DESPACHANTE DE COMANDOS
        //================================================================
        switch (vp_address) {

				
						case DESCARTA_AMOSTRA		:		printf("Botao Descarta Amostra Pressionado\n\r");                                      break;
						case SELECT_GRAIN				:		Graos_Handle_Entrada_Tela();                                                        	 break;
						case PRINT							:		Print(received_value);                                                                 break;
            case OFF								:		Display_Handle_ON_OFF(received_value);                                                 break;
						
            case SENHA_CONFIG				:		Auth_ProcessLoginEvent(data, len);                                                     break;
            case SET_TIME						:		RTC_Handle_Set_Time(data, len);                                                        break;
						case NR_REPETICOES      :   Set_Repeticoes(received_value);                                                        break;
						case DECIMALS           :   Set_Decimals(received_value);                                                          break;
						case DES_HAB_PRINT      :   Habilita_print(received_value);                                                        break;
						case SET_SENHA					:		Auth_ProcessSetPasswordEvent(data, len);                                               break;
						//auto diagnoses
						case USER               :   Type_User(data, len, received_value);                                                  break;
						case COMPANY            :   Type_Company(data, len, received_value);                                               break;
						case ABOUT_SYS          :   About();                                                                               break;
						
						
						case MONITOR						:		Controller_SetScreen(TELA_MONITOR_SYSTEM);                                             break;
						case SET_DATE_TIME      :   RTC_Handle_Set_Date_And_Time(data, len);                                               break;
						case MODEL_OEM          :   Model();                                                                               break;
						
						case TECLAS							:		Graos_Handle_Navegacao(received_value);           																		 break;
						case ESCAPE							:		Handle_Escape_Navigation();																												     break;
						
            default:                                                                 
							break;
        }
    }
}



static void Handle_Escape_Navigation(void)
{

    if (s_current_screen_id == TELA_MONITOR_SYSTEM) 
    {
         Controller_SetScreen(PRINCIPAL); 
         printf("CONTROLLER: Saindo do Monitor -> Tela de Servico.\r\n");
    }
    
}