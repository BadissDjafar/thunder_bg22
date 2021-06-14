################################################################################
#                       apps config plumbing                                   #
################################################################################
ifeq ($(CONFIG_APP_LED),y)
	TARGET = led
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
	
endif

ifeq ($(CONFIG_APP_BLE_ADV),y)
	TARGET = ble_adv
	IPATH += -I$(PRJROOT)/$(LIBS)/RF/bluetooth
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
endif

ifeq ($(CONFIG_APP_BLE_HCI),y)
	TARGET = ble_adv
	IPATH += -I$(PRJROOT)/$(LIBS)/RF/bluetooth
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
endif

ifeq ($(CONFIG_APP_BLE_THROUGHPUT),y)
	TARGET = ble_adv
	IPATH += -I$(PRJROOT)/$(LIBS)/RF/bluetooth
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
endif

ifeq ($(CONFIG_APP_CPUID),y)
	TARGET = cpuid
	IPATH += 
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
endif

ifeq ($(CONFIG_APP_SIN_COS_DSP),y)
	TARGET = sin_cos_dsp
	IPATH += -I$(PRJROOT)/include/CMSIS/DSP/include
	LINK_LIBS = m $(RADIO_LIB)
endif

ifeq ($(CONFIG_APP_PRS_RADIO),y)
	TARGET = prs_radio
	IPATH += -I$(PRJROOT)/$(LIBS)/RF/bluetooth
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
endif

ifeq ($(ARCH),cortex-m4)
	LINK_LIBS += nvm3
endif
ifeq ($(ARCH),cortex-m33)
	LINK_LIBS += nvm3_CM33_gcc
endif
	