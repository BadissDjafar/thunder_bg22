################################################################################
#                   recursive application build target def                     #
################################################################################
DIRS = led ble_adv led_heartbeat timer accelerometer cpuid mesh/embedded_provisioner

################################################################################
#                             BUILD TARGETS                                    #
################################################################################
led_app :
	$(MAKE) -C led all

accelerometer_app :
	$(MAKE) -C accelerometer all

ble_adv_app :
	$(MAKE) -C ble_adv all

led_heartbeat_app :
	$(MAKE) -C led_heartbeat all

timer_app :
	$(MAKE) -C timer all

cpuid_app :
	$(MAKE) -C cpuid all

embedded_provisioner_app :
	$(MAKE) -C mesh/embedded_provisioner all

################################################################################
#                             CLEAN TARGETS                                    #
################################################################################
clean:
	$(foreach DIR,$(DIRS),cd $(DIR) && make clean && cd .. &&) true
