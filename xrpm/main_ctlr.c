#include "common_includes.h"

#include <curses.h>

#include "sipm.h"
#include "utils.h"
#include "audio.h"

//
// defines
//

//
// variables
//

static WINDOW * window;

//
// prototypes
//

void update_display(int maxy, int maxx);
int input_handler(int input_char);

void curses_init(void);
void curses_exit(void);
void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char));

void *test_thread(void *cx);

// -----------------  MAIN  --------------------------------------------------

int main(int argc, char **argv)
{
    pthread_t tid;

    // init files
    utils_init("ctlr.log");
    audio_init();
    INFO("INITIALIZATION COMPLETE\n");

    // create threads
    pthread_create(&tid, NULL, test_thread, NULL);

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

static int update_count, char_count;

void update_display(int maxy, int maxx)
{
    int row = maxy / 2;
    int col = maxx / 2;

    update_count++;

    mvprintw(row, col, "maxy=%d maxx=%d chars=%d updates=%d", 
            maxy, maxx, char_count, update_count);
}

int input_handler(int input_char)
{
    if (input_char == 'q') {
        return -1;
    }

    char_count++;
    return 0;
}

// -----------------  CURSES WRAPPER  ----------------------------------------

void curses_init(void)
{
    // init curses
    window = initscr();
    cbreak();
    noecho();
    nodelay(window,TRUE);
    keypad(window,TRUE);
}

void curses_exit(void)
{
    endwin();
}

void curses_runtime(void (*update_display)(int maxy, int maxx), int (*input_handler)(int input_char))
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
            // do nothing
        } else if (input_char != ERR) {
            if (input_handler(input_char) != 0) {
                return;
            }
        } else {
            usleep(100000);
        }
    }
}

// -----------------  TBD  ---------------------------------------------------

void *test_thread(void *cx)
{
    msg_request_t      msg_req;
    msg_response_t     msg_resp;
    uint64_t           start_us, duration_us;
    int                len, optval, rc, seq_num=0, sfd, count;
    struct sockaddr_in sin;

    // connect to sipm_server
    // XXX hardcoded name here
    rc = getsockaddr("ds", SIPM_SERVER_PORT, &sin);
    if (rc != 0) {
        FATAL("getsockaddr failed\n");
    }
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd == -1) {
        FATAL("failed to create socket, %s\n",strerror(errno));
    }
    rc = connect(sfd, (struct sockaddr *)&sin, sizeof(sin));
    if (rc < 0) {
        FATAL("connect to sipm_server failed, %s\n", strerror(errno));
    }

    // enable TCP_NODELAY socket option
    optval = 1;
    rc = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    if (rc == -1) {
        FATAL("setsockopt TCP_NODELAY, %s", strerror(errno));
    }

    // get pulse_rate from sipm_server
    // XXX comments
    count = 0;
    while (true) {
        start_us = microsec_timer();

        msg_req.magic   = MSG_REQ_MAGIC;
        msg_req.id      = MSGID_GET_RATE;
        msg_req.seq_num = ++seq_num;
        len = do_send(sfd, &msg_req, sizeof(msg_req));
        if (len != sizeof(msg_req)) {
            ERROR("do_send ret=%d, %s\n", len, strerror(errno));
            return NULL;
        }

        len = do_recv(sfd, &msg_resp, sizeof(msg_resp));
        if (len != sizeof(msg_resp)) {
            ERROR("do_recv ret=%d, %s\n", len, strerror(errno));
            return NULL;
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
            return NULL;
        }

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
