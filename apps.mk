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

ifeq ($(CONFIG_APP_CPUID),y)
	TARGET = cpuid
	IPATH += 
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
endif

ifeq ($(CONFIG_APP_ACCELEROMETER_BMA440),y)
	TARGET = accelerometer
	IPATH += -I$(PRJROOT)/drivers/emdrv/ustimer
	LINK_LIBS = bluetooth $(RADIO_LIB) mbedtls
endif

ifeq ($(CONFIG_APP_MESH_EMBEDDED_PROVISIONER),y)
	TARGET = embedded_provisioner
	IPATH += -I$(PRJROOT)/$(LIBS)/RF/btmesh
	CFLAGS += -DNVM3_DEFAULT_MAX_OBJECT_SIZE=512 -DENABLE_LOGGING=1 -DMESH_LIB_NATIVE=1
	LINK_LIBS = bluetooth_mesh $(RADIO_LIB)
endif

ifeq ($(CONFIG_APP_MESH_SUBNET_BRIDGE),y)
	TARGET = subnet_bridge
	IPATH += -I$(PRJROOT)/$(LIBS)/RF/btmesh
	CFLAGS += -DNVM3_DEFAULT_MAX_OBJECT_SIZE=512 -DENABLE_LOGGING=1 -DMESH_LIB_NATIVE=1
	LINK_LIBS = bluetooth_mesh $(RADIO_LIB)
endif

ifeq ($(CONFIG_APP_MESH_EDGE_NODE),y)
	TARGET = edge_node
	IPATH += -I$(PRJROOT)/$(LIBS)/RF/btmesh -I$(PRJROOT)/drivers/emdrv/ustimer
	CFLAGS += -DNVM3_DEFAULT_MAX_OBJECT_SIZE=512 -DENABLE_LOGGING=1 -DMESH_LIB_NATIVE=1
	LINK_LIBS = bluetooth_mesh $(RADIO_LIB) m
endif

ifeq ($(CONFIG_APP_SIN_COS_DSP),y)
	TARGET = sin_cos_dsp
	IPATH += -I$(PRJROOT)/include/CMSIS/DSP/include
	LINK_LIBS = m $(RADIO_LIB)
endif

ifeq ($(CONFIG_APP_DSP_IADC),y)
	TARGET = dsp_iadc
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
	