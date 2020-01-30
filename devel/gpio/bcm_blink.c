#include <bcm2835.h>

int main(int argc, char **argv)
{
    if (!bcm2835_init())
	return 1;

    bcm2835_gpio_fsel(18, BCM2835_GPIO_FSEL_OUTP);

    while (1) {
	bcm2835_gpio_write(18, HIGH);
	delay(500);
	bcm2835_gpio_write(18, LOW);
	delay(500);
    }

    return 0;
}
