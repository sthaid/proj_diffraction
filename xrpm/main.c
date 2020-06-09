#include "common.h"

#include <readline/readline.h>
#include <readline/history.h>

// variables

static int unit_test_thread_cmd;
static bool unit_test_thread_stop_req;

// prototypes

static void sigint_handler(int signum);
static void * unit_test_thread(void *cx);

// -----------------  UNIT TEST  -----------------------------------

int main(int argc, char **argv)
{
    #define START_UNIT_TEST_THREAD(testid) \
        do { \
            pthread_t tid; \
            if (unit_test_thread_cmd != 0) { \
                printf("unit_test_thread already running\n"); \
                continue; \
            } \
            printf("unit_test_thread is being created\n"); \
            unit_test_thread_stop_req = false; \
            unit_test_thread_cmd = (testid); \
            pthread_create(&tid, NULL, unit_test_thread, NULL); \
        } while (0)

    char *s=NULL, s1[1000], *x, *tok[100];
    int max_tok;
    struct sigaction act;

    // register for SIGINT
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);

    // init other files
    utils_init();
    audio_init();
    // XXX init sipm, and exit
    xrail_init();
    INFO("INITIALIZATION COMPLETE\n");

    // loop, read and process user's commands
    while (true) {
        // get command string 's'
        free(s);
        if ((s = readline("> ")) == NULL) {
            break;
        }

        // copy s to s1, and parse s1 into tokens 
        strcpy(s1, s);
        memset(tok, 0, sizeof(tok));
        max_tok = 0;
        while ((x = strtok(max_tok == 0 ? s1 : NULL, " \n"))) {
             tok[max_tok++] = x;
        }

        // if blank line then continue
        if (tok[0] == NULL) {
            continue;
        }

        // save the command string 's' in history
        add_history(s);

        // process the cmd
        if (strcmp(tok[0], "au") == 0) {
            // process audio commands
            // - au incr
            // - au decr
            // - au test ...
            if (max_tok == 2 && strcmp(tok[1], "incr") == 0) {
                int vol = audio_change_volume(5);
                audio_say_text("volume is now %d pecent", vol);
            } else if (max_tok == 2 && strcmp(tok[1], "decr") == 0) {
                int vol = audio_change_volume(-5);
                audio_say_text("volume is now %d pecent", vol);
            } else if (max_tok == 2 && strcmp(tok[1], "test") == 0) {
                audio_say_text("open the pod bay doors Hal");
            } else {
                printf("invalid %s subcommand '%s'\n", tok[0], tok[1]);
            }
        } else if (strcmp(tok[0], "xr") == 0) {
            // process xrail commands
            // - xr cal +       # cal move 1 mm right
            // - xr cal -       # cal move 1 mm left
            // - xr cal done    # cal complete
            // - xr goto <mm>   # goto location
            // - xr test        # goto -25, +25, home
            // - xr             # return status
            double mm, curr_loc_mm, tgt_loc_mm, voltage;
            bool okay, cal;
            char status_str[1000];
            if (max_tok == 3 && strcmp(tok[1], "cal") == 0 && strcmp(tok[2], "+") == 0) {
                xrail_cal_move(1);
            } else if (max_tok == 3 && strcmp(tok[1], "cal") == 0 && strcmp(tok[2], "-") == 0) {
                xrail_cal_move(-1);
            } else if (max_tok == 3 && strcmp(tok[1], "cal") == 0 && strcmp(tok[2], "done") == 0) {
                xrail_cal_complete();
            } else if (max_tok == 3 && strcmp(tok[1], "goto") == 0 && sscanf(tok[2],"%lf", &mm) == 1) {
                xrail_goto_location(mm, true);
            } else if (max_tok == 2 && strcmp(tok[1], "test1") == 0) {
                START_UNIT_TEST_THREAD(1);
            } else if (max_tok == 2 && strcmp(tok[1], "test2") == 0) {
                START_UNIT_TEST_THREAD(2);
            } else if (max_tok == 1) {
                xrail_get_status(&okay, &cal, &curr_loc_mm, &tgt_loc_mm, &voltage, status_str);
                printf("xr: okay=%d cal=%d curr_loc_mm=%.2f tgt_loc_mm=%.2f voltage=%.1f status='%s'\n",
                       okay, cal, curr_loc_mm, tgt_loc_mm, voltage, status_str);
            } else {
                printf("invalid %s subcommand '%s'\n", tok[0], tok[1]);
            }
        } else if (strcmp(tok[0], "stop") == 0) {
            // stop the unit test thread
            if (unit_test_thread_cmd) {
                printf("unit_test_thread stop requested\n");
                unit_test_thread_stop_req = true;
            } else {
                printf("unit_test_thread is not currently running\n");
            }
        } else if (strcmp(tok[0], "q") == 0) {
            // quit
            break;
        } else {
            printf("invalid '%s'\n", tok[0]);
        }
    }

    // call other file exit routines
    INFO("TERMINATING\n");
    xrail_exit();
    audio_exit();
    utils_exit();

    // done
    return 0;
}

static void sigint_handler(int signum)
{
}

static void * unit_test_thread(void *cx)
{
    #define CHECK_STOP_REQ \
        do { \
            if (unit_test_thread_stop_req) { \
                printf("unit_test_thread stopping\n"); \
                return NULL; \
            } \
        } while (0)

    if (unit_test_thread_cmd == 1) {
        xrail_goto_location(-25, true); CHECK_STOP_REQ;
        xrail_goto_location(+25, true); CHECK_STOP_REQ;
        xrail_goto_location(0,   true); CHECK_STOP_REQ;
    } else if (unit_test_thread_cmd == 2) {
        double mm;
        for (mm = -25; mm <= 25; mm += .1) {
            xrail_goto_location(mm, true); CHECK_STOP_REQ;
            usleep(1000000);
        }
        xrail_goto_location(0, true);
    } else {
        FATAL("invalid unit_test_thread_cmd %d\n", unit_test_thread_cmd);
    }

    printf("unit_test_thread completed\n");
    unit_test_thread_cmd = 0;
    return NULL;
}
