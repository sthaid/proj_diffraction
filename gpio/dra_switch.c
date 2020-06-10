#include <stdio.h>
#include <dra_gpio.h>

#define GPIO_INPUT_PIN       20
#define GPIO_OUTPUT_LED_PIN  21

int main(int argc, char **argv)
{
    if (gpio_init() != 0) {
        return 1;
    }

    set_gpio_pin_mode(GPIO_INPUT_PIN, PIN_MODE_INPUT);
    set_gpio_pin_mode(GPIO_OUTPUT_LED_PIN, PIN_MODE_OUTPUT);

    while (1) {
        if (gpio_read(GPIO_INPUT_PIN)) {
            printf("IS ONE\n");
            system("echo ONE | festival --tts");
            gpio_write(GPIO_OUTPUT_LED_PIN, 1);
        } else {
            printf("IS ZERO\n");
            system("echo ZERO | festival --tts");
            gpio_write(GPIO_OUTPUT_LED_PIN, 0);
        }
        sleep(1);
    }

    return 0;
}

