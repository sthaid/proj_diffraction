#include "common_includes.h"

#include <curses.h>

#include "sipm.h"
#include "utils.h"
#include "audio.h"

//
// defines
//

#define MAX_SIPM_PULSE_RATE_HISTORY  1000000

//
// variables
//

static WINDOW * window;

static int sipm_server_sd;

static int sipm_pulse_rate_history[MAX_SIPM_PULSE_RATE_HISTORY];
static int max_sipm_pulse_rate_history;

//
// prototypes
//

static void update_display(int maxy, int maxx);
static int input_handler(int input_char);

static void curses_init(void);
static void curses_exit(void);
static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char));

static void start_sipm_data_collection(void);
static void * sipm_data_collection_thread(void *cx);
static int sipm_server_get_rate(struct get_rate_s *get_rate);

// -----------------  MAIN  --------------------------------------------------

int main(int argc, char **argv)
{
    // init files
    utils_init("ctlr.log");
    audio_init();

    // start collecting sipm data by connecting to the sipm_server
    start_sipm_data_collection();
while (true) pause(); //XXX

    // init is complete
    INFO("INITIALIZATION COMPLETE\n");

    // runtime uses curses
    curses_init();
    curses_runtime(update_display, input_handler);
    curses_exit();

    // clean up and exit
    INFO("TERMINATING\n");
    audio_exit();
    utils_exit();

    return 0;
}

// -----------------  CURSES HANDLERS  ---------------------------------------

// XXX  temp
static int update_count, char_count;

static void update_display(int maxy, int maxx)
{
    int row = maxy / 2;
    int col = maxx / 2;

    update_count++;

    mvprintw(row, col, "maxy=%d maxx=%d chars=%d updates=%d", 
            maxy, maxx, char_count, update_count);
}

// return -1 to exit pgm
static int input_handler(int input_char)
{
    if (input_char == 'q') {
        return -1;
    }

    char_count++;
    return 0;
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
            usleep(100000);
        }
    }
}

// -----------------  SIPM DATA COLLECTION  ----------------------------------

static void start_sipm_data_collection(void)
{
    int optval, rc;
    struct sockaddr_in sin;
    pthread_t tid;

    // connect to sipm_server XXX hardcoded name here
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
    pthread_create(&tid, NULL, sipm_data_collection_thread, NULL);
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
    //  the XXX thread.

    // XXX enable real time

    thread_start_us = microsec_timer();

    while (true) {
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

// XXX may want a common messageing routine
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
