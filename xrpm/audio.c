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

bool mute;
int  volume;
int  sd = -1;

// -----------------  INIT / EXIT  ----------------------------------------

void audio_init(void)
{
    int rc;
    struct sockaddr_in sin;

    // connect to festival text-to-speech service

    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(FESTIVAL_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    sd = socket(AF_INET, SOCK_STREAM, 0);
    rc = connect(sd, (struct sockaddr *)&sin, sizeof(sin));

    if (rc < 0) {
        FATAL("connect to festival failed, %s\n", strerror(errno));
    }

    INFO("GOT sd %d\n", sd);

    // XXX need a thread to read from the sd and discard
}

void audio_exit(void)
{
    if (sd != -1) {
        close(sd);
    }
}

// -----------------  APIS  -----------------------------------------------

void audio_say_text(char *text)
{
    char cmd[1000];
    int ret, len;

    if (mute) {
        return;
    }

    len = sprintf(cmd, "(SayText \"%s\")\n", text);
    ret = write(sd, cmd, len);
    if (ret != len) {
        ERROR("write to festival ret=%d, %s\n", ret, strerror(errno));
    }
}

void audio_set_volume(int volume_arg)
{
    char cmd[1000];

    volume = volume_arg;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;

    mute = false;

    // note - use "amixer controls" to get list of control names

    sprintf(cmd, "amixer set Master Playback Volume %d%% >/dev/null 2>&1", volume);
    system(cmd);
}

void audio_set_mute(bool mute_arg)
{
    mute = mute_arg;
}

int audio_get_volume(void)
{
    return volume;
}

bool audio_get_mute(void)
{
    return mute;
}
