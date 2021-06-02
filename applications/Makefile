################################################################################
#                   recursive application build target def                     #
################################################################################
DIRS = led ble_adv led_heartbeat timer accelerometer cpuid mesh sin_cos_dsp prs_radio dsp_iadc

################################################################################
#                             BUILD TARGETS                                    #
################################################################################

# BLE Rail and Radio example
ble_adv_app :
	$(MAKE) -C ble_adv all

prs_radio_app :
	$(MAKE) -C prs_radio all

# Peripheral examples
led_app :
	$(MAKE) -C led all

accelerometer_app :
	$(MAKE) -C accelerometer all


led_heartbeat_app :
	$(MAKE) -C led_heartbeat all

timer_app :
	$(MAKE) -C timer all

# Core & DSP examples
cpuid_app :
	$(MAKE) -C cpuid all

sin_cos_dsp_app :
	$(MAKE) -C sin_cos_dsp all

dsp_iadc_app :
	$(MAKE) -C dsp_iadc all

# Bluetooth Mesh examples
embedded_provisioner_app :
	$(MAKE) -C mesh/embedded_provisioner all

subnet_bridge_app :
	$(MAKE) -C mesh/subnet_bridge all

edge_node_app :
	$(MAKE) -C mesh/edge_node all

################################################################################
#                             CLEAN TARGETS                                    #
################################################################################
clean:
	$(foreach DIR,$(DIRS),cd $(DIR) && make clean && cd .. &&) true
