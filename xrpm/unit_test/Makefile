TARGETS = unit_test

CC = gcc
OUTPUT_OPTION=-MMD -MP -o $@
CFLAGS = -g -O2 \
         -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-clobbered -Werror \
         -I/usr/local/include/libpololu-tic-1

SRC_UNIT_TEST   = main_unit_test.c \
                  utils.c \
                  audio.c \
                  sipm.c \
                  xrail.c

DEP=$(SRC_UNIT_TEST:.c=.d)

#
# build rules
#

all: $(TARGETS)

unit_test: $(SRC_UNIT_TEST:.c=.o)
	$(CC) -pthread -lreadline -lm -lpololu-tic-1 -o $@ $(SRC_UNIT_TEST:.c=.o)

-include $(DEP)

#
# clean rule
#

clean:
	rm -f $(TARGETS) $(DEP) $(DEP:.d=.o)

