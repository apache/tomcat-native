# That an extract of what is in APR.
#

# may be libtool as to be the one of APR (that just not to forget it).
LIBTOOL      = libtool

# Compile commands
VPATH=.:../common
COMPILE      = $(CC) $(CFLAGS)
LT_COMPILE   = $(LIBTOOL) --mode=compile $(COMPILE) -c $< 
# && touch $@

# Implicit rules for creating outputs from input files

.SUFFIXES:
.SUFFIXES: .c .lo .o .slo .s

.c.o:
	$(COMPILE) -c $<

.s.o:
	$(COMPILE) -c $<

.c.lo:
	$(LT_COMPILE)

.s.lo:
	$(LT_COMPILE)

.c.slo:
	$(SH_COMPILE)

