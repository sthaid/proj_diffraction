#include "common_includes.h"

#include "audio.h"
#include "utils.h"

//
// notes
//
// - to auto start festival on raspberry pi - add this to /etc/rc.local
//     # start festival
//     /usr/bin/festival --server >/var/log/festival.log 2>&1 &
//
// - use "amixer controls" to get list of control names
//
// - if running this code on Raspberry Pi:
//   . could use the following to read volume from amixer
//       sudo amixer get PCM Playback Volume
//   . to set the volume, this might work
//       sudo amixer set PCM Playback Volume 90%
//

//
// defines
//

// define this when testing on Fedora
//#define UNITTEST_FEDORA_AUDIO

#define FESTIVAL_PORT  1314

//
// variables
//

static bool mute;
static int  volume;
static int  sd = -1;

//
// prototypes
//

static void *read_festival_stdout_thread(void *cx);

// -----------------  INIT / EXIT  ----------------------------------------

void audio_init(void)
{
    int rc, optval;
    struct sockaddr_in sin;
    pthread_t tid;

    // connect to festival text-to-speech service
    sin.sin_family      = AF_INET;
    sin.sin_port        = htons(FESTIVAL_PORT);
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        FATAL("failed to create socket, %s\n",strerror(errno));
    }
    rc = connect(sd, (struct sockaddr *)&sin, sizeof(sin));
    if (rc < 0) {
        FATAL("connect to festival failed, %s\n", strerror(errno));
    }

    // enable TCP_NODELAY socket option
    optval = 1;
    rc = setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    if (rc == -1) {
        FATAL("setsockopt TCP_NODELAY, %s", strerror(errno));
    }

    // create thread to read from festival stdout, and discard
    pthread_create(&tid, NULL, read_festival_stdout_thread, NULL);

    // set initial volume
    audio_set_volume(90, false);
}

void audio_exit(void)
{
    close(sd);
}

static void *read_festival_stdout_thread(void *cx)
{
    int rc;
    char buff[1000];

    while (true) {
        rc = read(sd, buff, sizeof(buff)-1);
        if (rc < 0) {
            ERROR("festival stdout read failed, rc=%d, %s\n", rc, strerror(errno));
            continue;
        }
        //buff[rc] = 0;
        //INFO("rc=%d buff='%s'\n", rc, buff);
    }

    return NULL;
}

// -----------------  API - SAY_TEXT  -------------------------------------

void audio_say_text(char *fmt, ...)
{
    char cmd[1100], say_str[1000];
    int rc, len;
    va_list ap;

    // if muted just return
    if (mute) {
        return;
    }

    // create string from callers fmt,varargs
    va_start(ap, fmt);
    vsnprintf(say_str, sizeof(say_str), fmt, ap);
    va_end(ap);

    // create cmd string for festival-text-to-speech, and write to festival sd
    len = sprintf(cmd, "(SayText \"%s\")\n", say_str);
    rc = write(sd, cmd, len);
    if (rc != len) {
        ERROR("write to festival rc=%d, %s\n", rc, strerror(errno));
    }
}

// -----------------  API - VOLUME CONTROL  -------------------------------

void audio_set_volume(int volume_arg, bool say)
{
    char cmd[1000];

    // set volume to volume_arg, but clip to range 50% - 100%
    volume = volume_arg;
    if (volume < 50) volume = 50;
    if (volume > 100) volume = 100;

    // clear mute
    audio_set_mute(false, say);

    // use system to run amixer and set the volume
#ifdef UNITTEST_FEDORA_AUDIO
    sprintf(cmd, "amixer set Master Playback Volume %d%% >/dev/null 2>&1", volume);
#else  // Raspberry Pi
    sprintf(cmd, "sudo amixer set Headphone Playback Volume %d%% >/dev/null 2>&1", volume);
#endif
    system(cmd);

    // say the new volume
    if (say) {
        audio_say_text("%d percent", volume);
    }
}

void audio_change_volume(int delta, bool say)
{
    audio_set_volume(volume+delta, say);
}

int audio_get_volume(void)
{
    return volume;
}

// -----------------  API - MUTE CONTROL  ---------------------------------

void audio_set_mute(bool mute_arg, bool say)
{
    if (mute == mute_arg) {
        return;
    }

    if (mute) {
        mute = false;
        if (say) {
            audio_say_text("un-muted");
        }
    } else {
        if (say) {
            audio_say_text("muted");
        }
        mute = true;
    }
}

bool audio_get_mute(void)
{
    return mute;
}

void audio_toggle_mute(bool say)
{
    audio_set_mute(!mute, say);
}
