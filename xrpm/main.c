#include "common.h"

#include <readline/readline.h>
#include <readline/history.h>

#include <sys/resource.h>

// variables

static int unit_test_thread_cmd;
static bool unit_test_thread_stop_req;

// prototypes

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

    // init files
    utils_init();
    audio_init();
    sipm_init();
    xrail_init();

    // init is complete
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
            // - xr             # get status
            // - xr cal +       # cal move 1 mm right
            // - xr cal -       # cal move 1 mm left
            // - xr cal done    # cal complete
            // - xr goto <mm>   # goto location
            // - xr test1       # goto -25, +25, home
            // - xr test2       # goto -25, +25, home in 0.1mm steps
            double mm, curr_loc_mm, tgt_loc_mm, voltage;
            bool okay, cal;
            char status_str[1000];
            if (max_tok == 1) {
                xrail_get_status(&okay, &cal, &curr_loc_mm, &tgt_loc_mm, &voltage, status_str);
                printf("xr: okay=%d cal=%d curr_loc_mm=%.2f tgt_loc_mm=%.2f voltage=%.1f status='%s'\n",
                       okay, cal, curr_loc_mm, tgt_loc_mm, voltage, status_str);
            } else if (max_tok == 3 && strcmp(tok[1], "cal") == 0 && strcmp(tok[2], "+") == 0) {
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
            } else {
                printf("invalid %s subcommand '%s'\n", tok[0], tok[1]);
            }
        } else if (strcmp(tok[0], "pm") == 0) {
            // process sipm commands
            // - pm     # get sipm pulse_rate and stats
            if (max_tok == 1) {
                START_UNIT_TEST_THREAD(3);
            } else {
                printf("invalid %s subcommand '%s'\n", tok[0], tok[1]);
            }
        } else if (strcmp(tok[0], "xrpm") == 0) {
            // process xrail/sipm commands
            // - xrpm   # XXX tbd descr
            if (max_tok == 1) {
                START_UNIT_TEST_THREAD(4);
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

    // clean up and exit
    INFO("TERMINATING\n");

    if (unit_test_thread_cmd != 0) {
        unit_test_thread_stop_req = true;
        INFO("waiting for unit_test_thread to finish\n");
        while (unit_test_thread_cmd != 0) {
            my_usleep(1000000);
        }
    }

    xrail_exit();
    sipm_exit();
    audio_exit();
    utils_exit();

    // done
    return 0;
}

// XXX ensure calibrated when needed
static void * unit_test_thread(void *cx)
{
    #define CHECK_STOP_REQ \
        do { \
            if (unit_test_thread_stop_req) { \
                printf("unit_test_thread stopping\n"); \
                unit_test_thread_cmd = 0; \
                return NULL; \
            } \
        } while (0)

    if (unit_test_thread_cmd == 1) {
        xrail_goto_location(-25, true); 
        CHECK_STOP_REQ;
        xrail_goto_location(+25, true); 
        CHECK_STOP_REQ;
        xrail_goto_location(0,   true); 
        CHECK_STOP_REQ;
    } else if (unit_test_thread_cmd == 2) {
        double mm;
        for (mm = -25; mm <= 25; mm += .1) {
            xrail_goto_location(mm, true); 
            my_usleep(1000000);
            CHECK_STOP_REQ;
        }
        xrail_goto_location(0, true);
    } else if (unit_test_thread_cmd == 3) {
        int pulse_rate, gpio_read_rate, gpio_read_and_analyze_rate;
        while (true) {
            sipm_get_rate(&pulse_rate, &gpio_read_rate, &gpio_read_and_analyze_rate);
            printf("pm: pulse_rate=%.1f K  gpio_read_rate=%.1f M  gpio_read_and_analyze_rate=%.1f M\n",
                   (double)pulse_rate/1000,
                   (double)gpio_read_rate/1000000,
                   (double)gpio_read_and_analyze_rate/1000000);
            audio_say_text("%d", (pulse_rate+500)/1000);
            my_usleep(1000000);
            CHECK_STOP_REQ;
        }
    } else if (unit_test_thread_cmd == 4) {
        double mm;
        int pulse_rate, gpio_read_rate, gpio_read_and_analyze_rate;
        for (mm = -25; mm <= 25; mm += .1) {
            // move xrail to location mm
            xrail_goto_location(mm, true); 

            // sleep 1 second to allow sipm code to collect data at this location
            my_usleep(1000000);

            // query sipm to get the pulse rate
            sipm_get_rate(&pulse_rate, &gpio_read_rate, &gpio_read_and_analyze_rate);
            printf("xrpm: mm=%.1f  pulse_rate=%.1f K  gpio_read_rate=%.1f M  gpio_read_and_analyze_rate=%.1f M\n",
                   mm,
                   (double)pulse_rate/1000,
                   (double)gpio_read_rate/1000000,
                   (double)gpio_read_and_analyze_rate/1000000);
            audio_say_text("%d", (pulse_rate+500)/1000);

            // check for request to stop this run
            CHECK_STOP_REQ;
        }

        // reset xrail to home location
        xrail_goto_location(0, true);
    } else {
        FATAL("invalid unit_test_thread_cmd %d\n", unit_test_thread_cmd);
    }

    printf("unit_test_thread completed\n");
    unit_test_thread_cmd = 0;
    return NULL;
}
