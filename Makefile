TARGETS = ds

CC = gcc
OUTPUT_OPTION=-MMD -MP -o $@
CFLAGS = -g -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -Wno-clobbered -Iutil 

util/util_sdl.o: CFLAGS += $(shell sdl2-config --cflags)

SRC_DS = main.c \
         display.c \
         util/util_misc.c \
         util/util_sdl.c \
         util/util_sdl_predefined_panes.c \
         util/util_png.c \
         util/util_jpeg.c

OBJ_DS=$(SRC_DS:.c=.o)

DEP=$(SRC_DS:.c=.d)

#
# build rules
#

all: $(TARGETS)

ds: $(OBJ_DS) 
	$(CC) -pthread -lrt -lm -lpng -ljpeg -lSDL2 -lSDL2_ttf -lSDL2_mixer -o $@ $(OBJ_DS)

-include $(DEP)

#
# clean rule
#

clean:
	rm -f $(TARGETS) $(OBJ_DS) $(DEP)

