#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>

/* libev is needed */
#include <ev.h>
#include <stdbool.h>

/*the header file of logger module */
#include "log.h"

/* modify values here */
#define INPUTBUF    64

/*Conditional Compilation on Debug */
#ifdef DEBUG
#define debug(...) do { fprintf(stderr, __VA_ARGS__); } while(0)
#else
#define debug(...) do {} while(0)
#endif
#define TRUE 0

/*Global variables and function declarations */
static void usage(void);
static int  backend_supported(void);
static int  set_nb_fd(int);
static int  send_policy(int);
static void timeout_cb(struct ev_loop *cl, struct ev_timer *w, int);
static void client_cb(EV_P_ struct ev_io *w, int);
static void net_cb(EV_P_ struct ev_io *w, int);

static char sendbuf[1500] = {0};
char *      logfile = NULL;
int         log_level = 0;
ev_timer    timeout_watch;
ev_io       net_watch;
ev_io       client_watch;

/*
 *
 *Function definition & Explanation
 *
 */

/* usage
 * type: normal function
 *
 * Description:  just informs messages to stderr
 * what we will do
 */
static void usage(void) {
    fprintf(stderr, "fpolicyd is designed to send a policy to flash clients.\n\n");
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "fpolicyd <-p port> <-f policy.xml> <-l loglevel<0|1|2|3>> <-r logfile>\n\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "-p port\t: listening port\n");
    fprintf(stderr, "-f policy.xml\t: policy file to send\n");
    fprintf(stderr, "-l loglevel\t: log output level\n\t\t0 = DEBUG\n\t\t1 = INFO\n\t\t2 = WARN\n\t\t3 = EMERGE\n" );
    fprintf(stderr, "-r logfile\t: log file path\n");
}

/* backend_supported
 * type: normal function
 *
 * Description: specify which backend are available
 * returns              0 = neither "epoll" nor  "kqueue"
 *          1 = kqueue
 *          2 = epoll
 */
static int backend_supported(void) {
    unsigned int kqueue = 1;
    unsigned int epoll = 2;
    unsigned int backend = 0;

    if ((ev_supported_backends() & EVBACKEND_EPOLL) == 4) {
                backend = epoll;
    }
    if ((ev_supported_backends() & EVBACKEND_KQUEUE) == 8) {
                backend = kqueue;
    }

    return backend;
}

/* set_nb_fd
 * type: normal function
 *
 * Description: sets fd to non-blocking mode
 */
static int set_nb_fd(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return flags;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        return -1;
    }

    return 0;
}

/* send_policy
 * type: normal function
 *
 * Description: send policy to our client
 * returns length of recv buffer
 */
static int send_policy(int sock) {
    static char rcvbuf[INPUTBUF] = {0};
    static char token[64] = {0};
    size_t len = 0;

    /* nullify our chains */
    /* compute our token */
    snprintf(token, 23, "<policy-file-request/>");
    /* see what's our client has to say */
    len = recv(sock, rcvbuf, INPUTBUF, 0);

    if (len == 0) {
        return 0;
    }

    /* check if we could send our policy */
    if (strncmp(rcvbuf, token, 23) == 0) {
                /* answering */
        log_info( "LOG_INFO: sending policy file\n");
                send(sock, sendbuf, sizeof(sendbuf), 1);
    }

    return len;
}

/* timeout_cb
 * type: callback
 *
 * Description: unloop on timeout
 */
static void timeout_cb(EV_P_ struct ev_timer *w, int revents) {
    log_warn("LOG_WARN: timeout: disconnecting client\n");
        ev_unloop(EV_A_ EVUNLOOP_ONE);
}

/* client_cb
 * type: callback
 *
 * Description: disconnects client
 * stop timer
 */
static void client_cb(EV_P_ struct ev_io *w, int revents) {
    /* disconnect after recv'd answer */
    if (send_policy(w->fd) < 0) {
        close(w->fd);
        logger_close();
        exit(1);
    }

    /* stop the timer here before unlooping */
    ev_timer_stop(loop, &timeout_watch);
    ev_unloop(EV_A_ EVUNLOOP_ONE);
    log_info("LOG_INFO: disconnected\n");
}

/* net_cb
 * type: callback
 *
 * Description: verifies tcp negociation
 * starts client loop on event
 */
static void net_cb (EV_P_ struct ev_io *w, int revents) {
    struct sockaddr_in addr;
    struct ev_loop *client_loop;
    int             sock = 0;
    unsigned int    backend = 0;

    socklen_t len = sizeof(addr);

    /* accept S/A */
    if ((sock = accept(w->fd, (struct sockaddr*) &addr, &len)) < 0) {
        if (EINTR == errno) { return; }
        close(sock);
        logger_close();
        err(1, "accept");
    }
    log_info("LOG_INFO: %s connected\n", inet_ntoa(addr.sin_addr));

        /* handle our client */
        /* backend decision process */
        backend = backend_supported();
    switch(backend) {
        case 1:
            client_loop = ev_loop_new(EVBACKEND_KQUEUE);
            break;
        case 2:
            client_loop = ev_loop_new(EVBACKEND_EPOLL);
            break;
        default:
        /* trying to take the best option still available */
            client_loop = ev_loop_new(0);
    }

    ev_io_init(&client_watch, client_cb, sock, EV_READ);
    ev_io_start(client_loop, &client_watch);

    /* don't forget the timer */
    /* defined here to 5 secs */
    ev_timer_init(&timeout_watch, timeout_cb, 5, 0.);
    ev_timer_start(client_loop, &timeout_watch);

    /* starting our new loop !*/
    ev_loop(client_loop, 0);

    /* what we do after unloop ? */
    close(sock);
    ev_io_stop(client_loop, &client_watch);
    ev_timer_stop(client_loop, &timeout_watch);
    ev_unloop(client_loop, EVUNLOOP_ONE);
    /* needed for epoll_create() */
    ev_loop_destroy(client_loop);
}

/* main function
 *
 * here is the main loop
 */
int main(int argc, char *argv[]) {
    /* Initialization */
    struct sockaddr_in listen_addr = {0};
    struct ev_loop *loop = NULL;
    int tcp_opt = 1;
    int sock = 0;
    int backend = 0;
    int arg = 0;
    int p = 0;
    int f = 0;
    int l = 0;
    int r = 0;
    unsigned short   sport = 0;
    char         policyfn[80] = {0};
    FILE        *policy = NULL;


    /* Start Daemon Process */
    if (daemon(1,1) < 0){
        printf("create daemon fail\n");
        exit(1);
    }


        /* Parameters Checking in Command Line */
    if (argc < 5) {
        usage();
        exit(1);
    }

    while ((arg = getopt(argc, argv, "p:f:l:r:h")) != -1) {
        switch (arg) {
            case 'p':
                sport=atoi(optarg);
                p++;
                break;
            case 'f':
                strncpy(policyfn, optarg, (sizeof(policyfn) - 1));
                f++;
                break;
            case 'l':
                log_level = atoi(optarg);
                l++;
                break;
            case 'r':
                logfile = optarg;
                if (NULL != logfile) {
                    r++;
                }
                break;
            case 'h':
            default:
                usage();
                exit(1);
        }
    }

    if (!p || !f || !l || !r) {
        fprintf(stderr, "Sorry we needs both -p and -f and -l and -r specified!\n");
        exit(1);
    }

    if (log_level < 0 || log_level > 3) {
        fprintf(stderr, "log level must be one of 0 1 2 3\n");
        exit(1);
    }

    /* Log Module Initialization */
    if(logger_init(log_level, logfile) < 0 ){
        printf("init log fail,please kill process and restart process\n");
        logger_close();
        exit(1);
    }
    log_debug("LOG_DEBUG: Flash Policy Daemon been start:\n");
    log_debug("LOG_DEBUG: Backend decision process:\n");

    /* backend decision process */
    backend = backend_supported();

    switch(backend) {
        case 1:
            log_debug( "LOG_DEBUG: KQUEUE is supported ! we enable it.\n");
            loop = ev_default_loop(EVBACKEND_KQUEUE);
            break;
        case 2:
            log_debug("LOG_DEBUG: EPOLL is supported ! we enable it.\n");
            loop = ev_default_loop(EVBACKEND_EPOLL);
            break;
        default:
            log_debug("LOG_DEBUG: Neither EPOLL or KQUEUE are detected\n");
            /* trying to take the best option still available */
            loop = ev_default_loop(0);
    }

    /* compute our response */
    log_debug("LOG_DEBUG: Storing our policy in memory...\n");
    if ((policy = fopen(policyfn, "r")) == NULL) {
        perror("Error for opening the policy file");
        log_emerg("LOG_EMERG: Please put the %s file in\n", policyfn);
        logger_close();
        exit(-1);
    }
    fread(&sendbuf, sizeof(sendbuf), 1, policy);
    fclose(policy);

    log_debug("LOG_DEBUG: Binding our socket [%d]\n", sport);

    /* create our socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        log_emerg("LOG_EMERG: create socket failed.\n");
        close(sock);
        logger_close();
        err(1, "create socket failed");
    }
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(sport);
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,&tcp_opt, sizeof(tcp_opt)) < 0){
        printf("setsockopt fail\n");
        close(sock);
        logger_close();
        exit(1);
    }
    /* setting to non blocking mode */
    if (set_nb_fd(sock) < 0) {
        log_emerg("LOG_EMERG: failed to set non-blocking socket\n");
        close(sock);
        logger_close();
        err(1, "failed to set non-blocking socket");
    }

    if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        log_emerg("LOG_EMERG: bind failed.\n");
        close(sock);
        logger_close();
        err(1, "bind [%d] failed.\n", sport);
    }
    if (listen(sock, 5) < 0) {
        log_emerg("LOG_EMERG: listen failed.\n");
        close(sock);
        logger_close();
        err(1, "listen failed");
    }

    log_debug("LOG_DEBUG: Socket created and listening.\n");

    /* prepare our main loop */
    log_debug("LOG_DEBUG: Ready to process clients\n");
    while (1) {
        net_watch.data = loop;
        ev_io_init(&net_watch, net_cb, sock, EV_READ);
        ev_io_start(loop, &net_watch);

        /* start */
        ev_loop(loop, 0);

        /* on returns */
        ev_io_stop(loop, &net_watch);
    }
    /* never returns.. */
    close(sock);
    logger_close();
    return 0;
}
/* EOP */
