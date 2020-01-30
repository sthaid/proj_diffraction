#include <bcm2835.h>

int main(int argc, char **argv)
{
    if (!bcm2835_init())
	return 1;

    bcm2835_gpio_fsel(18, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(25, BCM2835_GPIO_FSEL_INPT);

    while (1) {
        if (bcm2835_gpio_lev(25) == HIGH) {
	    bcm2835_gpio_write(18, HIGH);
        } else {
	    bcm2835_gpio_write(18, LOW);
        }
    }

    return 0;
}
