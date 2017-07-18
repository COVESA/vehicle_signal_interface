#include <linux/can.h>
#include <linux/can/raw.h>
#include <net/if.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <vsi.h>
#include "signals.h"
#include "vsi_core_api.h"
#include "can-signals.h"


#define MAX_SIG_CNT (20)
#define MAX_SIG_DATA (16)

extern struct CanSignal geniviDemoSignals[];
extern uint32_t geniviDemoSignals_cnt;
static int is_running;
static int exit_code;
static char canIfName[IFNAMSIZ];
typedef void (*syslog_t)(int priority, const char* format, ...);
static syslog_t syslogger = syslog;
static bool updates_only = false;
uint32_t sigPrevValues[MAX_SIG_CNT];
char *pidfile = "vsi-socketcand.pid";


static void fake_syslog(int priority, const char* format, ...)
{
    va_list ap;

    printf("[%d] ", priority);
    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);
    printf("\n");
}

static void print_usage(char* prg)
{
    fprintf(stderr, "\nUsage: %s [options] <vsi> <canif-name>\n\n", prg);
    fprintf(stderr, "Options: -F         (stay in foreground; no daemonize)\n");
    fprintf(stderr, "         -u         (update signals. Do not store signals if they haven't changed)\n");
    fprintf(stderr, "         -p pidfile (provide pidfile)\n");
    fprintf(stderr, "         -h         (show this help page)\n");
    fprintf(stderr, "\nExamples:\n");
    fprintf(stderr, "vsi-socketcand vss_rel_1.0.vsi can0\n");
    fprintf(stderr, "vsi-socketcand -F vss_rel_1.0.vsi can0\n");
    fprintf(stderr, "\n");
    exit(-EXIT_FAILURE);
}

static void writePidFile()
{
    int fd;
    char pid[10];
    
    if ((fd = open(pidfile, O_RDWR|O_CREAT, 0600)) == -1) {
        syslogger(LOG_ERR, "cannot open pidfile %s", pidfile);
        exit(-EXIT_FAILURE);
    }
    
    sprintf(pid, "%d\n", getpid());
    
    if (write(fd, pid, strlen(pid)) == -1) {
        syslogger(LOG_ERR, "cannot write pidfile");
        close(fd);
        exit(-EXIT_FAILURE);
    }
    
    close(fd);
}

static void child_handler(int signum)
{
    switch (signum) {

    case SIGUSR1:
        /* exit parent */
        exit(EXIT_SUCCESS);
        break;
    case SIGALRM:
    case SIGCHLD:
        syslogger(LOG_NOTICE, "received signal %i on %s", signum, canIfName);
        exit_code = -EXIT_FAILURE;
        is_running = 0;
        break;
    case SIGINT:
    case SIGTERM:
        syslogger(LOG_NOTICE, "received signal %i on %s", signum, canIfName);
        exit_code = -EXIT_SUCCESS;
        is_running = 0;
        break;
    }
}

static int openCANSocket(const char* canName)
{
    int s;
    struct ifreq ifr;
    struct sockaddr_can addr;

    if ((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
        syslogger(LOG_ERR, "Error while opening socket");
        return -EXIT_FAILURE;
    }

    strncpy(ifr.ifr_name, canName, IFNAMSIZ);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';
    ifr.ifr_ifindex = if_nametoindex(ifr.ifr_name);

    if (!ifr.ifr_ifindex) {
        syslogger(LOG_ERR, "Faild to convert name (%s) to net index", canName);
        return -EXIT_FAILURE;
    }

    ioctl(s, SIOCGIFINDEX, &ifr);

    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        syslogger(LOG_ERR, "Error in socket bind on %s interface", canName);
        return -EXIT_FAILURE;
    }

    return s;
}

static int readCAN(int canFd, struct can_frame* frame)
{
    fd_set rfds;
    struct timeval tv = {.tv_usec = 0, .tv_sec = 1 };
    int retval;

    FD_ZERO(&rfds);
    FD_SET(canFd, &rfds);

    retval = select(canFd + 1, &rfds, NULL, NULL, &tv);

    if (retval > 0) {
        retval = read(canFd, frame, sizeof(struct can_frame));
    }

    return retval;
}

static void sigUInt8Clbk(const char* name, uint32_t id, uint8_t value)
{
    int status = 0;
    vsi_result result = {.signalId = id,
        .domainId = 1,
        .name = (char*)name,
        .data = (char*)&value,
        .dataLength = sizeof(value) };

    if((id < MAX_SIG_CNT) && (updates_only && value == sigPrevValues[id])) {
        syslogger(LOG_DEBUG, "Ignore signal %s(%d). Sig max: %d, updates: %hhu, val: %hhu", 
                name, id, MAX_SIG_CNT, updates_only, value);
        return;
    }

    sigPrevValues[id] = value;

    syslogger(LOG_INFO, "Signal received - name: %s, id: %u, val(u8): %hhu", name, id, result.data[0]);

    vsi_insert_signal (&result);

    if (status) {
        syslogger(LOG_ERR, "Failed to store signal %s(%d), val: %hhu", name, id, value);
    }
}

static void sigUInt16Clbk(const char* name, uint32_t id, uint16_t value)
{
    int status = 0;
    vsi_result result = {.signalId = id,
        .domainId = 1,
        .name = (char*)name,
        .data = (char*)&value,
        .dataLength = sizeof(value) };

    if((id < MAX_SIG_CNT) && (updates_only && value == sigPrevValues[id])) {
        syslogger(LOG_DEBUG, "Ignore signal %s(%d). Sig max: %d, updates: %hhu, val: %hu", 
                name, id, MAX_SIG_CNT, updates_only, value);
        return;
    }

    sigPrevValues[id] = value;

    syslogger(LOG_INFO, "Signal received - name: %s, id: %u, val(u16): %hu", name, id, result.data);

    vsi_insert_signal (&result);

    if (status) {
        syslogger(LOG_ERR, "Failed to store signal %s(%d), val: %hu", name, id, value);
    }
}

static void sigBoolClbk(const char* name, uint32_t id, bool value)
{
    int status = 0;
    vsi_result result = {.signalId = id,
        .domainId = 1,
        .name = (char*)name,
        .data = (char*)&value,
        .dataLength = sizeof(value) };
 
    if((id < MAX_SIG_CNT) && (updates_only && value == sigPrevValues[id])) {
        syslogger(LOG_DEBUG, "Ignore signal %s(%d). Sig max: %d, updates: %hhu, val: %hhu", 
                name, id, MAX_SIG_CNT, updates_only, value);
        return;
    }

    sigPrevValues[id] = value;

    syslogger(LOG_INFO, "Signal received - name: %s, id: %u, val(bool): %hhu", name, id, result.data);

    vsi_insert_signal (&result);

    if (status) {
        syslogger(LOG_ERR, "Failed to store signal %s(%d), val: %hhu", name, id, value);
    }
}

int main(int argc, char** argv)
{
    char* canIf = NULL;
    char* vssName = NULL;
    int opt;
    int run_as_daemon = 1;
    int canFd = -1;
    struct can_frame frame;
    int readCnt = 0;

    while ((opt = getopt(argc, argv, "u?hFp:")) != -1) {
        switch (opt) {
        case 'u':
            updates_only = true;
            break;
        case 'F':
            run_as_daemon = 0;
            break;
        case 'p':
            pidfile = strdup(optarg);
            break;
        case 'h':
        case '?':
        default:
            print_usage(argv[0]);
            break;
        }
    }

    memset(sigPrevValues, 0xffffffff, sizeof(sigPrevValues));

    if (!run_as_daemon)
        syslogger = fake_syslog;

    /* Parse vss filename and can interface name */
    vssName = argv[optind];
    if (NULL == vssName)
        print_usage(argv[0]);

    canIf = argv[optind + 1];
    if (NULL == canIf)
        print_usage(argv[0]);

    strncpy(canIfName, canIf, strnlen(canIf, IFNAMSIZ));

    vsi_initialize(false);

    canFd = openCANSocket(canIf);
    if (canFd < 0) {
        syslogger(LOG_ERR, "Failed to open CAN interface (%s)", canIf);
        exit(-EXIT_FAILURE);
    }

    if (!initCanSignals(geniviDemoSignals, geniviDemoSignals_cnt, syslogger, sigBoolClbk, sigUInt8Clbk, sigUInt16Clbk)) {
        syslogger(LOG_ERR, "Failed to intialize CAN signal extraction\n");
        exit(-EXIT_FAILURE);
    }

    /* Daemonize */
    if (run_as_daemon) {
        if (daemon(1, 0)) {
            syslogger(LOG_ERR, "failed to daemonize");
            exit(-EXIT_FAILURE);
        }
        writePidFile();
    }

    /* Trap signals that we expect to receive */
    signal(SIGINT, child_handler);
    signal(SIGTERM, child_handler);

    syslogger(LOG_INFO, "Started on %s interface using %s signal specification.", canIf, vssName);

    is_running = 1;

    while (is_running) {
        readCnt = readCAN(canFd, &frame);

        if (readCnt > 0) {
            processCanFrame(&frame);
        }
    }

    syslogger(LOG_INFO, "Bye Bye (%s).", canIf);

    vsi_core_close();

    return 0;
}
