/* includes */

#include <vxWorks.h>
#include <semLib.h>

#include "motor.h"
#include "udp.h"
#include "string.h"
#include "stdio.h"

SEM_ID update_sem;
int end_tasks;
int *target_steps;

void start(char *ip_address, int port) {
	
	int is_slave;
	update_sem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	
	if (strcmp(ip_address, "") == 0) {
		ip_address = NULL;
	}
	
	is_slave = (ip_address == NULL);
	end_tasks = 0;
	
	taskSpawn("tMotor", 100, 0, 4096, (FUNCPTR) motorTask, &update_sem, &target_steps, &end_tasks, is_slave, 0, 0, 0, 0, 0, 0);
	taskSpawn("tUdp", 100, 0, 4096, (FUNCPTR) udpTask, &update_sem, &target_steps, &end_tasks, is_slave, ip_address, port, 0, 0, 0, 0);
	
	char c;
	while(1) {
		c = getchar();
		if (c == 'q' || c == 'Q') break;
	}
	
	end_tasks = 1;
	semGive(update_sem); // release the task
	motorShutdown();
	printf("Ending\n");
}

