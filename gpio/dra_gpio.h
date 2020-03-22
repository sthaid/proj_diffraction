#ifndef __DRA_GPIO_H__
#define __DRA_GPIO_H__

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define PIN_MODE_INPUT   1
#define PIN_MODE_OUTPUT  2

volatile unsigned int *gpio_regs;

static inline void gpio_init(void)
{
    #define PI4B_PERIPHERAL_BASE_ADDR 0xfe000000
    #define GPIO_BASE_ADDR            (PI4B_PERIPHERAL_BASE_ADDR + 0x200000)
    #define GPIO_BLOCK_SIZE           0x1000

    int fd;

    if ((fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
        printf("can't open /dev/mem \n");
        exit(-1);
    }

    gpio_regs = mmap(NULL,
                     GPIO_BLOCK_SIZE,
                     PROT_READ | PROT_WRITE,
                     MAP_SHARED,
                     fd,
                     GPIO_BASE_ADDR);

    close(fd);

    if (gpio_regs == MAP_FAILED) {
        printf("mmap failed\n");
        exit(-1);
    }
}

static inline void gpio_write(int pin, int value)
{
    if (value) {
        gpio_regs[7] = (1 << pin);
    } else {
        gpio_regs[10] = (1 << pin);
    }
}

static inline int gpio_read(int pin)
{
    return (gpio_regs[13] & (1 << pin)) != 0;
}

static inline void set_gpio_pin_mode(int pin, int mode)
{
    int mask, value;

    switch (mode) {
    case PIN_MODE_INPUT:
        mask  = (7 << ((pin % 10) * 3));
        value = 0;
        gpio_regs[pin/10] = (gpio_regs[pin/10] & ~mask) | value;
        break;
    case PIN_MODE_OUTPUT:
        mask  = (7 << ((pin % 10) * 3));
        value = (1 << ((pin % 10) * 3));
        gpio_regs[pin/10] = (gpio_regs[pin/10] & ~mask) | value;
        break;
    default:
        printf("ERROR: %s: invalid mode %d\n", __func__, mode);
        exit(1);
        break;
    }
}

#endif
