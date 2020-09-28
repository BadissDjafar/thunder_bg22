################################################################################
#                   recursive application build target def                     #
################################################################################
DIRS = led accelerometer board_id#ble_adv

################################################################################
#                             BUILD TARGETS                                    #
################################################################################
led_app :
	$(MAKE) -C led all

accelerometer_app :
	$(MAKE) -C accelerometer all

board_id_app :
	$(MAKE) -C board_id all

#ble_adv_app :
#	$(MAKE) -C ble_adv all

################################################################################
#                             CLEAN TARGETS                                    #
################################################################################
clean:
	$(foreach DIR,$(DIRS),cd $(DIR) && make clean && cd .. &&) true
