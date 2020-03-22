#include <stdio.h>
#include <wiringPi.h>

int main (void)
{
    if (wiringPiSetupGpio() == -1)
        return 1;

    pinMode (18, OUTPUT);
    pinMode (25, INPUT);

    while (1) {
        if (digitalRead(25)) {
            digitalWrite(18, 1);
        } else {
            digitalWrite(18, 0);
        }
    }

  return 0;
}
