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
    ((x) == XRAIL_CTRL_CMD_NONE                  ? "NONE"                  : \
     (x) == XRAIL_CTRL_CMD_CAL_MOVE_PLUS_ONE_MM  ? "CAL_MOVE_PLUS_ONE_MM"  : \
     (x) == XRAIL_CTRL_CMD_CAL_MOVE_MINUS_ONE_MM ? "CAL_MOVE_MINUS_ONE_MM" : \
     (x) == XRAIL_CTRL_CMD_CAL_COMPLETE          ? "CAL_COMPLETE"          : \
     (x) == XRAIL_CTRL_CMD_TEST                  ? "TEST"                  : \
                                                   "????")

#define COLOR_PAIR_RED 1

#define MAX_GRAPH (4+1)

//
// typedefs
//

typedef struct {
    uint64_t time_us;
    uint64_t duration_us;
    int      pulse_rate;
    int      gpio_read_rate;
    int      gpio_read_and_analyze_rate;
} sipm_status_t;

typedef struct {
    bool  exists;
    char *name;
    int  *values;
    int  *max_values;
    int   start_idx;
    int   y_offset;
    int   y_span;
} graph_t;
 
//
// variables
//

static WINDOW      * window;

static int           sipm_pulse_rate_history[MAX_SIPM_PULSE_RATE_HISTORY];
static int           max_sipm_pulse_rate_history;

static char          xrail_status_str[200];
static sipm_status_t sipm_status;

static graph_t       graph[MAX_GRAPH];
static graph_t     * curr_graph;

//
// prototypes
//

static void update_display(int maxy, int maxx);
static int input_handler(int input_char);
static void display_alert(char *msg, ...) __attribute__ ((format (printf, 1, 2)));

static void graph_create(char *name, int *values, int *max_values, int start_idx, int y_offset, int y_span);
static void display_current_graph(int maxy, int maxx);

static void curses_init(void);
static void curses_exit(void);
static void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char));

static void sipm_local_init(void);
static void sipm_local_exit(void);

static void xrail_local_init(void);
static void xrail_local_exit(void);
static void xrail_local_issue_ctrl_cmd(int cmd);
static void xrail_local_cancel_ctrl_cmd(void);

// -----------------  MAIN  --------------------------------------------------

// XXX enforce minimum screen size of 24 rows and 100 columns,  just on startup

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

// -----------------  CURSES DISPLAY AND INPUT HANDLER ROUTINES  -------------

static char     alert_msg[100];
static uint64_t alert_msg_time_us;

static void update_display(int maxy, int maxx)
{
    // display help section, at top left
    mvprintw(0, 0, "--- HELP ---");
    mvprintw(1, 0, "AUDIO: v V m 1");
    mvprintw(2, 0, "XRAIL: + - c <esc> 2");
    mvprintw(3, 0, "GPAPH: %s%s%s%s",
             graph[1].exists ? "F1 " : "",
             graph[2].exists ? "F2 " : "",
             graph[3].exists ? "F3 " : "",
             graph[4].exists ? "F4 " : "");
    mvprintw(4, 0, "QUIT:  q");

    // display status section
    mvprintw(0, 30, "--- STATUS ---");
    mvprintw(1, 30, "AUDIO: %d%% %s", audio_get_volume(), audio_get_mute() ? "MUTED" : "");
    mvprintw(2, 30, "XRAIL: %s", xrail_status_str);
    mvprintw(3, 30, "SIPM:  pulse=%0.1fK gpio=%0.1fM analyze=%0.1fM dur=%dms",
             sipm_status.pulse_rate / 1000.,
             sipm_status.gpio_read_rate / 1000000.,
             sipm_status.gpio_read_and_analyze_rate / 1000000.,
             sipm_status.duration_us / 1000);

    // display alert for 3 secs
    if (microsec_timer() - alert_msg_time_us < 3000000) {
        attron(COLOR_PAIR(COLOR_PAIR_RED));
        mvprintw(5, 0, "%s", alert_msg);
        attroff(COLOR_PAIR(COLOR_PAIR_RED));
    }

    // display the current graph
    display_current_graph(maxy, maxx);
}

// return -1 to exit pgm
static int input_handler(int input_char)
{
    switch (input_char) {
    // audio:
    //  v : volume down
    //  V : volume up
    //  m : toggle mute
    //  1 : audio test
    case 'v':
        audio_change_volume(-5, true);
        break;
    case 'V':
        audio_change_volume(5, true);
        break;
    case 'm':
        audio_toggle_mute(true);
        break;
    case '1':
        audio_say_text("Open the pod bay doors Hal.");
        break;

    // xrail:
    //  +     : cal move 1 mm to right
    //  -     : cal move 1 mm to left
    //  c     : cal done
    //  <esc> : cancel xrail_cmd
    //  2     : xrail test
    case '+':
        xrail_local_issue_ctrl_cmd(XRAIL_CTRL_CMD_CAL_MOVE_PLUS_ONE_MM);
        break;
    case '-':
        xrail_local_issue_ctrl_cmd(XRAIL_CTRL_CMD_CAL_MOVE_MINUS_ONE_MM);
        break;
    case 'c':
        xrail_local_issue_ctrl_cmd(XRAIL_CTRL_CMD_CAL_COMPLETE);
        break;
    case 27:  // ESC
        xrail_local_cancel_ctrl_cmd();
        break;
    case '2':
        xrail_local_issue_ctrl_cmd(XRAIL_CTRL_CMD_TEST);
        break;

    // graph select
    //  F1 ... F4 : select graph 1 through 4
    case KEY_F(1) ... KEY_F(4): {
        int func_key = input_char-KEY_F(0);
        if (graph[func_key].exists) {
            curr_graph = &graph[func_key];
            INFO("selecting graph %d, '%s'\n", func_key, curr_graph->name);
        }
        break; }

    // tests that don't appear in help section of window
    //  9  : display test alert
    case '9':
        display_alert("ALERT TEST");
        break;

    // terminate pgm:
    // - q
    case 'q':
        return -1;
        break;

    // ignore all others
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

// xxx where should this be in this file?
static void graph_create(char *name, int *values, int *max_values, int start_idx, int y_offset, int y_span)
{
    graph_t *g = NULL;
    int i;

    // xxx comment why not using 0
    for (i = 1; i < MAX_GRAPH; i++) {
        if (graph[i].exists == false) {
            g = &graph[i];
            break;
        }
    }

    if (g == NULL) {
        ERROR("all graphs are already in use\n");
        return;
    }

    g->exists      = true;
    g->name        = name;
    g->values      = values;
    g->max_values  = max_values;
    g->start_idx   = start_idx;
    g->y_offset    = y_offset;
    g->y_span      = y_span;

    if (curr_graph == NULL) {
        curr_graph = g;
    }
}

// xxx testing 40 x 100
// xxx still need to work on the graphing controls
static void display_current_graph(int maxy, int maxx)
{
    graph_t *g = curr_graph;
    int      x, y, idx;
    char     c;

    #define NUM_Y_TOP              6  // includes xxx tbd  (graph title included)
    #define NUM_Y_GRAPH_TOTAL      (maxy - NUM_Y_TOP)
    #define NUM_Y_GRAPH_NEGATIVE   4
    #define NUM_Y_GRAPH_POSITIVE   (NUM_Y_GRAPH_TOTAL - NUM_Y_GRAPH_NEGATIVE)

    if (g == NULL) {
        return;
    }

    // xxx document display layout someplace

    // xxx comments needed in here

    mvprintw(NUM_Y_TOP-1, maxx/2-strlen(g->name)/2, "%s", g->name);

    move(maxy-NUM_Y_GRAPH_NEGATIVE-1, 0);
    for (idx = 0; idx < maxx; idx++) {
        addch('_');
    }

    if (true) { // xxx tracking, need some sort of control
        g->start_idx = *g->max_values - maxx;
        if (g->start_idx < 0) {
            g->start_idx = 0;
        }
    }

    for (x = 0; x < maxx; x++) {
        idx = g->start_idx + x;
        if (idx < 0) continue;
        if (idx >= *g->max_values) break;

        if (g->values[idx] == 0) continue;

        //xxx maybe change name of y_span to y_positive_span
        y = NUM_Y_GRAPH_NEGATIVE + 
            (int)( (g->values[idx] - g->y_offset) / 
                   ((double)g->y_span / NUM_Y_GRAPH_POSITIVE) );
        if (g->values[idx] < g->y_offset) {
            y--;
        }

        c = 'x';
        if (y < 0) {
            y = 0;
            c = 'v';
        } else if (y > (NUM_Y_GRAPH_TOTAL - 1)) {
            y = (NUM_Y_GRAPH_TOTAL - 1);
            c = '^';
        }

        y = (maxy-1) - y;

        mvprintw(y, x, "%c", c);
    }
}

// -----------------  CURSES WRAPPER  ----------------------------------------

static void curses_init(void)
{
    window = initscr();

    start_color();
    use_default_colors();
    init_pair(COLOR_PAIR_RED, COLOR_RED, -1);

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
    int maxy_last=0, maxx_last=0;

    while (true) {
        // erase display
        erase();

        // get window size, and print whenever it changes
        getmaxyx(window, maxy, maxx);
        if (maxy != maxy_last || maxx != maxx_last) {
            INFO("maxy=%d maxx=%d\n", maxy, maxx);
            maxy_last = maxy;
            maxx_last = maxx;
        }

        // update the display
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
static int sipm_server_request(int req_id, union response_data_u *response_data);

static void sipm_local_init(void)
{
    int optval, rc;
    struct sockaddr_in sin;

    // connect to sipm_server xxx hardcoded name here - solve with a define in sipm.h 
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

    // create the SIPM_PULSE_RATE graph
    graph_create("SIPM_PULSE_RATE",
                 sipm_pulse_rate_history,
                 &max_sipm_pulse_rate_history,
                 0,         // start_idx
                 20000,     // y_offset
                 10000);    // y_span
}

static void sipm_local_exit(void)
{
    sipm_local_terminate = true;
    pthread_join(sipm_data_collection_thread_id, NULL);
}

static void *sipm_data_collection_thread(void *cx)
{
    uint64_t          delay_us, duration_us;
    uint64_t          get_rate_start_us, get_rate_complete_us;
    int               rc, idx;
    struct get_rate_s get_rate;

    // Every 500ms request sipm data from sipm_server.
    // Store the pulse_rate data collected sipm_pulse_rate[], indexed by seconds.
    // Store the latest pulse_rate, and other info.

    // xxx enable real time

    while (true) {
        // if terminate requested then return
        if (sipm_local_terminate) {
            return NULL;
        }

        // sleep until next 500ms time boundary
        delay_us = 500000 - (microsec_timer() % 500000);
        usleep(delay_us);

        // request data from sipm_server
        get_rate_start_us = microsec_timer();
        rc = sipm_server_get_rate(&get_rate);
        if (rc != 0) {
            FATAL("sipm_server_get_rate failed\n");
        }
        get_rate_complete_us = microsec_timer();
        duration_us = get_rate_complete_us - get_rate_start_us;

        // warn if the sipm_server_get_rate took a long time
        //INFO("duration = %0.3f secs\n", duration_us / 1000000.);
        if (duration_us > 200000) {
            INFO("sipm_server_get_rate long duration %0.3f secs\n", duration_us / 1000000.);
        }

        // store the pulse_rate in sipm_pulse_rate
        idx = get_rate_start_us / 1000000;
        //INFO("STORING SIPM_PULSE_RATE idx=%d  value=%d  start_sec=%0.1f\n", 
        //     idx, get_rate.pulse_rate, get_rate_start_us/1000000.);
#if 0 //xxx TESTING
        sipm_pulse_rate_history[idx] = get_rate.pulse_rate;
#else
        sipm_pulse_rate_history[idx] = idx * 333.3333 + 18000;
#endif
        __sync_synchronize();
        max_sipm_pulse_rate_history = idx+1;

        // store the latest_sipm_data
        sipm_status.time_us                    = get_rate_start_us;
        sipm_status.duration_us                = get_rate_complete_us - get_rate_start_us;
        sipm_status.pulse_rate                 = get_rate.pulse_rate;
        sipm_status.gpio_read_rate             = get_rate.gpio_read_rate;
        sipm_status.gpio_read_and_analyze_rate = get_rate.gpio_read_and_analyze_rate;
    }

    return NULL;
}

static int sipm_server_get_rate(struct get_rate_s *get_rate)
{
    int rc;
    union response_data_u response_data;

    rc = sipm_server_request(MSGID_GET_RATE, &response_data);
    *get_rate = response_data.get_rate;
    return rc;
}

static int sipm_server_request(int req_id, union response_data_u *response_data)
{
    msg_request_t      msg_req;
    msg_response_t     msg_resp;
    int                len;

    static int         seq_num;

    memset(response_data, 0, sizeof(*response_data));

    msg_req.magic   = MSG_REQ_MAGIC;
    msg_req.id      = req_id;
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
        msg_resp.id != req_id ||
        msg_resp.seq_num != seq_num)
    {
        ERROR("invalid msg_resp %d %d %d, exp_seq_num=%d\n",
              msg_resp.magic,
              msg_resp.id,
              msg_resp.seq_num,
              seq_num);
        return -1;
    }

    *response_data = msg_resp.response_data;
    return 0;
}

// -----------------  XRAIL CONTROL & STATUS  --------------------------------

static pthread_t xrail_local_ctrl_thread_id;
static pthread_t xrail_local_status_thread_id;
static bool      xrail_local_terminate;
static bool      xrail_calibrated;
static int       xrail_ctrl_cmd;
static bool      xrail_ctrl_cmd_cancel;

static void * xrail_local_ctrl_thread(void *cx);
static void * xrail_local_status_thread(void *cx);

static void xrail_local_init(void)
{
    // create the xrail control and status threads
    pthread_create(&xrail_local_ctrl_thread_id, NULL, xrail_local_ctrl_thread, NULL);
    pthread_create(&xrail_local_status_thread_id, NULL, xrail_local_status_thread, NULL);
}

static void xrail_local_exit(void)
{
    // cause the xrail ctrl and status threads to exit
    xrail_local_terminate = true;
    pthread_join(xrail_local_ctrl_thread_id, NULL);
    pthread_join(xrail_local_status_thread_id, NULL);
}

static void xrail_local_issue_ctrl_cmd(int cmd)
{
    // if a cmd is in progress then it is an error
    if (xrail_ctrl_cmd != XRAIL_CTRL_CMD_NONE) {
        display_alert("%s is in progress", XRAIL_CTRL_CMD_STR(xrail_ctrl_cmd));
        return;
    }

    // check if the cmd is valid for current state of xrail_calibrated
    // xxx

    // issue the cmd
    xrail_ctrl_cmd_cancel = false;
    __sync_synchronize();
    xrail_ctrl_cmd = cmd;
}

static void xrail_local_cancel_ctrl_cmd(void)
{
    xrail_ctrl_cmd_cancel = true;
}

static void * xrail_local_ctrl_thread(void *cx)
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
        //INFO("STARTING %s\n", XRAIL_CTRL_CMD_STR(xrail_ctrl_cmd));
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
        //INFO("DONE %s\n", XRAIL_CTRL_CMD_STR(xrail_ctrl_cmd));

cancel:
        // if cancelled then issue message
        if (xrail_ctrl_cmd_cancel) {
            //INFO("CANCELED %s\n", XRAIL_CTRL_CMD_STR(xrail_ctrl_cmd));
            display_alert("%s is cancelled", XRAIL_CTRL_CMD_STR(xrail_ctrl_cmd));
        }

        // clearing the cmd indicates it has completed
        xrail_ctrl_cmd = 0;
    }

    return NULL;
}

static void * xrail_local_status_thread(void *cx)
{
    bool okay, calibrated;
    double curr_loc_mm, tgt_loc_mm, voltage;
    char status_str[100];

    // get the xrail status
    while (true) {
        // if terminate requested then return
        if (xrail_local_terminate) {
            return NULL;
        }

        // get xrail status
        xrail_get_status(&okay, &calibrated, &curr_loc_mm, &tgt_loc_mm, &voltage, status_str);

        // publish xrail strings 
        if (!okay) {
            sprintf(xrail_status_str, "NOT-OKAY: V=%0.1f, %s", voltage, status_str);
        } else if (!calibrated) {
            sprintf(xrail_status_str, "NOT-CALIBRATED");
        } else {
            sprintf(xrail_status_str, "%0.1f", curr_loc_mm);
        }

        // one sec sleep
        // xxx maybe more often
        sleep(1);
    }

    return NULL;
}


