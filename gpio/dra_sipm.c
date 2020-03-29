// This program was written to test the integration of the following ...
// - Silicon Photomultiplier in a box which I hope keeps the light out
//      Microfc-SMA-10035-GEVB
//      C Series 1mm 35u SMA
//      ON Semiconductor
// - Mini-Ciruits Coaxial Amplifier ZX60-43-S+
//      2 of these, connected in series
// - Pulse Stretcher Ciruit
//      Built using LTC6752-4 Comparator
//      Refer to Typical Application for
//      Pulse Stretcher Circuit / Monostable Multivrator
// - Raspberry Pi 4 4GB 
//      The Pulse Strectcher output connected to GPIO_PIN 26
// - This test program, which reads the GPIO 26 value in a tight loop,
//   and post processes the data to determine the pulse rate.
//   Refer to comments in the PARSE_PULSE macro for how the gpio values
//   are processed to determine the pulse rate.

// Power supplies
// - The SiPM should be supplied an Overvoltage in Range 1.0 - 5.0 over Vbr.
//   Which works out to 25.5 to 29.5 volts; where higher voltages increases the
//   device's effeciency of detecting photons. I'm using 3 9v batteries in series,
//   with a measured voltage of 28.2V, which is on the high end of the range.
// - The 2 Mini-Circuits amplifiesrs are being powered by 3 D cells. The measured
//   voltage was originally about 4.8V, and now is down to 4.4V. So, I have a 
//   concern about using these D cells to provide power to these amplifiers.
// I use batteries because I worry about ground loops and other possible adverse
// characteristics that may be associated with using an AC adapter.

// Also refer to jpg files in this directory, for pictures of the setup.

// This program needs to be run as root to be able to access the GPIO registers.
// If this program is just being used on a saved gpio file, then running as 
// root is not required.

// This program, running the Raspberry PI 4, is able to read the GPIO 26 at a
// rate of about 70 million reads per second. The pulse stretcher is supposed
// to create 100 ns pulse. So, there should be about 7 GPIO '1' values per pulse.

// Pulse stretcher construction:
// - The LTC6752ISC6-4#TRMPBFCT-ND Comparators were ordered from Proto Advantage,
//   They purchased the components from Digikey and mounted on a DIP-6 SMT Adapter,
//   which can be used on a breadboard.
// - I tested the breadboard circuit using a signal generator and dual channel
//   oscilloscope. At first I was just using a BNC cable to connect to the oscilloscope,
//   and this may have been the cause of some difficulties getting this to work. I
//   think I had better success after I started using oscilloscope probes.

// My initial test of the SiPM was to put it in a sealed box, and connect it's output
// directly to the oscilloscope input. I thought I saw pulses, but they were barely
// visible with the scope on 5mv/division setting.

// I purchased an inexpensive amplifier, 
//    HiLetgo 0.1-2000MHz RF WideBand Amplifier 30dB High Gain Low Noise LNA Amplifier
// and connected this to the SiPM and Oscilloscope. I saw what I thought was a lot of 
// noise, and the output signal did not look anything like what is shown on page 8 of the
// ON Semiconductor 'Biasing and Readout of ON Semiconductiro SiPM Sensors' AND9782/D doc.
// So, based on this ON Semiconductor document I purchased the 2 Mini-Circuits amplifiers.
// These also looked noisy. I never did a direct comparison between these and the 
// HiLetgo, so I can't say for sure which is better. Also, there may have been other reasons
// for what I thought was noise; such as the box not being well sealed, and perhaps the 
// oscilloscope is not working well.

// I tried to reduce the noise by connecting the output of the Mini-Circuits amplifier 
// to a low pass filter and viewed the output of the low pass filter load resistor on 
// the oscilloscope. And there was no reduction of the noise. This is when I started to
// consider that something else may be going on.

// I discarded the low pass filter, and connected th Mini-Circuit amplifiers to the
// pulse stretcher, and did observe stretched pulses on the oscilloscope, but it
// did not look clean. 

// Next I disconnected the oscilloscope from the pulse stretcher output, and connected
// the pulse stretcher output to the Raspberry PI GPIO 26. And started running the 
// Raspberry Pi software in the dra_pulse.c program to count pulses. 

// While running the dra_pulse program on the Raspberry Pi I adjusted the voltage divider 
// on the input Pulse Stretcher threshold detector (U1), values are shown in the table below.  
// I was attempting to get count rate readings that were in the Dark Count range that is
// documented by ON Semiconductor.

// I'm using 10K instead of 15K on the 3.3V side of the voltage divider. And I varied
// the resistor on the side attached to ground, which is a 49.9 ohm resistor in the
// pulse stretcher circuit diagram. Here are the results. Note that the 'Signal' is 
// injected by uncovering a small hole on the top of the box to let room light in.
//                                   Resistance, ohms
//                          1000    680     510     470     330
//                          ----   ----    ----    ----    ----
// Dark Rate K Cnt/sec         0      2      16      20     113
// Signal Rate K Cnt/sec      24    270     877    1072    2250
//
// I've currently settled on the 470 ohm choice.

// After making the adjustments described above, I noticed fluctuations due to room 
// lighting, turning off the lights and closing the curtains made a big difference. Even
// my position in the room would block outside light from the box. So, the box is not that
// well sealed, and this could by the cause of some of what I've been calling noise. I have
// since sealed the box better. Using multiple layers of black electrical tape on the edges.

// XXX possible future work
// - better pwoer for amplifiers, use a 5V rechargeable battey
// - if isolcpus is available on Raspbian, try it

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <dra_gpio.h>
#include <util_misc.h>

// defines
#define GPIO_PIN  26

#define GPIO_STRUCT_MAGIC  0x227755aa

#define MAX_DURATION_SECS    10
#define MAX_GPIO_READ_RATE   100000000    // 100 million samles/sec
#define MAX_GPIO_DATA        (MAX_GPIO_READ_RATE / 32 * MAX_DURATION_SECS)

// typedefs
typedef struct {
    int magic;
    int max_data;
    uint64_t duration_us;
    unsigned int data[MAX_GPIO_DATA];
} gpio_t;

// variables
volatile bool sigalrm_rcvd;
volatile bool sigint_rcvd;
gpio_t        gpio;
bool          gpio_hw_access_initialized;

// prototypes
void sigalrm_handler(int signum);
void sigint_handler(int signum);
char * stars(double value, int stars_max, double value_max);

void count_consecutive_reset(void);
bool count_consecutive_is_at_eod(void);
int count_consecutive(int v);

void read_gpio(char *duration_secs_str, bool verbose, double *gpio_read_rate_ret);
int read_file(char *filename);
void write_file(char *filename);
void pulse_count(char *output_fmt_str);
void pulse_display(void);
void run(char *duration_secs_str, char *output_fmt_str);
void help(void);

// -----------------  MAIN  --------------------------------------------

int main(int argc, char **argv)
{
    struct sigaction act;
    char cmdline[1000], cmd[100], arg1[100], arg2[100];
    int rc;
    double gpio_read_rate;

    // if arg is provided it should be a filename
    if (argc > 1) {
        rc = read_file(argv[1]);
        if (rc != 0) {
            printf("ERROR: exitting because read_file failed\n");
            return 1;
        }
    }

    // init dra_gpio capability
    rc = gpio_init();
    gpio_hw_access_initialized = (rc == 0);

    // register for SIGALRM, and SIGINT
    memset(&act, 0, sizeof(act));
    act.sa_handler = sigalrm_handler;
    sigaction(SIGALRM, &act, NULL);

    memset(&act, 0, sizeof(act));
    act.sa_handler = sigint_handler;
    sigaction(SIGINT, &act, NULL);

    // read cmdline
    while (printf("> "), fgets(cmdline, sizeof(cmdline), stdin) != NULL) {
        // scan cmdline 
        cmd[0] = arg1[0] = arg2[0] = '\0';
        sscanf(cmdline, "%s %s %s", cmd, arg1, arg2);

        // if blank cmd then continue
        if (cmd[0] == '\0') {
            continue;
        }

        // process the cmd
        if (strcmp(cmd, "read_gpio") == 0) {
            if (!gpio_hw_access_initialized) {
                printf("ERROR: gpio hardware access is not initialized\n");
                continue;
            }
            read_gpio(arg1, true, &gpio_read_rate);  // duration_secs_str, verbose, gpio_read_rate
        } else if (strcmp(cmd, "read_file") == 0) {
            read_file(arg1);  // filename_str
        } else if (strcmp(cmd, "write_file") == 0) {
            write_file(arg1);  // filename_str
        } else if (strcmp(cmd, "pulse_count") == 0) {
            pulse_count(arg1);  // output_fmt_str
        } else if (strcmp(cmd, "pulse_display") == 0) {
            pulse_display();
        } else if (strcmp(cmd, "run") == 0) {
            run(arg1,arg2);  // duration_secs_str, output_fmt_str
        } else if (strcmp(cmd, "help") == 0) {
            help();
        } else if (strcmp(cmd, "q") == 0) {
            break;
        } else {
            printf("ERROR: invalid cmd '%s'\n", cmd);
        }
    }

    return 0;
}

// -----------------  MISC SUPPORT  ------------------------------------

void sigalrm_handler(int signum)
{
    sigalrm_rcvd = true;
}

void sigint_handler(int signum)
{
    sigint_rcvd = true;
}

char * stars(double value, int stars_max, double value_max)
{
    static char stars[1000];
    int len;
    bool overflow = false;

    len = nearbyint((value / value_max) * stars_max);
    if (len > stars_max) {
        len = stars_max;
        overflow = true;
    }

    memset(stars, '*', len);
    stars[len] = '\0';
    if (overflow) {
        strcpy(stars+len, "...");
    }

    return stars;
}

// -----------------  COUNT CONSECUTICE BITS OF GPIO.DATA  -------------

#define GET_BIT_AT_CCIDX \
    ((gpio.data[ccidx>>5] & (1 << (31 - (ccidx & 0x1f)))) != 0)

int ccidx;
int max_ccidx;

void count_consecutive_reset(void)
{
    ccidx     = 0;
    max_ccidx = gpio.max_data * 32;
}

bool count_consecutive_is_at_eod(void)
{
    return ccidx >= max_ccidx;
}

int count_consecutive(int v)
{
    int cnt=0;

    while (true) {
        if (ccidx >= max_ccidx) {
            return cnt;
        }

        if (GET_BIT_AT_CCIDX == v) {
            cnt++;
            ccidx++;
        } else {
            return cnt;
        }
    }
}

// -----------------  CMD HANDLER ROUTINES  ----------------------------

void read_gpio(char *duration_secs_str, bool verbose, double *gpio_read_rate_ret)
{
    struct itimerval new_value, old_value;
    uint64_t start_us, end_us;
    double duration_secs;
    int max_data;
    double gpio_read_rate;

    // convert caller's duration_secs_str to duration_secs
    if (duration_secs_str[0] == '\0') {
        duration_secs = 1;
    } else {
        if (sscanf(duration_secs_str, "%lg", &duration_secs) != 1) {
            printf("ERROR: '%s' is not a number\n", duration_secs_str);
            return;
        }
    }

    // check if duration_secs is out of range
    if (duration_secs <= 0 || duration_secs > MAX_DURATION_SECS) {
        printf("ERROR: '%s' out of range\n", duration_secs_str);
        return;
    }

    // set interval timer, which will set the sigalrm_rcvd flag
    memset(&new_value, 0, sizeof(new_value));
    new_value.it_value.tv_sec  = duration_secs;
    new_value.it_value.tv_usec = (duration_secs - new_value.it_value.tv_sec) * 1000000;
    setitimer(ITIMER_REAL, &new_value, &old_value);
    //printf("DEBUG: SECS = %ld   US = %ld\n", 
    //       new_value.it_value.tv_sec, new_value.it_value.tv_usec);

    // loop until sigalrm_rcvd
    max_data = 0;
    start_us = microsec_timer();
    while (!sigalrm_rcvd) {
        int i;
        unsigned int v32;

        for (v32 = 0, i = 0; i < 32; i++) {
            v32 <<= 1;
            v32 |= gpio_read(GPIO_PIN);
        }

        gpio.data[max_data++] = v32;
    }
    end_us = microsec_timer();

    // clear sigalrm_rcvd
    sigalrm_rcvd = false;

    // set other fields in the gpio struct
    gpio.magic       = GPIO_STRUCT_MAGIC ;
    gpio.max_data    = max_data;
    gpio.duration_us = end_us - start_us;

    // print gpio read rate
    gpio_read_rate = (max_data * 32.) / ((end_us - start_us) / 1000000.);
    if (verbose) {
        printf("gpio read rate %g million gpio_read/sec\n", gpio_read_rate/1000000);
    }

    // return the gpio_read_rate 
    *gpio_read_rate_ret = gpio_read_rate;
}

int read_file(char * filename)
{
    int header_size = offsetof(gpio_t, data);
    int fd, expected_file_size, len;
    struct stat statbuf;

    // open filename for reading
    fd = open(filename, O_RDONLY);
    if (fd < 0) {
        printf("ERROR: file '%s' failed open, %s\n", filename, strerror(errno));
        return -1;
    }

    // stat file to get its length.
    // if length shorter than the hdr it is an error
    memset(&statbuf, 0, sizeof(statbuf));
    fstat(fd, &statbuf);
    if (statbuf.st_size < header_size) {
        printf("ERROR: file '%s' size=%ld is less than header_size=%d\n",
               filename, statbuf.st_size, header_size);
        close(fd);
        return -1;
    }

    // read the header, and verify
    len = read(fd, &gpio, header_size);
    if (len != offsetof(gpio_t, data)) {
        printf("ERROR: file '%s' header read failed %s\n", 
               filename, strerror(errno));
        close(fd);
        return -1;
    }
    if (gpio.magic != GPIO_STRUCT_MAGIC  || gpio.max_data <= 0 || gpio.max_data >= MAX_GPIO_DATA) {
        printf("ERROR: file '%s' has bad magic=0x%x or max_data=%d\n", 
               filename, gpio.magic, gpio.max_data);
        close(fd);
        return -1;
    }

    // determine expected_file_size form header max_data field
    expected_file_size = offsetof(gpio_t, data[gpio.max_data]);
    if (statbuf.st_size != expected_file_size) {
        printf("ERROR: file '%s' expected_size=%d actual_size=%ld\n",
               filename, expected_file_size, statbuf.st_size);
        close(fd);
        return -1;
    }

    // read the gpio.data, and verify length read
    len = read(fd, gpio.data, statbuf.st_size-header_size);
    if (len != statbuf.st_size-header_size) {
        printf("ERROR: file '%s' len read %d, expected %ld, %s\n",
               filename, len, statbuf.st_size-header_size, strerror(errno));
        close(fd);
        return -1;
    }

    // close fd, and return success
    close(fd);
    return 0;
}

void write_file(char * filename)
{
    int fd, len, file_size;

    // verifiy gpio struct has been initialized
    if (gpio.magic != GPIO_STRUCT_MAGIC  || gpio.max_data <= 0 || gpio.max_data >= MAX_GPIO_DATA) {
        printf("ERROR: gpio is not initialized\n");
        return;
    }

    // determine file length
    file_size = offsetof(gpio_t, data[gpio.max_data]);

    // open, create, trunc filename
    fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);

    // write file
    len = write(fd, &gpio, file_size);
    if (len != file_size) {
        printf("ERROR: write to file '%s' failed, %s\n", filename, strerror(errno));
        close(fd);
        return;
    }

    // close
    close(fd);
}

// First look for consecutive 1s followed by consecutive 0s.
// If there are few consecutive 0s following the consecutive 1s then 
//  these 0s are not considered the end of the pulse, instead they are considered
//  to be part of the pulse. We allow for up to 4 short groups of consecutive
//  0s to be considered part of the pulse.
#define PARSE_PULSE \
    do { \
        int _i; \
        cnt1 = count_consecutive(1); \
        cnt0 = count_consecutive(0); \
        for (_i = 0; _i < 4; _i++) { \
            if (cnt0 >= 8) { \
                break; \
            } \
            cnt1 += cnt0; \
            cnt1 += count_consecutive(1); \
            cnt0 = count_consecutive(0); \
        } \
    } while (0)

void pulse_count(char *output_fmt_str)
{
    int count=0, sum_cnt0=0, sum_cnt1=0;
    double rate_kcntpersec;
    double duration_secs;
    double avg_pulse_len;
    int cnt1, cnt0, output_fmt;

    if (gpio.magic != GPIO_STRUCT_MAGIC) {
        printf("ERROR: no gpio data\n");
        return;
    }

    if (output_fmt_str[0] == '\0') {
        // graphical output, max rate value = 100 k cnt/sec
        output_fmt = 100;
    } else {
        if (sscanf(output_fmt_str, "%d", &output_fmt) != 1 ||
            output_fmt < 0 || output_fmt > 10000)
        {
            printf("ERROR: expected output_fmt=0 for text format or\n");
            printf("       output_fmt=1..10000 for graph format\n");
            return;
        }
    }

    count_consecutive_reset();
    sum_cnt0 += count_consecutive(0);

    while (true) {
        PARSE_PULSE;

        sum_cnt0 += cnt0;
        sum_cnt1 += cnt1;

        if (count_consecutive_is_at_eod()) {
            break;
        }

        count++;
    }

    if (sum_cnt0 + sum_cnt1 != gpio.max_data * 32) {
        printf("ERROR: BUG sum_cnt0=%d sum_cnt1=%d gpio.max_data=%d\n",
               sum_cnt0, sum_cnt1, gpio.max_data*32);
    }

    duration_secs = gpio.duration_us / 1000000.;
    rate_kcntpersec = (count / duration_secs) / 1000.;
    avg_pulse_len = (count ? (double)sum_cnt1 / count : 0);
    
    switch (output_fmt) {
    case 0:
        printf("count=%d  duration_secs=%g  avg_pulse_len=%g  rate=%g k/sec\n", 
               count, duration_secs, avg_pulse_len, rate_kcntpersec);
        break;
    case 1 ... 10000:
        printf("max=%d-kcnt/sec - %8.3f: %s\n", 
               output_fmt, rate_kcntpersec, stars(rate_kcntpersec, 130, output_fmt));
        break;
    }
}

void pulse_display(void)
{
    int cnt1, cnt0, lines=0;
    char s[100];

    if (gpio.magic != GPIO_STRUCT_MAGIC) {
        printf("ERROR: no gpio data\n");
        return;
    }

    count_consecutive_reset();
    count_consecutive(0);

    while (true) {
        PARSE_PULSE;
        if (count_consecutive_is_at_eod()) {
            break; 
        }

        printf("%3d %6d: ", cnt1, cnt0);
        printf("%-25s ", stars(cnt1, 20, 20));
        printf("%-65s ", stars(cnt0, 60, 60));
        printf("\n");

        if (++lines == 45) {
            lines = 0;
            printf("CR FOR MORE ");
            fgets(s,sizeof(s),stdin);
            if (s[0] != '\n') {
                printf("\n");
                break;
            }
        }
    }
}

void run(char *duration_secs_str, char *output_fmt_str)
{
    double gpio_read_rate;

    sigint_rcvd = false;
    while (!sigint_rcvd) {
        read_gpio(duration_secs_str, false, &gpio_read_rate);
        printf("gpio_read_rate=%2.0f ", gpio_read_rate/1000000);
        pulse_count(output_fmt_str);
    }
    sigint_rcvd = false;
}

void help(void)
{
    printf("\
USAGE: commands:\n\
- read_gpio [<secs>]:    continuously read gpio data and save the results \n\
                         in gpio data buffer, to be processed later\n\
- read_file <filename>:  read rpio data that was previously saved to filename\n\
- write_file <filename>: save gpio data to filename\n\
- pulse_count <fmt>:     analyze gpio data to determine the pulse rate\n\
                         fmt=0:       text output format\n\
                         fmt=1-10000: graphical, and the fmt value us used as\n\
                                      max_graph in K-cnt/sec\n\
- pulse_display:         display graphical representation of the gpio data pulses\n\
- run [<secs> <fmt>]:    loop performing read_gpio and puls_count\n\
- help:                  display summary of commands\n\
- q:                     quit\n\
");
}

