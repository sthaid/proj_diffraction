TARGETS = wpi_blink wpi_switch wpi_timing \
          bcm_blink bcm_switch bcm_timing

CFLAGS = -g -O2 -Wall -lm -lrt

all: $(TARGETS)

wpi_blink: wpi_blink.c
	gcc $(CFLAGS) -lwiringPi $^ -o $@

wpi_switch: wpi_switch.c
	gcc $(CFLAGS) -lwiringPi $^ -o $@

wpi_timing: wpi_timing.c ../../util/util_misc.c
	gcc $(CFLAGS) -lwiringPi $^ -o $@

bcm_blink: bcm_blink.c
	gcc $(CFLAGS) $^ -lbcm2835 -o $@

bcm_switch: bcm_switch.c
	gcc $(CFLAGS) $^ -lbcm2835 -o $@

bcm_timing: bcm_timing.c ../../util/util_misc.c
	gcc $(CFLAGS) $^ -lbcm2835 -o $@

clean:
	rm -f $(TARGETS)




#bcm_blink.c
#wpi_blink.c
#wpi_switch.c
#wpi_timing.c
#gcc -o timing  -lwiringPi timing.c ../../util/util_misc.c
