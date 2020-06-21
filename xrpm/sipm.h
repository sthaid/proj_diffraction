#ifndef __SIPM_H__
#define __SIPM_H__

// defines
#define SIPM_SERVER_NAME  "ds"
#define SIPM_SERVER_PORT  9100

#define MSG_REQ_MAGIC  123
#define MSG_RESP_MAGIC 456

#define MSGID_GET_RATE 1

// typedefs
typedef struct {
    int magic;
    int id;
    int seq_num;
} msg_request_t;

typedef struct {
    int magic;
    int id;
    int seq_num;
    union response_data_u {
        struct get_rate_s {
            int pulse_rate;
            int gpio_read_rate;
            int gpio_read_and_analyze_rate;
        } get_rate;
    } response_data;
} msg_response_t;

// prototypes
void sipm_init(void);
void sipm_exit(void);
void sipm_get_rate(int *pulse_rate, int *gpio_read_rate, int *gpio_read_and_analyze_rate);

#endif
