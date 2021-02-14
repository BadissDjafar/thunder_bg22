#TBD add the license

################################################################################
#                               PROJECT CONFIG                                 #
################################################################################
include .config

################################################################################
#                              EXPORTS TO SUBMK                                #
################################################################################
export CC = $(CONFIG_CROSS_CC)
export LD = $(CONFIG_CROSS_LD)
export OBJCPY = $(CONFIG_CROSS_OBJCPY)
export DUMP = $(CONFIG_CROSS_OBJDUMP)
export PRJROOT = $(CONFIG_PRJROOT)
export MAKE = $(CONFIG_MAKE)

# include/lib path
export IPATH = -I$(PRJROOT)/include -I$(PRJROOT)/include/CMSIS/include -I$(PRJROOT)/drivers/emlib -I$(PRJROOT)/drivers/emdrv/gpiointerrupt -I$(PRJROOT)/libs/efr32bg22/RF/radio -I$(PRJROOT)/libs/efr32bg22/RF/bluetooth -I$(PRJROOT)/soc/efr32bg22 -I$(PRJROOT)/boards/thunderboard_bg22  
export LPATH = -L$(PRJROOT)/libs/efr32bg22/RF/bluetooth -L$(PRJROOT)/libs/efr32bg22/RF/radio -L$(PRJROOT)/drivers/emdrv/nvm3

# compilation/linking flag
export CFLAGS = -g -gdwarf-2 -mcpu=cortex-m33 -mthumb -std=c99 -DNVM3_DEFAULT_NVM_SIZE=24576 -DHAL_CONFIG=1 -D__StackLimit=0x20000000 -D__HEAP_SIZE=0xD00 -D__STACK_SIZE=0x800 -DEFR32BG22C224F512IM40=1 -mfpu=fpv5-sp-d16 -mfloat-abi=hard
#export LDFLAGS = --gc-sections -nostartfiles -T $(PRJROOT)/soc/efr32bg22/efr32bg22c224f512im40.ld
export LDFLAGS = -g -gdwarf-2 -mcpu=cortex-m33 -mthumb -T $(PRJROOT)/soc/efr32bg22/efr32bg22c224f512im40.ld -Xlinker --gc-sections -Xlinker -Map="system.map" -mfpu=fpv5-sp-d16 -mfloat-abi=hard --specs=nano.specs -lm 

################################################################################
#                               directories layout                             #
################################################################################
CLEAN_DIRS = soc/efr32bg22 drivers applications boards/thunderboard_bg22

################################################################################
#                           APP TARGET DEFINITIONS                             #
################################################################################
default : usage

all : led ble_adv

menuconfig : 
	@echo -e "\033[1;35m[Menuconfig $@]\033[0m"
	python3.exe menuconfig.py Kconfig
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

led : led_compile
	@echo -e "\033[1;35m[Linking $@]\033[0m"
	$(CC) $(LDFLAGS) $(LPATH) *.o -lrail_efr32xg22_gcc_release $(PRJROOT)/libs/efr32bg22/RF/bluetooth/binapploader.o -lnvm3_CM33_gcc -o $@.elf -lgcc -lc -lnosys

	@echo -e "\033[1;35m[OBJCPY ->> HEX & SREC images $@]\033[0m"
	$(OBJCPY) -O ihex $@.elf $@.hex
	$(OBJCPY) -O srec $@.elf $@.srec
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

led_heartbeat : led_heartbeat_compile
	@echo -e "\033[1;35m[Linking $@]\033[0m"
	$(CC) $(LDFLAGS) $(LPATH) *.o -lrail_efr32xg22_gcc_release $(PRJROOT)/libs/efr32bg22/RF/bluetooth/binapploader.o -lnvm3_CM33_gcc -o $@.elf -lgcc -lc -lnosys

	@echo -e "\033[1;35m[OBJCPY ->> HEX & SREC images $@]\033[0m"
	$(OBJCPY) -O ihex $@.elf $@.hex
	$(OBJCPY) -O srec $@.elf $@.srec
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

timer : timer_compile
	@echo -e "\033[1;35m[Linking $@]\033[0m"
	$(CC) $(LDFLAGS) $(LPATH) *.o -lrail_efr32xg22_gcc_release $(PRJROOT)/libs/efr32bg22/RF/bluetooth/binapploader.o -lnvm3_CM33_gcc -o $@.elf -lgcc -lc -lnosys

	@echo -e "\033[1;35m[OBJCPY ->> HEX & SREC images $@]\033[0m"
	$(OBJCPY) -O ihex $@.elf $@.hex
	$(OBJCPY) -O srec $@.elf $@.srec
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

ble_adv : ble_adv_compile
	@echo -e "\033[1;35m[Linking $@]\033[0m"
	$(CC) $(LDFLAGS) $(LPATH) *.o -lbluetooth -lrail_efr32xg22_gcc_release -lmbedtls $(PRJROOT)/libs/efr32bg22/RF/bluetooth/binapploader.o -lnvm3_CM33_gcc -o $@.elf -lgcc -lc -lnosys

	@echo -e "\033[1;35m[OBJCPY ->> HEX & SREC images $@]\033[0m"
	$(OBJCPY) -O ihex $@.elf $@.hex
	$(OBJCPY) -O srec $@.elf $@.srec
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

accelerometer : accelerometer_compile
	@echo -e "\033[1;35m[Linking $@]\033[0m"
	$(CC) $(LDFLAGS) $(LPATH) *.o -o $@.elf -lgcc -lc -lnosys

	@echo -e "\033[1;35m[OBJCPY ->> HEX & SREC images $@]\033[0m"
	$(OBJCPY) -O ihex $@.elf $@.hex
	$(OBJCPY) -O srec $@.elf $@.srec
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

board_id : board_id_compile
	@echo -e "\033[1;35m[Linking $@]\033[0m"
	$(CC) $(LDFLAGS) $(LPATH) *.o -o $@.elf -lgcc -lc -lnosys

	@echo -e "\033[1;35m[OBJCPY ->> HEX & SREC images $@]\033[0m"
	$(OBJCPY) -O ihex $@.elf $@.hex
	$(OBJCPY) -O srec $@.elf $@.srec
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

################################################################################
#                             BUILD TARGETS                                    #
################################################################################
led_compile : dev drv
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C applications led_app
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

led_heartbeat_compile : dev drv
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C applications led_heartbeat_app
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

timer_compile : dev drv
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C applications timer_app
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

ble_adv_compile : dev drv
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C applications ble_adv_app
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

accelerometer_compile : dev drv
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C applications accelerometer_app
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

board_id_compile : dev drv
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C applications board_id_app
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

dev :
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C soc/efr32bg22 all
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

drv :
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C boards/thunderboard_bg22 all
	$(MAKE) -C drivers all
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

################################################################################
#                          CLEAN/USAGE TARGETS                                 #
################################################################################

usage list help:
	@echo -e "\033[1;36m[Listing available target for build : ]\033[0m"
	@echo -e "\033[1;36m[	all             - builds all applications ]\033[0m"
	@echo -e "\033[1;36m[	led             - builds the LED app      ]\033[0m"
#	@echo -e "\033[1;36m[	ble_adv         - builds the apps         ]\033[0m"
#	@echo -e "\033[1;36m[	accelerometer   - builds the apps         ]\033[0m"
	@echo -e "\033[1;36m[	clean           - clean all built files   ]\033[0m"

PHONY := clean all
clean:
	$(foreach DIR,$(CLEAN_DIRS),cd $(DIR) && make clean && cd $(PRJROOT) &&) true
	rm *.a *.o *~ *.hex *.srec *.elf *.map

#Black        0;30     Dark Gray     1;30
#Red          0;31     Light Red     1;31
#Green        0;32     Light Green   1;32
#Brown/Orange 0;33     Yellow        1;33
#Blue         0;34     Light Blue    1;34
#Purple       0;35     Light Purple  1;35
#Cyan         0;36     Light Cyan    1;36
#Light Gray   0;37     White         1;37
#
#APPS = sha256_fast sha256 matrixMul_0 matrixMul_1 matrixMul_2
#
#all :
#	$(foreach APP,$(APPS),cd $(APP) && make && cd .. &&) true
#
#clean:
#	$(foreach APP,$(APPS),cd $(APP) && make clean && cd .. &&) true
