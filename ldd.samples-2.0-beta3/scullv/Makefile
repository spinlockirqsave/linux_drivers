
# Comment/uncomment the following line to enable/disable debugging
#DEBUG = y

# This Makefile has been simplified as much as possible, by putting all
# generic material, independent of this specific directory, into
# ../Rules.make. Read that file for details

TOPDIR  := $(shell cd ..; pwd)
include $(TOPDIR)/Rules.make

ifeq ($(DEBUG),y)
  DEBFLAGS = -O -g -DSCULLV_DEBUG # "-O" is needed to expand inlines
else
  DEBFLAGS = -O2
endif

CFLAGS += $(DEBFLAGS) -I..

TARGET = scullv
SRC = main.c mmap.c

all: .depend $(TARGET).o

$(TARGET).o: $(SRC:.c=.o)
	$(LD) -r $^ -o $@

install:
	install -d $(INSTALLDIR)
	install -c $(TARGET).o $(INSTALLDIR)

clean:
	rm -f *.o *~ core .depend

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > $@


ifeq (.depend,$(wildcard .depend))
include .depend
endif
