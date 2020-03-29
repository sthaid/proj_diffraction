TARGETS = wpi_blink wpi_switch wpi_timing \
          bcm_blink bcm_switch bcm_timing \
          dra_blink dra_switch dra_timing dra_pulse dra_sipm

CFLAGS = -g -O3 -Wall -lm -lrt -I. -I../util

all: $(TARGETS)

dra_blink: dra_blink.c
	gcc $(CFLAGS) $^ -o $@

dra_switch: dra_switch.c
	gcc $(CFLAGS) $^ -o $@

dra_timing: dra_timing.c ../util/util_misc.c
	gcc $(CFLAGS) $^ -o $@

dra_pulse: dra_pulse.c ../util/util_misc.c
	gcc $(CFLAGS) $^ -o $@

dra_sipm: dra_sipm.c ../util/util_misc.c
	gcc $(CFLAGS) $^ -o $@

wpi_blink: wpi_blink.c
	gcc $(CFLAGS) -lwiringPi $^ -o $@

wpi_switch: wpi_switch.c
	gcc $(CFLAGS) -lwiringPi $^ -o $@

wpi_timing: wpi_timing.c ../util/util_misc.c
	gcc $(CFLAGS) -lwiringPi $^ -o $@

bcm_blink: bcm_blink.c
	gcc $(CFLAGS) $^ -lbcm2835 -o $@

bcm_switch: bcm_switch.c
	gcc $(CFLAGS) $^ -lbcm2835 -o $@

bcm_timing: bcm_timing.c ../util/util_misc.c
	gcc $(CFLAGS) $^ -lbcm2835 -o $@

clean:
	rm -f $(TARGETS)

