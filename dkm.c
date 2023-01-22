/* includes */

#include <vxWorks.h>
#include <semLib.h>

#include "motor.h"
#include "udp.h"
#include "string.h"
#include "stdio.h"
#include "math.h"

SEM_ID update_sem;
int end_tasks;
int *target_steps;
float pwm_duty;


#define BAR_SIZE 20
void reportTask()
{
	char buf[BAR_SIZE+1];
	buf[BAR_SIZE] = '\0';
	while(!end_tasks) {	
		int i, k = round(BAR_SIZE*pwm_duty);
		char sign = (pwm_duty >= 0) ? '+' : '-';
		
		for (i = 0; i < k; i++) buf[i] = sign;
		for ( ; i < BAR_SIZE; i++) buf[i] = '.';
				
		printf("%5.3f | %s |\r", pwm_duty, buf);
		taskDelay(50);
	}
}

void start(char *ip_address, int port) {
	
	int is_slave;
	update_sem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	
	if (strcmp(ip_address, "") == 0) {
		ip_address = NULL;
	}
	
	is_slave = (ip_address == NULL);
	end_tasks = 0;
	*target_steps = 0;
	
	taskSpawn("tMotor", 90, 0, 4096, (FUNCPTR) motorTask, &update_sem, &target_steps, &end_tasks, is_slave, &pwm_duty, 0, 0, 0, 0, 0);
	taskSpawn("tUdp", 90, 0, 4096, (FUNCPTR) udpTask, &update_sem, &target_steps, &end_tasks, is_slave, ip_address, port, 0, 0, 0, 0);
	taskSpawn("tRepor", 110, 0, 4096, (FUNCPTR) reportTask, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	
	char c;
	while(1) {
		c = getchar();
		if (c == 'q' || c == 'Q') break;
	}
	
	end_tasks = 1;
	taskDelay(10);
	semGive(update_sem); // release the task
	motorShutdown();
	printf("Ending all \n");
}

