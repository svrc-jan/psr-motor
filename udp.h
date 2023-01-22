#ifndef UDP_H_
#define UDP_H_

/**
 * \file udp.c
 * \author Jan Svrcina
 * 
 * UDP communication
*/


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



/**!
 * struct to save UDP connection info
*/
struct udp {
	int sockd;
	struct sockaddr_in client_addr;
	struct sockaddr_in server_addr;
};

/**
 * \brief up UDP socket structure for communication,
 *   - **slave** - sets the `target_steps` variable to the adress of recieved data, in loop receive a single integer from UDP on specified port and sets it to `target_steps`, gives signal to `update_sem`
 *   - **master** - in loop waits for signal from `update_sem`, sends current postion in `target_steps`
 * \param update_sem pointer to global synchronazition semaphore `updade_sem`
 * \param target_step pointer to `target_step` reference
 * \param end_task pointer to `end_task` global variable {0, 1}, signals to end all loops
 * \param is_slave binary value assigning mode, 0 for **master**, 1 for **slave**
 * \param ip_address IP address of target, NULL for slave
 * \param port selected port
 */
void udpTask(SEM_ID *update_sem, int **target_step, int *end_tasks, int is_slave, char *ip_address, int port);

#endif /* UDP_H_ */
