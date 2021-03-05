################################################################################
#                           basic animation build file                         #
################################################################################
DIRS = dmadrv gpiointerrupt nvm3 sleep ustimer

all :
	$(foreach DIR,$(DIRS),cd $(DIR) && make && cd .. &&) true

clean:
	$(foreach DIR,$(DIRS),cd $(DIR) && make clean && cd .. &&) true
