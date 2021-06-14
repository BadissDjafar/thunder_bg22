################################################################################
#                   recursive application build target def                     #
################################################################################
DIRS = led ble_adv ble_hci ble_throughput led_heartbeat cpuid sin_cos_dsp prs_radio

################################################################################
#                             BUILD TARGETS                                    #
################################################################################

# BLE Rail and Radio example
ble_adv_app :
	$(MAKE) -C ble_adv all

ble_hci_app :
	$(MAKE) -C ble_hci all

ble_throughput_app :
	$(MAKE) -C ble_throughput all

prs_radio_app :
	$(MAKE) -C prs_radio all

# Peripheral examples
led_app :
	$(MAKE) -C led all

led_heartbeat_app :
	$(MAKE) -C led_heartbeat all

# Core & DSP examples
cpuid_app :
	$(MAKE) -C cpuid all

sin_cos_dsp_app :
	$(MAKE) -C sin_cos_dsp all

################################################################################
#                             CLEAN TARGETS                                    #
################################################################################
clean:
	$(foreach DIR,$(DIRS),cd $(DIR) && make clean && cd .. &&) true
