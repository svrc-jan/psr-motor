/**
 * \file dkm.c
 * \author Jan Svrcina
 * 
 * Entry point for the program, global variables
*/


#include <vxWorks.h>
#include <semLib.h>

#include "motor.h"
#include "udp.h"
#include "string.h"
#include "stdio.h"
#include "math.h"


SEM_ID update_sem; 	//!< binary synchronization semaphore
int end_tasks;		//!< signal task to end their loops
int *target_steps;  //!< target steps of the motor, for **slave** recieved data from UDP, for **master** current pos
float pwm_duty;     //!< singed duty of the PWM (-1, 1)



#define BAR_SIZE 20
/**
 * \brief for **slave** prints out current duty cycle with bar to serial port every 50 timer ticks specified in `taskDelay`
 */
void reportTask()
{
	char buf[BAR_SIZE];
	buf[BAR_SIZE-1] = '\0';
	while(!end_tasks) {	
		int i, k = ceil(abs(BAR_SIZE*pwm_duty));
		char sign = (pwm_duty >= 0) ? '+' : '-';
		
		for (i = 0; i < k; i++) buf[i] = sign;
		for ( ; i < BAR_SIZE-1; i++) buf[i] = '.';
				
		printf("%5.3f | %s |  \r", pwm_duty, buf);
		taskDelay(50);
	}
}

/**
 * \brief entry point of the program, sets up global variable to their initial state, 
 * selects the mode based on input, starts up other tasks, waits in loop to recieve `q` or `Q` 
 * to signal the other tasks to end, cleans motor structure at the end
 * \param ip_address string specifying target for **master** and `""` or `NULL` for **slave**
 * \param port selected port for UDP communication
 */
void start(char *ip_address, int port) {
	
	int is_slave;
	int sockd;
	update_sem = semBCreate(SEM_Q_FIFO, SEM_EMPTY);
	
	if (strcmp(ip_address, "") == 0) {
		ip_address = NULL;
	}
	
	is_slave = (ip_address == NULL);
	end_tasks = 0;
	*target_steps = 0;
	
	taskSpawn("tMotor", 90, VX_FP_TASK, 4096, (FUNCPTR) motorTask, &update_sem, &target_steps, &end_tasks, is_slave, &pwm_duty, 0, 0, 0, 0, 0);
	taskSpawn("tUdp", 90, 0, 4096, (FUNCPTR) udpTask, &update_sem, &target_steps, &end_tasks, is_slave, ip_address, port, &sockd, 0, 0, 0);
	if (is_slave) {
		taskSpawn("tRepor", 110, VX_FP_TASK, 4096, (FUNCPTR) reportTask, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}

	char c;
	while(1) {
		c = getchar();
		if (c == 'q' || c == 'Q') break;
	}
	
	end_tasks = 1;
	taskDelay(10);
	semGive(update_sem); // release the task
	close(sockd);
	motorShutdown();
	taskDelay(10);
	printf("Ending all \n");
}

