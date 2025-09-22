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