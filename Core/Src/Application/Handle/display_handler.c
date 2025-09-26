#include "display_handler.h"


/**
 * @brief Lógica para ligar/desligar o backlight.
 * Movido de Tela_ON_OFF do controller.c
 */
void Display_Handle_ON_OFF(int16_t received_value)
{
	if (received_value == 0x0010) {
		DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_10, sizeof(CMD_AJUSTAR_BACKLIGHT_10));
		DWIN_Driver_SetScreen(SYSTEM_STANDBY);
		printf("Display Handler: Desliga backlight\n\r");
	}
	else {
		DWIN_Driver_WriteRawBytes(CMD_AJUSTAR_BACKLIGHT_100, sizeof(CMD_AJUSTAR_BACKLIGHT_100));
		DWIN_Driver_SetScreen(PRINCIPAL);
		printf("Display Handler: Religa backlight\n\r");
	}
}

void Set_Repeticoes(uint16_t received_value)
{
	if (received_value == 0x0050)
	{
		char buffer[30] = " ";
		sprintf (buffer, "Atual NR_Repetition: %d", Gerenciador_Config_Get_NR_Repetition());
		DWIN_Driver_WriteString(0x4096, buffer, sizeof(buffer)); 
		DWIN_Driver_SetScreen(TELA_SETUP_REPETICOES);
	}
	else
	{
		Gerenciador_Config_Set_NR_Repetitions(received_value);
		char buffer[30] = " ";
		sprintf (buffer, "Novo NR_Repetition: %d", Gerenciador_Config_Get_NR_Repetition());
		DWIN_Driver_WriteString(0x4096, buffer, sizeof(buffer));
	} 
}

void Set_Decimals(uint16_t received_value)
{
	if (received_value == 0x0050)
	{
		char buffer[30] = " ";
		sprintf (buffer, "Atual NR_Decimals: %d", Gerenciador_Config_Get_NR_Decimals());
		DWIN_Driver_WriteString(0x4096, buffer, sizeof(buffer)); 
		DWIN_Driver_SetScreen(TELA_SET_DECIMALS);
	}
	else
	{
		Gerenciador_Config_Set_NR_Decimals(received_value);
		char buffer[30] = " ";
		sprintf (buffer, "Novo NR_Decimals: %d", Gerenciador_Config_Get_NR_Decimals());
		DWIN_Driver_WriteString(0x4096, buffer, sizeof(buffer));
	} 
}

void Print(uint16_t received_value)
{
	if (received_value == 0x0000) //set print
	{
		Config_Grao_t Dados_Grao;
		uint8_t indice_grao; 
		Gerenciador_Config_Get_Grao_Ativo(&indice_grao); 
		
		
		if (Gerenciador_Config_Get_NR_Decimals() == 1 && estado_print)
		{	
			if (Gerenciador_Config_Get_Dados_Grao(indice_grao, &Dados_Grao)) 
			{
				DWIN_Driver_WriteString(GRAO_A_MEDIR, Dados_Grao.nome, MAX_NOME_GRAO_LEN);
				DWIN_Driver_WriteInt(UMIDADE_1_CASA, 237);
				DWIN_Driver_WriteInt(CURVA, Dados_Grao.id_curva);
				DWIN_Driver_WriteInt(UMI_MIN, (Dados_Grao.umidade_min)*10);
				DWIN_Driver_WriteInt(UMI_MAX, (Dados_Grao.umidade_max)*10);
				DWIN_Driver_SetScreen(MEDE_RESULT_01);
			}
		}
		
		if (Gerenciador_Config_Get_NR_Decimals() == 2 && estado_print)
		{
			if (Gerenciador_Config_Get_Dados_Grao(indice_grao, &Dados_Grao)) 
			{
				DWIN_Driver_WriteString(GRAO_A_MEDIR, Dados_Grao.nome, MAX_NOME_GRAO_LEN);
				DWIN_Driver_WriteInt(UMIDADE_2_CASAS, 2754);
				DWIN_Driver_WriteInt(CURVA, Dados_Grao.id_curva);
				DWIN_Driver_WriteInt(UMI_MIN, (Dados_Grao.umidade_min)*10);
				DWIN_Driver_WriteInt(UMI_MAX, (Dados_Grao.umidade_max)*10);
				DWIN_Driver_SetScreen(MEDE_RESULT_02);
			}
		}
	}
	else
	{
		if (estado_print)
		{
			Relatorio_Printer();
		}
	}
}

void Type_User(const uint8_t* dwin_data, uint16_t len, uint16_t received_value)
{
	if (received_value == 0x0050)
	{
	
		char nome_atual[21] = {0}; 
		char buffer_display[50] = {0};

		Gerenciador_Config_Get_Usuario(nome_atual, sizeof(nome_atual));

		sprintf(buffer_display, "Atual Usuario: %s", nome_atual);
		DWIN_Driver_WriteString(0x4096, buffer_display, strlen(buffer_display)); 
		DWIN_Driver_SetScreen(TELA_USER);
	}
	else
	{
		char novo_nome[21] = {0}; 
		const uint8_t* payload = &dwin_data[6];
		uint16_t payload_len = len - 6;


		if (DWIN_Parse_String_Payload_Robust(payload, payload_len, novo_nome, sizeof(novo_nome)))
		{
			if (strlen(novo_nome) > 0)
			{
				printf("Display Handler: Recebido novo nome de usuario: '%s'\n", novo_nome);
				
				Gerenciador_Config_Set_Usuario(novo_nome);

				char buffer_display[50] = {0};
				sprintf(buffer_display, "Novo Usuario: %s", novo_nome);
				DWIN_Driver_WriteString(0x4096, buffer_display, strlen(buffer_display));
			}
		}
	}
}

void Type_Company(const uint8_t* dwin_data, uint16_t len, uint16_t received_value)
{
	if (received_value == 0x0050)
	{
		char empresa_atual[21] = {0};
		char buffer_display[50] = {0};

		Gerenciador_Config_Get_Company(empresa_atual, sizeof(empresa_atual));

		sprintf(buffer_display, "Atual Empresa: %s", empresa_atual);
		DWIN_Driver_WriteString(0x4096, buffer_display, strlen(buffer_display)); 
		DWIN_Driver_SetScreen(TELA_COMPANY);
	}
	else
	{
		char nova_empresa[21] = {0}; 
		const uint8_t* payload = &dwin_data[6];
		uint16_t payload_len = len - 6;


		if (DWIN_Parse_String_Payload_Robust(payload, payload_len, nova_empresa, sizeof(nova_empresa)))
		{
			if (strlen(nova_empresa) > 0)
			{
				printf("Display Handler: Recebido novo nome de usuario: '%s'\n", nova_empresa);
				
				Gerenciador_Config_Set_Company(nova_empresa);

				char buffer_display[50] = {0};
				sprintf(buffer_display, "Nova Empresa: %s", nova_empresa);
				DWIN_Driver_WriteString(0x4096, buffer_display, strlen(buffer_display));
			}
		}
	}
}

void Habilita_print(uint16_t received_value)
{
	if (received_value == 0x001)
	{
		estado_print = true;
		printf("%s\n\r", estado_print ? "true" : "false");
	}
	else
	{
		estado_print = false;
		printf("%s\n\r", estado_print ? "true" : "false");
	}
}

void About(void)
{
	DWIN_Driver_WriteString(0x4096, "G620_Teste_Gab", strlen("G620_Teste_Gab"));
	DWIN_Driver_SetScreen(TELA_ABOUT_SYSTEM);
}