TARGETS = ifsim

CC = gcc
OUTPUT_OPTION=-MMD -MP -o $@
CFLAGS = -g -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-clobbered -Wno-unused-result -I../util 

../util/util_sdl.o: CFLAGS += $(shell sdl2-config --cflags)

SRC_IFSIM = main.c \
            sim.c \
            display.c \
            ../util/util_geometry.c \
            ../util/util_misc.c \
            ../util/util_sdl.c \
            ../util/util_png.c \
            ../util/util_jpeg.c

OBJ_IFSIM=$(SRC_IFSIM:.c=.o)

DEP=$(SRC_IFSIM:.c=.d)

#
# build rules
#

all: $(TARGETS)

ifsim: $(OBJ_IFSIM) 
	$(CC) -o $@ $(OBJ_IFSIM) -pthread -lrt -lm -lpng -ljpeg -lSDL2 -lSDL2_ttf -lSDL2_mixer

-include $(DEP)

#
# clean rule
#

clean:
	rm -f $(TARGETS) $(OBJ_IFSIM) $(DEP)

