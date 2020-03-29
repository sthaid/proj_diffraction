#include <stdio.h>
#include <dra_gpio.h>

int main(int argc, char **argv)
{
    if (gpio_init() != 0) {
        return 1;
    }

    set_gpio_pin_mode(25, PIN_MODE_INPUT);
    set_gpio_pin_mode(18, PIN_MODE_OUTPUT);

    while (1) {
        if (gpio_read(25)) {
            gpio_write(18, 1);
        } else {
            gpio_write(18, 0);
        }
    }

    return 0;
}

