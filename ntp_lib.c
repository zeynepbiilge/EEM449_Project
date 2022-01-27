#include <stdint.h>
#include <string.h>
#include <time.h>

#include <xdc/std.h>
#include <xdc/runtime/System.h>

#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/hal/Seconds.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/drivers/GPIO.h>
#include <ti/net/http/httpcli.h>
#include <ti/net/sntp/sntp.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ti/net/socket.h>

#include "Board.h"


#define NTP_HOSTNAME     "pool.ntp.org"
#define NTP_PORT         "123"
#define NTP_SERVERS      3
#define NTP_SERVERS_SIZE (NTP_SERVERS * sizeof(struct sockaddr_in))

unsigned char ntpServers[NTP_SERVERS_SIZE];

static Semaphore_Handle semHandle = NULL;

time_t ts;

void timeUpdateHook(void *p)
{
    Semaphore_post(semHandle);
}

Void startNTP(UArg arg1, UArg arg2)
{
    int ret;
    int currPos;
  //  time_t ts;
    struct sockaddr_in ntpAddr;
    struct addrinfo hints;
    struct addrinfo *addrs;
    struct addrinfo *currAddr;
    Semaphore_Params semParams;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;

    ret = getaddrinfo(NTP_HOSTNAME, NTP_PORT, NULL, &addrs);
    if (ret != 0) {
        System_printf("startNTP: NTP host cannot be resolved!\n");
    }

    currPos = 0;

    for (currAddr = addrs; currAddr != NULL; currAddr = currAddr->ai_next) {
        if (currPos < NTP_SERVERS_SIZE) {
            ntpAddr = *(struct sockaddr_in *)(currAddr->ai_addr);
            memcpy(ntpServers + currPos, &ntpAddr, sizeof(struct sockaddr_in));
            currPos += sizeof(struct sockaddr_in);
        }
        else {
            break;
        }
    }

    freeaddrinfo(addrs);

    ret = SNTP_start(Seconds_get, Seconds_set, timeUpdateHook,
                     (struct sockaddr *)&ntpServers, NTP_SERVERS, 0);
    if (ret == 0) {
        System_printf("startNTP: SNTP cannot be started!\n");
        System_flush();
        BIOS_exit(-1);
    }

    Semaphore_Params_init(&semParams);
    semParams.mode = Semaphore_Mode_BINARY;
    semHandle = Semaphore_create(0, &semParams, NULL);
    if (semHandle == NULL) {
       System_printf("startNTP: Cannot create semaphore!");
       System_flush();
       BIOS_exit(-2);
    }

    SNTP_forceTimeSync();

    Semaphore_pend(semHandle, BIOS_WAIT_FOREVER);

    ts = time(NULL);
    ts += 10800;
    System_printf("Current time: %s\n", ctime(&ts));
    System_flush();
}
