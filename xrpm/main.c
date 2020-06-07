#include "common.h"

int main(int argc, char **argv)
{
    int volume;

    audio_init();

    volume = audio_get_volume();

    while (true) {
        audio_say_text("hello");  // XXX use varargs, sprintf fmt
        audio_set_volume(volume);
        volume -= 1;
        if (volume < 50) volume = 50;
        sleep(1);
    }

    audio_exit();

    return 0;
}
