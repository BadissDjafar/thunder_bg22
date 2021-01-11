################################################################################
#                   recursive application build target def                     #
################################################################################
DIRS = led led_heartbeat ble_adv accelerometer accelerometer_ble timer

################################################################################
#                             BUILD TARGETS                                    #
################################################################################
led_app :
	$(MAKE) -C led all

led_heartbeat_app :
	$(MAKE) -C led_heartbeat all

timer_app :
	$(MAKE) -C timer all

ble_adv_app :
	$(MAKE) -C ble_adv all

accelerometer_app :
	$(MAKE) -C accelerometer all

accelerometer_ble_app :
	$(MAKE) -C accelerometer_ble all

################################################################################
#                             CLEAN TARGETS                                    #
################################################################################
clean:
	$(foreach DIR,$(DIRS),cd $(DIR) && make clean && cd .. &&) true
