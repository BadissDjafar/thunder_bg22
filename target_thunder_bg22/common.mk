################################################################################
#                     common object build template                             #
################################################################################
all : $(addsuffix .o,$(OBJS))

%.o : %.c
	$(CC) $(CFLAGS) $(IPATH) -o $@ -c $<
	cp $@ $(PRJROOT)

PHONY: clean
clean:
	rm -f *.o *.lib *.obj *.exp
