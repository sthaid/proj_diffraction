TARGETS = xrpm

CC = gcc
OUTPUT_OPTION=-MMD -MP -o $@
CFLAGS = -g -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-clobbered -I../util 

SRC_XRPM = main.c \
           utils.c \
           audio.c \
           sipm.c \
           xrail.c

OBJ_XRPM=$(SRC_XRPM:.c=.o)

DEP=$(SRC_XRPM:.c=.d)

#
# build rules
#

all: $(TARGETS)

xrpm: $(OBJ_XRPM) 
	$(CC) -pthread -lrt -o $@ $(OBJ_XRPM)

-include $(DEP)

#
# clean rule
#

clean:
	rm -f $(TARGETS) $(OBJ_XRPM) $(DEP)

