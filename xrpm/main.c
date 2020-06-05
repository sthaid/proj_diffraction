#include "common.h"

int main(int argc, char **argv)
{
    int volume=100;
    audio_init();

    while (true) {
        audio_say_text("hello");  // XXX use varargs, sprintf fmt
        audio_set_volume(volume);
        volume -= 10;
        if (volume < 30) volume = 30;
        sleep(1);
    }

    audio_exit();

    return 0;
}
