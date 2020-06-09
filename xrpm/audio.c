#include "common.h"

#include <sys/socket.h>
#include <netinet/in.h>

//
// defines
//

#define FESTIVAL_PORT  1314

//
// variables
//

static bool mute;
static int  volume;
static int  sd = -1;

// -----------------  INIT / EXIT  ----------------------------------------

// XXX need a thread to read from the sd and discard

void audio_init(void)
{
    int rc;
    struct sockaddr_in sin;

    // connect to festival text-to-speech service
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(FESTIVAL_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sd = socket(AF_INET, SOCK_STREAM, 0);
    // XXX check sd
    rc = connect(sd, (struct sockaddr *)&sin, sizeof(sin));

    if (rc < 0) {
        FATAL("connect to festival failed, %s\n", strerror(errno));
    }

    // set initial volume
    audio_set_volume(90);
}

void audio_exit(void)
{
    close(sd);
}

// -----------------  API - SAY_TEXT  -------------------------------------

void audio_say_text(char *fmt, ...)
{
    char cmd[1100], say_str[1000];
    int ret, len;
    va_list ap;

    if (mute) {
        return;
    }

    va_start(ap, fmt);
    vsnprintf(say_str, sizeof(say_str), fmt, ap);
    va_end(ap);

    INFO("saying '%s'\n", say_str);

    len = sprintf(cmd, "(SayText \"%s\")\n", say_str);
    ret = write(sd, cmd, len);
    if (ret != len) {
        ERROR("write to festival ret=%d, %s\n", ret, strerror(errno));
    }
}

// -----------------  API - VOLUME CONTROL  -------------------------------

void audio_set_volume(int volume_arg)
{
    char cmd[1000];

    volume = volume_arg;
    if (volume < 50) volume = 50;
    if (volume > 100) volume = 100;

    mute = false;

    // note - use "amixer controls" to get list of control names

    sprintf(cmd, "sudo amixer set PCM Playback Volume %d%% >/dev/null 2>&1", volume);

    INFO("setting volume %d\n", volume);
    system(cmd);
}

int audio_change_volume(int delta)
{
    audio_set_volume(volume+delta);
    return volume;
}

int audio_get_volume(void)
{
    // note - can use the following to read volume from amixer
    //        $ sudo amixer get PCM Playback Volume
    return volume;
}

// -----------------  API - MUTE CONTROL  ---------------------------------

void audio_set_mute(bool mute_arg)
{
    mute = mute_arg;
}


bool audio_get_mute(void)
{
    return mute;
}
