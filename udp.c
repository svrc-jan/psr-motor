/**
 * \file udp.c
 * \author Jan Svrcina
 * 
 * UDP communication
*/

#include "udp.h"

/**
 * \brief open UDP socket
 * \return socked ID
*/
int open_udp_socket(void)
{
	int sockd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockd == -1) {
		perror("Socket creation error\n");
		exit(1);	
	}
	
	return sockd;
}

/**
 * \brief setup my address for UDP and bind it to socket
 * \param sockd socket ID
 * \param port selected port
*/
struct sockaddr_in set_my_addr(int sockd, int port)
{
	struct sockaddr_in addr;
	
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	
	if (port != NULL) {
		addr.sin_port = htons(port);
	}
	else {
		addr.sin_port = 0;
	}
	
	if (bind(sockd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
		perror("Socket bind error\n");
		exit(1);
	}
	
	return addr;
}

/**
 * \brief setup target address for UDP
 * \param ip_address target IP adress, NULL for slave
 * \param port selected port
*/

struct sockaddr_in set_target_addr(char ip_addr[], int port)
{
	struct sockaddr_in addr;
	
	addr.sin_family = AF_INET;
	
	if (ip_addr != NULL) {
		inet_aton(ip_addr, &addr.sin_addr);
	}
	else {
		addr.sin_addr.s_addr = INADDR_ANY;
	}
	
	if (port != NULL) {
		addr.sin_port = htons(port);
	}
	else {
		addr.sin_port = 0;
	}
	
	return addr;
}

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
void udpTask(SEM_ID *update_sem, int **target_step, int *end_tasks, int is_slave, char *ip_address, int port, int *sock_ptr)
{
	int pos = 0;
	
	
	int sockd;
	struct sockaddr_in my_addr, target_addr;
	
	printf("Starting udp task - %s\n", is_slave ? "slave" : "master");
	
	// setup udp com
	sockd = open_udp_socket();
	my_addr = set_my_addr(sockd, port);
	target_addr = set_target_addr(ip_address, port);
	*sock_ptr = sockd;
		
	if (is_slave) {
		*target_step = &pos;
		
		int status, addr_len;
		
		while(!(*end_tasks)) {
			addr_len = sizeof(target_addr);
			status = recvfrom(sockd, &pos, sizeof(pos), 0, (struct sockaddr *)&target_addr, &addr_len);
			semGive(*update_sem);
		}
	}
	else { // master
		while(!(*end_tasks)) {
			semTake(*update_sem, WAIT_FOREVER);
//			taskDelay(1);
			pos = **target_step;
			sendto(sockd, &pos, sizeof(pos), 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
		}
	}
	
	printf("Ending udp task - %s\n", is_slave ? "slave" : "master");
}


