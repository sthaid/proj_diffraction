#include "common.h"

// XXX get ctrlc and use this to exit orderly

int main(int argc, char **argv)
{
    utils_init();
    audio_init();
    xrail_init();
    INFO("INITIALIZATION COMPLETE\n");

#if 0
    int volume;
    volume = audio_get_volume();
    while (true) {
        audio_say_text("hello");  // XXX use varargs, sprintf fmt
        audio_set_volume(volume);
        volume -= 1;
        if (volume < 50) volume = 50;
        sleep(1);
    }
#endif
    // XXX unit test xrail

    sleep(5);

    INFO("TERMINATING\n");
    xrail_exit();
    audio_exit();
    utils_exit();

    return 0;
}
