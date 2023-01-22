/* includes */

#include "vxWorks.h"


void start(void) {
	int end_tasks = 0;
	int target_steps = 0;
	printf("Motor\n");
	
	taskSpawn("tMotor", 100, 0, 4096, (FUNCPTR) motorTask, &target_steps, &end_tasks, 0, 0, 0, 0, 0, 0, 0, 0);
	
	char c;
	int i = 0;
	while(1) {
		i = (i + 1) % 5;
		target_steps = (i - 2)*512;
		c =	 (char)getchar();
		
		
		if (c == 'q' || c == 'Q') break;
	}
	
	end_tasks = 1;
		
}
