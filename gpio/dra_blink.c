#include <stdio.h>
#include <dra_gpio.h>

#define GPIO_PIN 21

int main(int argc, char **argv)
{
    if (gpio_init() != 0) {
        return 1;
    }

    set_gpio_pin_mode(GPIO_PIN, PIN_MODE_OUTPUT);

    while (1) {
        printf("TURN ON\n");
        gpio_write(GPIO_PIN, 1);
        sleep(10);
        printf("TURN OFF\n");
        gpio_write(GPIO_PIN, 0);
        sleep(10);
    }
        
    return 0;
}

