#include <stdio.h>
#include <dra_gpio.h>

int main(int argc, char **argv)
{
    gpio_init();

    set_gpio_pin_mode(18, PIN_MODE_OUTPUT);

    while (1) {
        gpio_write(18, 1);
        sleep(1);
        gpio_write(18, 0);
        sleep(1);
    }
        
    return 0;
}

