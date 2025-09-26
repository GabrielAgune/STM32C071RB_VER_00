#include "relato.h"


const char Ejeta[] = "================================\n\r" "\n\r"  "\n\r"  "\n\r ";
const char Dupla[] = "\n\r================================\n\r";
const char Linha[] = "--------------------------------\n\r";


void Who_am_i(void)
{
  printf(Dupla);
  printf("         G620_Teste_Gab\n\r");
  printf("     (c) GEHAKA, 2004-2025\n\r");
  printf(Linha);
  printf("CPU      =           STM32C071RB\n\r");
  printf("Firmware = %21s\r\n", FIRMWARE);
  printf("Hardware = %21s\r\n", HARDWARE);
  printf("Serial   = %21s\r\n", SERIAL);
  printf(Linha);
  printf("Medidas  = %21d\n\r", 22);
  printf(Ejeta);
}

void Assinatura(void)
{
	uint8_t hours, minutes, seconds;
	uint8_t day, month, year; 
	
	printf("\n\r");
  printf("\n\r");
  printf(Linha);
	if (RTC_Driver_GetTime(&hours, &minutes, &seconds))
	{
		printf("Assinatura              %02d:%02d:%02d\n\r", hours, minutes, seconds);
	}
	if (RTC_Driver_GetDate(&day, &month, &year))
	{
		printf("Responsavel             %02d/%02d/%02d\n\r", day, month, year);
	}
	printf ("\n\r");
  printf ("\n\r");
  printf ("\n\r");
  printf ("\n\r");
}

void Cabecalho(void)
{
  printf(Dupla);
 	printf("GEHAKA            G620_Teste_Gab\n\r");
  printf(Linha);
	printf("Versao Firmware= %15s\n\r", FIRMWARE);
 	printf("Numero de Serie= %15s\n\r", SERIAL);
  printf(Linha);
}

void Relatorio_Printer (void)
{	
		Config_Grao_t dados_grao;
		uint8_t indice_grao_ativo;
		uint16_t casas_decimais = 0;
		
		if (Gerenciador_Config_Get_Grao_Ativo(&indice_grao_ativo) &&
        Gerenciador_Config_Get_Dados_Grao(indice_grao_ativo, &dados_grao)) {
        
        // Busca o número de casas decimais
        casas_decimais = Gerenciador_Config_Get_NR_Decimals(); 
    } else {
        // Falha ao obter dados de configuração, imprime um erro ou usa valores padrão
        printf("ERRO: Nao foi possivel carregar dados do grao para o relatorio.\n\r");
        return;
    }
		
    Cabecalho();
  
    printf("Produto       = %16s\n\r",  dados_grao.nome);
  	printf("Versao Equacao= %10lu\n\r",   (unsigned long)dados_grao.id_curva);
  	printf("Validade Curva= %13s\n\r", dados_grao.validade);
  	printf("Amostra Numero= %8i\n\r",      4);
  	printf("Temp.Amostra .= %8.1f 'C\n\r", 22.0);
  	printf("Temp.Instru ..= %8.1f 'C\n\r", 21.7);
  	printf("Peso Amostra .= %8.1f g\n\r", 0.0);
  	printf("Densidade ....= %8.1f Kg/hL\n\r",  71.0);
    printf(Linha);         
  	printf("Umidade ......= %14.*f %%\n\r", (int)casas_decimais, 27.432);
  	printf(Linha);

  	Assinatura();
}
