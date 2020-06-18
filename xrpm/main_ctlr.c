#include "common_includes.h"

#include <curses.h>
#include <signal.h>

#include "sipm.h"
#include "utils.h"
#include "audio.h"
#include "xrail.h"

//
// defines
//

#define MAX_SIPM_PULSE_RATE_HISTORY  1000000

#define XRAIL_CTRL_CMD_NONE                    0
#define XRAIL_CTRL_CMD_CAL_MOVE_PLUS_ONE_MM    1
#define XRAIL_CTRL_CMD_CAL_MOVE_MINUS_ONE_MM   2
#define XRAIL_CTRL_CMD_CAL_COMPLETE            3
#define XRAIL_CTRL_CMD_TEST                    4

#define XRAIL_CTRL_CMD_STR(x) \
    ((x) == XRAIL_CTRL_CMD_NONE                  ? "XXX" : \
     (x) == XRAIL_CTRL_CMD_CAL_MOVE_PLUS_ONE_MM  ? "XXX" : \
     (x) == XRAIL_CTRL_CMD_CAL_MOVE_MINUS_ONE_MM ? "XXX" : \
     (x) == XRAIL_CTRL_CMD_CAL_COMPLETE          ? "XXX" : \
     (x) == XRAIL_CTRL_CMD_TEST                  ? "XXX" : \
                                                   "XXX")
 
//
// variables
//

static WINDOW * window;

static int sipm_pulse_rate_history[MAX_SIPM_PULSE_RATE_HISTORY];
static int max_sipm_pulse_rate_history;

//
// prototypes
//

static void update_display(int maxy, int maxx);
static int input_handler(int input_char);
static void display_alert(char *msg, ...) __attribute__ ((format (printf, 1, 2)));

static void curses_init(void);
static void curses_exit(void);
static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char));

static void sipm_local_init(void);
static void sipm_local_exit(void);

static void xrail_local_init(void);
static void xrail_local_exit(void);
static void xrail_local_issue_ctrl_cmd(int cmd);
static void xrail_local_cancel_ctrl_cmd(int cmd);

// -----------------  MAIN  --------------------------------------------------

int main(int argc, char **argv)
{
    struct rlimit rl;
    sigset_t set;

    // set resource limti to allow core dumps
    rl.rlim_cur = RLIM_INFINITY;
    rl.rlim_max = RLIM_INFINITY;
    setrlimit(RLIMIT_CORE, &rl);

    // block signal SIGINT
    sigemptyset(&set);
    sigaddset(&set,SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    // init 
    utils_init("ctlr.log");
    audio_init();
    xrail_init();

    sipm_local_init();
    xrail_local_init();

    INFO("INITIALIZATION COMPLETE\n");

    // xxx temp
    while (true) pause();

    // runtime using curses
    curses_init();
    curses_runtime(update_display, input_handler);
    curses_exit();

    // clean up and exit
    INFO("TERMINATING\n");

    xrail_local_exit();
    sipm_local_exit();

    xrail_exit();
    audio_exit();
    utils_exit();

    return 0;
}

// -----------------  CURSES HANDLERS  ---------------------------------------

// XXX  temp, work these 
static int update_count, char_count;

static char     alert_msg[100];
static uint64_t alert_msg_time_us;

static void update_display(int maxy, int maxx)
{
    int row = maxy / 2;
    int col = maxx / 2;

    update_count++;

    mvprintw(0, 0, "maxy=%d maxx=%d chars=%d updates=%d", 
            maxy, maxx, char_count, update_count);

    if (microsec_timer() - alert_msg_time_us < 3000000) {
        mvprintw(1, 0, "%s", alert_msg);
    }

    // XXX sipm value

    // XXX xrail status
}

// return -1 to exit pgm
static int input_handler(int input_char)
{
    switch (input_char) {
    // audio:
    // - v : volume down
    // - ^ : volume up
    // - m : toggle mute
    // - a : audio test 
    case 'v':
        audio_change_volume(-5);
        break;
    case '^':
        audio_change_volume(5);
        break;
    case 'm':
        audio_toggle_mute();
        break;
    case 'a':
        audio_say_text("Open the pod bay doors Hal.");
        break;

    // terminate pgm:
    // - q
    case 'q':
        return -1;
        break;

    // ignore all others xxx maybe warning
    default:
        break;
    }

    // return 0 means don't terminate pgm
    return 0;
}

static void display_alert(char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(alert_msg, sizeof(alert_msg), fmt, ap);
    va_end(ap);

    alert_msg_time_us = microsec_timer();

    // xxx update display now
}

// -----------------  CURSES WRAPPER  ----------------------------------------

static void curses_init(void)
{
    window = initscr();
    cbreak();
    noecho();
    nodelay(window,TRUE);
    keypad(window,TRUE);
}

static void curses_exit(void)
{
    endwin();
}

static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char))
{
    int input_char, maxy, maxx;

    while (true) {
        // erase display
        erase();

        // update the display
        getmaxyx(window, maxy, maxx);
        update_display(maxy, maxx);

        // put the cursor back to the origin, and
        // refresh display
        move(0,0);
        refresh();

        // process character inputs
        input_char = getch();
        if (input_char == KEY_RESIZE) {
            // loop around and redraw display
        } else if (input_char != ERR) {
            if (input_handler(input_char) != 0) {
                return;
            }
        } else {
            // xxx sleep duration, and sleep with recent kbd input
            usleep(100000);
        }
    }
}

// -----------------  SIPM DATA COLLECTION  ----------------------------------

// variables
pthread_t   sipm_data_collection_thread_id;
static int  sipm_server_sd;
static bool sipm_local_terminate;

// prototypes
static void * sipm_data_collection_thread(void *cx);
static int sipm_server_get_rate(struct get_rate_s *get_rate);

static void sipm_local_init(void)
{
    int optval, rc;
    struct sockaddr_in sin;

    // connect to sipm_server xxx hardcoded name here
    rc = getsockaddr("ds", SIPM_SERVER_PORT, &sin);
    if (rc != 0) {
        FATAL("getsockaddr failed\n");
    }
    sipm_server_sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sipm_server_sd == -1) {
        FATAL("failed to create socket, %s\n",strerror(errno));
    }
    rc = connect(sipm_server_sd, (struct sockaddr *)&sin, sizeof(sin));
    if (rc < 0) {
        FATAL("connect to sipm_server failed, %s\n", strerror(errno));
    }

    // enable TCP_NODELAY socket option
    optval = 1;
    rc = setsockopt(sipm_server_sd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    if (rc == -1) {
        FATAL("setsockopt TCP_NODELAY, %s", strerror(errno));
    }

    // create sipm_data_collection_thread
    pthread_create(&sipm_data_collection_thread_id, NULL, sipm_data_collection_thread, NULL);
}

static void sipm_local_exit(void)
{
    sipm_local_terminate = true;
    pthread_join(sipm_data_collection_thread_id, NULL);
}

static void *sipm_data_collection_thread(void *cx)
{
    uint64_t          thread_start_us, delay_us;
    uint64_t          get_rate_start_us, get_rate_complete_us;
    int               rc, idx;
    struct get_rate_s get_rate;

    #define TIME_NOW_US  (microsec_timer() - thread_start_us)

    // Every 500ms request sipm data from sipm_server.
    // Store the pulse_rate data collected sipm_pulse_rate[], indexed by seconds.
    // Store the latest pulse_rate, and other info, so it can be displayed and used by
    //  the xxx thread.

    // xxx enable real time

    thread_start_us = microsec_timer();

    while (true) {
        // if terminate requested then return
        if (sipm_local_terminate) {
            return NULL;
        }

        // sleep until next 500ms time boundary
        delay_us = 500000 - (TIME_NOW_US % 500000);
        usleep(delay_us);

        // request data from sipm_server
        get_rate_start_us = TIME_NOW_US;
        rc = sipm_server_get_rate(&get_rate);
        if (rc != 0) {
            FATAL("sipm_server_get_rate failed\n");
        }
        get_rate_complete_us = TIME_NOW_US;

        // warn if the sipm_server_get_rate took a long time
        if (get_rate_complete_us - get_rate_start_us > 200000) {
            INFO("sipm_server_get_rate long duration %0.2f secs\n",
                 (get_rate_complete_us - get_rate_start_us) / 1000000.);
        }

        // store the pulse_rate in sipm_pulse_rate
        idx = get_rate_start_us / 1000000;
        //INFO("STORING SIPM_PULSE_RATE idx=%d  value=%d  start_sec=%0.1f\n", 
        //     idx, get_rate.pulse_rate, get_rate_start_us/1000000.);
        sipm_pulse_rate_history[idx] = get_rate.pulse_rate;
        __sync_synchronize();
        max_sipm_pulse_rate_history = idx+1;

        // store the latest_sipm_data
        // XXX tbd
    }

    return NULL;
}

// xxx may want a common messageing routine
static int sipm_server_get_rate(struct get_rate_s *get_rate)
{
    msg_request_t      msg_req;
    msg_response_t     msg_resp;
    int                len;

    static int         seq_num;

    msg_req.magic   = MSG_REQ_MAGIC;
    msg_req.id      = MSGID_GET_RATE;
    msg_req.seq_num = ++seq_num;
    len = do_send(sipm_server_sd, &msg_req, sizeof(msg_req));
    if (len != sizeof(msg_req)) {
        ERROR("do_send ret=%d, %s\n", len, strerror(errno));
        return -1;
    }

    len = do_recv(sipm_server_sd, &msg_resp, sizeof(msg_resp));
    if (len != sizeof(msg_resp)) {
        ERROR("do_recv ret=%d, %s\n", len, strerror(errno));
        return -1;
    }

    if (msg_resp.magic != MSG_RESP_MAGIC ||
        msg_resp.id != MSGID_GET_RATE ||
        msg_resp.seq_num != seq_num)
    {
        ERROR("invalid msg_resp %d %d %d, exp_seq_num=%d\n",
              msg_resp.magic,
              msg_resp.id,
              msg_resp.seq_num,
              seq_num);
        return -1;
    }

    *get_rate = msg_resp.get_rate;
    return 0;
}

// -----------------  XRAIL CONTROL & STATUS  --------------------------------

static pthread_t xrail_ctrl_thread_id;
static pthread_t xrail_status_thread_id;
static bool      xrail_local_terminate;
static bool      xrail_calibrated;
static int       xrail_ctrl_cmd;
static bool      xrail_ctrl_cmd_cancel;

static void * xrail_ctrl_thread(void *cx);
static void * xrail_status_thread(void *cx);

static void xrail_local_init(void)
{
    // create the xrail control and status threads
    pthread_create(&xrail_ctrl_thread_id, NULL, xrail_ctrl_thread, NULL);
    pthread_create(&xrail_status_thread_id, NULL, xrail_status_thread, NULL);
}

static void xrail_local_exit(void)
{
    // cause the xrail ctrl and status threads to exit
    xrail_local_terminate = true;
    pthread_join(xrail_ctrl_thread_id, NULL);
    pthread_join(xrail_status_thread_id, NULL);
}

static void xrail_local_issue_ctrl_cmd(int cmd)
{
    // if a cmd is in progress then it is an error
    if (xrail_ctrl_cmd != XRAIL_CTRL_CMD_NONE) {
        display_alert("%s is in progress", XRAIL_CTRL_CMD_STR(xrail_ctrl_cmd));
        return;
    }

    // check if the cmd is valid for current state of xrail_calibrated
    // XXX

    // issue the cmd
    xrail_ctrl_cmd_cancel = false;
    __sync_synchronize();
    xrail_ctrl_cmd = cmd;
}

static void xrail_local_cancel_ctrl_cmd(int cmd)
{
    xrail_ctrl_cmd_cancel = true;
}

static void * xrail_ctrl_thread(void *cx)
{
    #define CHECK_FOR_CANCEL_REQ \
        do { \
            if (xrail_ctrl_cmd_cancel || xrail_local_terminate) { \
                goto cancel; \
            } \
        } while (0)

    while (true) {
        // if terminate requested then return
        if (xrail_local_terminate) {
            return NULL;
        }

        // if no cmd then delay and continue
        if (xrail_ctrl_cmd == XRAIL_CTRL_CMD_NONE) {
            usleep(1000);
            continue;
        }

        // switch on the cmd
        switch (xrail_ctrl_cmd) {
        case XRAIL_CTRL_CMD_CAL_MOVE_PLUS_ONE_MM:
            xrail_cal_move(1);
            break;
        case XRAIL_CTRL_CMD_CAL_MOVE_MINUS_ONE_MM:
            xrail_cal_move(-1);
            break;
        case XRAIL_CTRL_CMD_CAL_COMPLETE:
            xrail_cal_complete();
            xrail_calibrated = true;
            break;
        case XRAIL_CTRL_CMD_TEST:
            xrail_goto_location(0, true);
            CHECK_FOR_CANCEL_REQ;
            xrail_goto_location(-25, true);
            CHECK_FOR_CANCEL_REQ;
            xrail_goto_location(25, true);
            CHECK_FOR_CANCEL_REQ;
            xrail_goto_location(0, true);
            CHECK_FOR_CANCEL_REQ;
            break;
        default:
            FATAL("invalid xrail_ctrl_cmd %d\n", xrail_ctrl_cmd);
            break;
        }

cancel:
        // if cancelled then issue message
        if (xrail_ctrl_cmd_cancel) {
            display_alert("%s is cancelled", XRAIL_CTRL_CMD_STR(xrail_ctrl_cmd));
        }

        // clearing the cmd indicates it has completed
        xrail_ctrl_cmd = 0;
    }

    return NULL;
}

static void * xrail_status_thread(void *cx)
{
    bool okay, calibrated;
    double curr_loc_mm, tgt_loc_mm, voltage;
    char status_str[200];

    // get the xrail status
    while (true) {
        // if terminate requested then return
        if (xrail_local_terminate) {
            return NULL;
        }

        // xxx
        xrail_get_status(&okay, &calibrated, &curr_loc_mm, &tgt_loc_mm, &voltage, status_str);

        // XXX publish strings, and display them

        // one sec sleep
        sleep(1);
    }

    return NULL;
}


#if 0
// -----------------  TBD  ---------------------------------------------------

void *test_thread(void *cx)
{
    uint64_t           start_us, duration_us;
    int                len, optval, rc, seq_num=0, sfd, count;



    // get pulse_rate from sipm_server
    // XXX comments
    count = 0;
    while (true) {
        start_us = microsec_timer();


        duration_us = microsec_timer() - start_us;

        // XXX do something about this print
        printf("dur_us=%d pulse_rate=%d K gpio_read_rte=%d M gpio_read_and_analyze_rate=%d M\n",
               (int)duration_us,
               msg_resp.get_rate.pulse_rate / 1000,
               msg_resp.get_rate.gpio_read_rate / 1000000,
               msg_resp.get_rate.gpio_read_and_analyze_rate / 1000000);

        // test audio too
        if ((count++ % 5) == 0) {
            audio_say_text("%d", msg_resp.get_rate.pulse_rate/1000);
        }

        usleep(500000);
    }
}
#endif