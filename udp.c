/* includes */

#include "udp.h"

int open_udp_socket(void)
{
	int sockd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockd == -1) {
		perror("Socket creation error\n");
		exit(1);	
	}
	
	return sockd;
}

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

void udpTask(SEM_ID *update_sem, int **target_step, int *end_tasks, int is_slave, char *ip_address, int port)
{
	int pos = 0;
	
	
	int sockd;
	struct sockaddr_in my_addr, target_addr;
	
	printf("Starting udp task - %s\n", is_slave ? "slave" : "master");
	
	// setup udp com
	sockd = open_udp_socket();
	my_addr = set_my_addr(sockd, port);
	target_addr = set_target_addr(ip_address, port);
		
	if (is_slave) {
		*target_step = &pos;
		
		int status, addr_len;
		
		while(!(*end_tasks)) {
			addr_len = sizeof(target_addr);
			status = recvfrom(sockd, &pos, sizeof(pos), 0, (struct sockaddr *)&target_addr, &addr_len);
			printf("recv %d\n", pos);
			semGive(*update_sem);
		}
	}
	else { // master
		while(!(*end_tasks)) {
			semTake(*update_sem, WAIT_FOREVER);
//			taskDelay(1);
			pos = **target_step;
			sendto(sockd, &pos, sizeof(pos), 0, (struct sockaddr *)&target_addr, sizeof(target_addr));
			printf("sent %d\n", pos);
		}
	}
	
	printf("Ending udp task - %s\n", is_slave ? "slave" : "master");
	close(sockd);
}


