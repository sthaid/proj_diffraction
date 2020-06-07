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

// XXX set initial volume
// XXX maybe readback volume instead
// XXX read from sd
// XXX run as setuid

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

    // XXX need a thread to read from the sd and discard

    // set initial volume
    audio_set_volume(90);

    // say init text
    audio_say_text("audio init complete");
}

void audio_exit(void)
{
    audio_say_text("audio exitting");

    close(sd);
}

// -----------------  APIS  -----------------------------------------------

void audio_say_text(char *text)
{
    char cmd[1000];
    int ret, len;

    if (mute) {
        return;
    }

    INFO("saying text '%s'\n", text);

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

#if 0
    sprintf(cmd, "amixer set Master Playback Volume %d%% >/dev/null 2>&1", volume);
#else
    sprintf(cmd, "sudo amixer set PCM Playback Volume %d%% >/dev/null 2>&1", volume);
#endif

    INFO("setting volume %d\n", volume);
    system(cmd);
}

void audio_set_mute(bool mute_arg)
{
    mute = mute_arg;
}

int audio_get_volume(void)
{
#if 0 // XXX tbd
    [DS haid@ds xrpm]$ sudo amixer get PCM Playback Volume
    Simple mixer control 'PCM',0
      Capabilities: pvolume pvolume-joined pswitch pswitch-joined
      Playback channels: Mono
      Limits: Playback -10239 - 400
      Mono: Playback -238 [94%] [-2.38dB] [on]
#endif
    return volume;
}

bool audio_get_mute(void)
{
    return mute;
}
