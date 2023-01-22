/* includes */

#include "vxWorks.h"
#include "udp.h"
#include <taskLib.h>
/* I think we are only sending one int at each time:*/
#define payload 4
static int end;
static UDP *udp;

void start(char *ip_address, int port) {
	
	udp = initudp(ip_address, port);

	/* MASTER */
	  sprintf(taskName, "tControlMotor");
	  TASK_ID tControlMotor = taskSpawn(
	  taskName, 110, 0, 4096, (FUNCPTR)getDesiredPos,
	         (UDP *)udp, (int)payload, end, 0, 0, 0, 0, 0, 0, 0);
}
