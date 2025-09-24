#include "relato.h"


const char Ejeta[] = "================================\n\r" "\n\r"  "\n\r"  "\n\r ";
const char Dupla[] = "\n\r================================\n\r";
const char Linha[] = "--------------------------------\n\r";


void Who_am_i(void)
{
  printf(Dupla);
  printf("             G5000\n\r");
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
	printf("\n\r");
  printf("\n\r");
  printf(Linha);
	printf("Assinatura              ");	RTC_Driver_Get_Time();	printf("\n\r");
	printf("Responsavel             ");	RTC_Driver_Get_Date();	printf("\n\r");
	printf ("\n\r");
  printf ("\n\r");
  printf ("\n\r");
  printf ("\n\r");
}

void Cabecalho(void)
{
  printf(Dupla);
 	printf("GEHAKA                     G4800\n\r");
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

void Gerar_String_Relatorio_Para_QR(char* buffer, size_t buffer_size)
{	
    Config_Grao_t dados_grao;
    uint8_t indice_grao_ativo;
    uint16_t casas_decimais = 0;
    
    // 1. A mesma lógica para obter os dados de configuração
    if (Gerenciador_Config_Get_Grao_Ativo(&indice_grao_ativo) &&
        Gerenciador_Config_Get_Dados_Grao(indice_grao_ativo, &dados_grao)) {
        
        casas_decimais = Gerenciador_Config_Get_NR_Decimals(); 
    } else {
        snprintf(buffer, buffer_size, "ERRO: Nao foi possivel carregar dados do grao.");
        return;
    }

    // 2. Ponteiros para construir a string de forma segura e incremental
    char* ptr = buffer;
    size_t remaining_size = buffer_size;
    int len = 0;

    // 3. Adiciona o conteúdo de cada printf() ao buffer usando snprintf

    // ATENÇÃO: Substitua o texto abaixo pelo conteúdo exato que sua função Cabecalho() imprime.
    len = snprintf(ptr, remaining_size, "--- Relatorio de Umidade ---\n");
    ptr += len; remaining_size -= len;

    len = snprintf(ptr, remaining_size, "Produto: %s\n", dados_grao.nome);
    ptr += len; remaining_size -= len;

    len = snprintf(ptr, remaining_size, "Versao Equacao: %lu\n", (unsigned long)dados_grao.id_curva);
    ptr += len; remaining_size -= len;

    len = snprintf(ptr, remaining_size, "Validade Curva: %s\n", dados_grao.validade);
    ptr += len; remaining_size -= len;
		
		len = snprintf(ptr, remaining_size, "Temp. Amostra: %.1f 'C\n", 22.0);
    ptr += len; remaining_size -= len;
		
		len = snprintf(ptr, remaining_size, "Densidade: %.1f Kg/hL\n", 71.0);
    ptr += len; remaining_size -= len;
		
		len = snprintf(ptr, remaining_size, "--------------------------\n");
    ptr += len; remaining_size -= len;

    len = snprintf(ptr, remaining_size, "Umidade: %.*f %%\n", (int)casas_decimais, 27.432);
    ptr += len; remaining_size -= len;

    len = snprintf(ptr, remaining_size, "--------------------------\n");
    ptr += len; remaining_size -= len;
		
		printf("Qr_Code Carregado");
}