#TBD add the license

################################################################################
#                               PROJECT CONFIG                                 #
################################################################################
include config.mk

################################################################################
#                              EXPORTS TO SUBMK                                #
################################################################################
export CC = $(CROSS_CC)
export LD = $(CROSS_LD)
export OBJCPY = $(CROSS_OBJCPY)
export DUMP = $(CROSS_OBJDUMP)

# include/lib path
export IPATH = -I$(PRJROOT)/include -I$(PRJROOT)/include/CMSIS/include -I$(PRJROOT)/config -I$(PRJROOT)/drivers/emlib -I$(PRJROOT)/drivers/bsp -I$(PRJROOT)/common -I$(PRJROOT)/device -I$(PRJROOT)/RF/radio -I$(PRJROOT)/RF/bluetooth
export LPATH = -L$(PRJROOT)/RF/bluetooth -L$(PRJROOT)/RF/radio -L$(PRJROOT)/drivers/emdrv/nvm3

# compilation/linking flag
export CFLAGS = -g -gdwarf-2 -mcpu=cortex-m33 -mthumb -std=c99 -DNVM3_DEFAULT_NVM_SIZE=24576 -DHAL_CONFIG=1 -D__StackLimit=0x20000000 -D__HEAP_SIZE=0xD00 -D__STACK_SIZE=0x800 -DEFR32BG22C224F512IM40=1 -mfpu=fpv5-sp-d16 -mfloat-abi=hard
#export LDFLAGS = --gc-sections -nostartfiles -T $(PRJROOT)/device/efr32bg22c224f512im40.ld
export LDFLAGS = -g -gdwarf-2 -mcpu=cortex-m33 -mthumb -T $(PRJROOT)/device/efr32bg22c224f512im40.ld -Xlinker --gc-sections -Xlinker -Map="system.map" -mfpu=fpv5-sp-d16 -mfloat-abi=hard --specs=nano.specs -lm 

################################################################################
#                               directories layout                             #
################################################################################
CLEAN_DIRS = common device drivers applications

################################################################################
#                           APP TARGET DEFINITIONS                             #
################################################################################
default : usage

all : led

led : led_compile
	@echo -e "\033[1;35m[Linking $@]\033[0m"
	#$(LD) $(LDFLAGS) $(LPATH) -Map="soc-empty.map" -lnvm3_CM33_gcc -lrail_efr32xg22_gcc_release -lbluetooth -lmbedtls $(PRJROOT)/RF/bluetooth/binapploader.o *.o -o $@.elf
	$(CC) $(LDFLAGS) $(LPATH) *.o -lbluetooth -lrail_efr32xg22_gcc_release -lmbedtls $(PRJROOT)/RF/bluetooth/binapploader.o -lnvm3_CM33_gcc -o $@.elf -lgcc -lc -lnosys

	@echo -e "\033[1;35m[OBJCPY ->> HEX & SREC images $@]\033[0m"
	$(OBJCPY) -O ihex $@.elf $@.hex
	$(OBJCPY) -O srec $@.elf $@.srec
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

################################################################################
#                             BUILD TARGETS                                    #
################################################################################
led_compile : comn dev drv
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C applications led_app
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

comn :
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C common all
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

dev :
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
	$(MAKE) -C device all
	@echo -e  "\033[1;32m[Finished $@]\033[0m"

drv :
	@echo -e "\033[1;32m[Compiling $@]\033[0m"
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
	$(foreach DIR,$(CLEAN_DIRS),cd $(DIR) && make clean && cd .. &&) true
	rm *.a *.o *~ 

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
