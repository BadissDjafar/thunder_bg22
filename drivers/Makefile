################################################################################
#                           basic animation build file                         #
################################################################################
DIRS = emlib emdrv

all :
	$(foreach DIR,$(DIRS),cd $(DIR) && make && cd .. &&) true

clean:
	$(foreach DIR,$(DIRS),cd $(DIR) && make clean && cd .. &&) true
