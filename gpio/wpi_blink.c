#include <wiringPi.h>

int main (void)
{
    if (wiringPiSetupGpio () == -1)
        return 1;

    pinMode (18, OUTPUT);

    while (1) {
        digitalWrite(18, 1);
        delay(500);
        digitalWrite(18, 0);
        delay(500);
    }

    return 0;
}
