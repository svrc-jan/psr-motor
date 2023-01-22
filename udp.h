#ifndef UDP_H_
#define UDP_H_


#include <inetLib.h>
#include <sockLib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <semLib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysLib.h>
#include <stdbool.h>
#include "vxWorks.h"



/**
 * @brief Structure to store parameters for UDP communication between 
 * the sender and the receiver modes.
 *
 */
/**/
typedef struct _UDP {
	/**
	 * @brief parameter of mode: sender or receiver.
	 *
	 */
	bool isSender;
	/**
	 * @brief Value of desired motor position.
	 *
	 */
	int desired_pos;
	/*Parameters for connection
	 * 
	 */
	int sockd, status, count;
	socklen_t addrlen;
	struct sockaddr_in my_addr, srv_addr;
} UDP;

/**
 * @brief Initialization of UDP structure. If the IP address is given the UDP 
 * connection is set for the Sender mode. If just an empty string ' ' is given, 
 * then it is running in the receiver mode,.
 *
 * @param ipAddress string of target IP address for sender mode, 
 * 					empty string, ' ', for receiver mode. 
 * @param port Number with target IP protocols
 * @return UDP structure
 */
UDP *initudp(char *ipAddress, int port);

/**
 * @brief This function sends data of a given size to target IP address stored 
 * in UDP structure.
 *
 * @param udp The UDP structure
 * @param data data to be sent, desired position from controller motor
 * @param size payload of the data
 */
void sendUDPData(UDP *udp, int data, int size);

void receiveUDPData(UDP *udp, int data, int payload);


#endif /* UDP_H_ */
