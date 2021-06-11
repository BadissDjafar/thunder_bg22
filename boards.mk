################################################################################
#                        boards config plumbing                                #
################################################################################
ifeq ($(CONFIG_BOARD_EFR32BG22_SLTB010A),y)
	export LIBS = libs/efr32bg22
	export BOARD = boards/thunderboard_bg22
	export SOC = soc/efr32bg22
	CHIP := EFR32BG22C224F512IM40
	LINKER_SCRIPT := efr32bg22c224f512im40
	RADIO_LIB := rail_efr32xg22_gcc_release
	LL_HCI_LIB := bluetooth_controller_efr32xg22_gcc_release
	ARCH := cortex-m33
	FPU := fpv5-sp-d16
	FLOAT_ABI := hard
endif

ifeq ($(CONFIG_BOARD_EFR32BGM220_EXP_KIT),y)
	export LIBS = libs/bgm220
	export BOARD = boards/bgm220_explorer_kit
	export SOC = soc/bgm220
	CHIP := BGM220PC22HNA
	LINKER_SCRIPT := bgm220pc22hna
	RADIO_LIB := rail_module_efr32xg22_gcc_release rail_config_bgm220pc22hna_gcc
	LL_HCI_LIB := bluetooth_controller_efr32xg22_gcc_release
	ARCH := cortex-m33
	FPU := fpv5-sp-d16
	FLOAT_ABI := hard
endif

ifeq ($(CONFIG_BOARD_EFR32MG21_BRD4181A01),y)
	export LIBS = libs/efr32mg21
	export BOARD = boards/brd4181a01
	export SOC = soc/efr32bg21
	CHIP := EFR32BG21A020F1024IM32
	LINKER_SCRIPT := efr32mg21a010f1024im32
	RADIO_LIB := rail_efr32xg21_gcc_release
	LL_HCI_LIB := bluetooth_controller_efr32xg21_gcc_release
	ARCH := cortex-m33
	FPU := fpv5-sp-d16
	FLOAT_ABI := hard
endif

ifeq ($(CONFIG_BOARD_EFR32MG12P_SLTB004A),y)
	export LIBS = libs/efr32mg12p
	export BOARD = boards/thunderboard_s2
	export SOC = soc/efr32mg12p
	CHIP := EFR32MG12P332F1024GL125
	LINKER_SCRIPT := efr32mg12p332f1024gl125
	RADIO_LIB := rail_efr32xg12_gcc_release
	LL_HCI_LIB := bluetooth_controller_efr32xg12_gcc_release
	ARCH := cortex-m4
	FPU := fpv4-sp-d16
	FLOAT_ABI := softfp
endif
