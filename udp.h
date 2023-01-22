#ifndef UDP_H_
#define UDP_H_

#include "vxWorks.h"


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inetLib.h>
#include <sockLib.h>
#include <sysLib.h>
#include <semLib.h>

#define MAX_BUF 100

struct udp {
	int sockd;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;
};


void udpTask(SEM_ID *update_sem, int **target_step, int *end_tasks, int is_slave, char *ip_address, int port);

#endif /* UDP_H_ */
