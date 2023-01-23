/**
 * \file dkm.c
 * \author Jan Svrcina
 * 
 * Entry point for the program, global variables
*/

/**
 * \mainpage Real-Time systems programming - Motor Control
 * This is the semetral project for [Real-Time systems programming course](https://rtime.felk.cvut.cz/psr/cviceni/semestralka/) on FEE CTU.
 *
 * ## Assignement
 * The goal of the semestral work is to create a digital motor controller. This program controls the position of the motor according to the set-point given by the position of another motor, moved by hand (steer-by-wire). The set-point will be transferred between the two motor controllers using UDP messages.
 *
 * ## Compilation and usage
 *
 * The program is created for the embedded system [Zynq-7000](https://www.xilinx.com/support/documentation/user_guides/ug585-Zynq-7000-TRM.pdf) and provided electrical motor, written in **C language**, compiled and loaded onto the board using **Wind River Workbench IDE**. The program is downloadable kernel module, started using entry point `start`, with parameters `ip_address` and `port`. If the progrems is started with `ip_address` as `NULL` or `""`, it will start in the **slave** mode, controlling the motor with incomming information from the UDP on specified port, otherwise it will start in the **master** mode, sending information about the motor via UDP to the specified `ip_address` and `port`. Program can be terminated by sending `q` or `Q` via the serial port.
 * 
 * ```
 * start <ip address> <port> # to start master
 * start "" <port>           # to start slave
 * ```
 * ## User Interface
 * Simple text output bar is showing the duty of the PWM is send via serial port of the **slave**.
 * ```
 * -0.523 | ---------.......... |
 *  0.312 | +++................ |
 * ```
 * 
 * ## Solution description
 * 
 * The program runs in two modes **slave** and **master**. The full functionally is done using four processes (tasks). These process are communicating with each other using global variables, passed through pointers, and being synchronized by a single binary semaphore `update_sem`.
 * 
 * \image html psr_motor.svg
 * \image latex pst_motor.svg "Diagram" width=15cm
 * 
 * ### Shared variables
 * 
 * - `update_sem` - binary semaphore for synchronizing the updates between tasks
 * - `end_tasks` - boolean to end the loop on all running tasks
 * - `target_steps` -  pointer to integer marking where the target of the motor position is
 * - `pwm_duty` - float with current duty percentage, signed based on direction

 * ### Tasks and ISR
 * - `tStart` - entry of the program, sets up global variable to their initial state, selects the mode based on input, starts up other tasks, waits in loop to recieve `q` or `Q` to signal the other tasks to end, cleans motor structure at the end
 * - `tMotor` - initializes the motor driver, registeres the Interrupt Service Request (ISR) for the IRC encoder
 *   - **slave** - in loop calculates and sets the PWM width, action value of the controller, using a simple P regulator based on value in `target_steps` and current step counter `steps`, waiting for signal from `update_sem`
 *   - **master** - sets the `target_steps` variable to the adress of `steps`, for master the target is the current position of the counter, no loop is required, new information is passed by `motorISR`
 * - `motorISR` - interrupt request called on every change of the IRC encoders, decodes the direction of turning and increments or decrements the `steps` counter, gives a signal to `update_sem`
 * - `tUdp` - sets up UDP socket structure for communication,
 *   - **slave** - sets the `target_steps` variable to the adress of recieved data, in loop receive a single integer from UDP on specified port and sets it to `target_steps`, gives signal to `update_sem`
 *   - **master** - in loop waits for signal from `update_sem`, sends current postion in `target_steps`
 * - `tReport` - for **master** prints out current duty cycle with bar to serial port every 50 timer ticks specified in `taskDelay`
 * 
 * ### Driver
 * The driver for the motor was mostly taken from [project assignement](https://rtime.felk.cvut.cz/psr/cviceni/semestralka/#toc-entry-12), extended by `motorShutdown` procedure.
 * 
 * ### Diagram of parts
 * Note: we are currently not using the HTTP server.
 * 
 * ![connection](https://rtime.felk.cvut.cz/psr/cviceni/semestralka/connection.png)
 * 
 * ## Documentation
 * Documentation for the project can be generated using [Doxygen](https://www.doxygen.nl/)
 * ```
 * doxygen dconfig
 * ```
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



/**
 * \brief for **slave** prints out current duty cycle with bar to serial port every 50 timer ticks specified in `taskDelay`
 */
#define BAR_SIZE 20
void reportTask()
{
	char buf[BAR_SIZE+1];
	buf[BAR_SIZE] = '\0';
	while(!end_tasks) {	
		int i, k = round(abs(BAR_SIZE*pwm_duty));
		char sign = (pwm_duty >= 0) ? '+' : '-';
		
		for (i = 0; i < k; i++) buf[i] = sign;
		for ( ; i < BAR_SIZE; i++) buf[i] = '.';
				
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

