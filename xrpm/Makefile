TARGETS = unit_test sipm_server ctlr

CC = gcc
OUTPUT_OPTION=-MMD -MP -o $@
CFLAGS = -g -O2 \
         -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-clobbered -Werror \
         -I/usr/local/include/libpololu-tic-1
# XXX
CFLAGS = -g -O2 \
         -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-clobbered \
         -I/usr/local/include/libpololu-tic-1

SRC_UNIT_TEST   = main_unit_test.c \
                  utils.c \
                  audio.c \
                  sipm.c \
                  xrail.c
SRC_SIPM_SERVER = main_sipm_server.c \
                  utils.c \
                  sipm.c 
SRC_CTLR        = main_ctlr.c \
                  utils.c \
                  audio.c \
                  xrail.c

DEP=$(SRC_UNIT_TEST:.c=.d) \
    $(SRC_SIPM_SERVER:.c=.d) \
    $(SRC_CTLR:.c=.d)

#
# build rules
#

all: $(TARGETS)

unit_test: $(SRC_UNIT_TEST:.c=.o)
	$(CC) -pthread -lreadline -lm -lpololu-tic-1 -o $@ $(SRC_UNIT_TEST:.c=.o)

sipm_server: $(SRC_SIPM_SERVER:.c=.o)
	$(CC) -pthread -lm -o $@ $(SRC_SIPM_SERVER:.c=.o)

ctlr: $(SRC_CTLR:.c=.o)
	$(CC) -pthread -lncurses -lm -lpololu-tic-1 -o $@ $(SRC_CTLR:.c=.o)

-include $(DEP)

#
# clean rule
#

clean:
	rm -f $(TARGETS) $(DEP) $(DEP:.d=.o)

