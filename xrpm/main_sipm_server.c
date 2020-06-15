#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "sipm.h"
#include "utils.h"

//
// defines
//

//
// variables
//

//
// prototypes
//

static void accept_and_process_client_connection(void);
static void process_client_messages(int sfd);

// -----------------  MAIN - SIPM SERVER  ---------------------------------------

int main(int argc, char **argv)
{
    // init files
    utils_init("sipm_server.log");
    sipm_init();

    // init is complete
    INFO("INITIALIZATION COMPLETE\n");

    // accept and process the client connection
    accept_and_process_client_connection();

    // clean up and exit
    INFO("TERMINATING\n");
    sipm_exit();
    utils_exit();
    return 0;
}

// -----------------  ACCEPT AND PROCESS CLIENT CONNECTION ----------------------

void accept_and_process_client_connection()
{
    int listen_sfd, sfd, rc;
    struct sockaddr_in addr;
    socklen_t          addrlen;
    char               str[100];
    int                optval;

    // create listen socket
    listen_sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sfd == -1) {
        FATAL("socket listen_sfd\n");
    }

    // enable socket option to reuse the address
    optval = 1;
    if (setsockopt(listen_sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        FATAL("setsockopt SO_REUSEADDR, %s", strerror(errno));
    }

    // bind listen socket to any IP addr, and SIPM_SERVER_PORT
    bzero(&addr,sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);
    addr.sin_port = htons(SIPM_SERVER_PORT);
    if (bind(listen_sfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        FATAL("bind listen_sfd\n");
    }

    // listen for a connect request
    if (listen(listen_sfd, 0) == -1) {
        FATAL("listen\n");
    }

reconnect:
    // wait for a connect request, and accept the connection
    addrlen = sizeof(addr);
    sfd = accept(listen_sfd, (struct sockaddr *)&addr, &addrlen);
    if (sfd == -1) {
        FATAL("accept, %s\n", strerror(errno));
    }
    INFO("accepted connection from %s\n",
         sock_addr_to_str(str, sizeof(str), (struct sockaddr*)&addr));

    // enable TCP_NODELAY socket option
    optval = 1;
    rc = setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    if (rc == -1) {
        FATAL("setsockopt TCP_NODELAY, %s", strerror(errno));
    }

    // process messages
    process_client_messages(sfd);

    // lost connection; reconnect
    close(sfd);
    sfd = -1;
    ERROR("lost connection\n");
    goto reconnect;
}

// this routine returns when the client disconnects or 
// when an error has occurred
static void process_client_messages(int sfd)
{
    msg_request_t  msg_req;
    msg_response_t msg_resp;
    int            len;
    int            expected_seq_num = 0;

    while (true) {
        // read msg_req from client
        len = do_recv(sfd, &msg_req, sizeof(msg_req));
        if (len != sizeof(msg_req)) {
            ERROR("do_recv ret=%d, %s\n", len, strerror(errno));
            return;
        }

        // validate msg_req magic and seq_num fields
        if (msg_req.magic != MSG_REQ_MAGIC) {
            ERROR("invalid msg_req.magic=0x%x\n", msg_req.magic);
            return;
        }
        if (msg_req.seq_num != ++expected_seq_num) {
            ERROR("invalid msg_req.seq_num=%d expected_deq_num=%d\n",
                  msg_req.seq_num, expected_seq_num);
            return;
        }

        // switch on msg_req.id, process msg_req, and construt msg_resp
        switch (msg_req.id) {
        case MSGID_GET_RATE: {
            int pulse_rate, gpio_read_rate, gpio_read_and_analyze_rate;

            sipm_get_rate(&pulse_rate, &gpio_read_rate, &gpio_read_and_analyze_rate);

            msg_resp.magic = MSG_RESP_MAGIC;
            msg_resp.id = MSGID_GET_RATE;
            msg_resp.seq_num = msg_req.seq_num;
            msg_resp.get_rate.pulse_rate = pulse_rate;
            msg_resp.get_rate.gpio_read_rate = gpio_read_rate;
            msg_resp.get_rate.gpio_read_and_analyze_rate = gpio_read_and_analyze_rate;
            break; }
        default:
            ERROR("invalid msg_req.id %d\n", msg_req.id);
            return;
        }

        // send msg_resp to clinet
        len = do_send(sfd, &msg_resp, sizeof(msg_resp));
        if (len != sizeof(msg_resp)) {
            ERROR("do_send ret=%d, %s\n", len, strerror(errno));
            return;
        }
    }
}
