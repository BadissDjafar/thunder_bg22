config APP_PRS_RADIO
	bool "Ouput radio RX/TX ACT on pins using PRS"
	depends on BOARD_EFR32MG21_BRD4181A01 # Only available on xG21 Gecko wireless kit so far
	
