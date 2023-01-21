/* includes */

#include "vxWorks.h"
#include "motor.h"



void start(void) {
	int end_tasks = 0;
	
	taskSpawn("tMotor", 100, 0, 4096, (FUNCPTR) motorTask, &end_tasks, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	
	char c;
	while(1) {
		c = (char)getchar();
		if (c == 'q' || c == 'Q') break;
	}
	
	end_tasks = 1;
		
}


