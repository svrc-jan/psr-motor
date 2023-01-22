#include "motor.h"
#include "udp.h"

// driver
LOCAL VXB_FDT_DEV_MATCH_ENTRY psrMotorMatch[] = {
    {"cvut,psr-motor", NULL},
    {} /* Empty terminated list */
};

LOCAL STATUS motorProbe(VXB_DEV_ID pInst)
{
        if (vxbFdtDevMatch(pInst, psrMotorMatch, NULL) == ERROR)
                return ERROR;
        VXB_FDT_DEV *fdtDev = vxbFdtDevGet(pInst);
        /* Tell VxWorks which of two motors we want to control */
        if (strcmp(fdtDev->name, "pmod1") == 0)
                return OK;
        else
                return ERROR;
}

static void motorISR(struct psrMotor *pMotor);
// Don't forget to acknowledge the interrupt in the handler by:
// GPIO_INT_STAT(pMotor) = pMotor->gpioIrqBit;



LOCAL STATUS motorAttach(VXB_DEV_ID pInst)
{
    struct psrMotor *pMotor;

    VXB_RESOURCE *pResFPGA = NULL;
    VXB_RESOURCE *pResGPIO = NULL;
    VXB_RESOURCE *pResIRQ = NULL;

    pMotor = (struct psrMotor *)vxbMemAlloc(sizeof(struct psrMotor));
    if (pMotor == NULL) {
        perror("vxbMemAlloc");
        goto error;
    }

    // Map memory for FPGA registers and GPIO registers
    pResFPGA = vxbResourceAlloc(pInst, VXB_RES_MEMORY, 0);
    pResGPIO = vxbResourceAlloc(pInst, VXB_RES_MEMORY, 1);
    if (pResFPGA == NULL || pResGPIO == NULL) {
        fprintf(stderr, "Memory resource allocation error\n");
        goto error;
    }

    pMotor->fpgaRegs = ((VXB_RESOURCE_ADR *)(pResFPGA->pRes))->virtAddr;
    pMotor->gpioRegs = ((VXB_RESOURCE_ADR *)(pResGPIO->pRes))->virtAddr;

    pResIRQ = vxbResourceAlloc(pInst, VXB_RES_IRQ, 0);
    if (pResIRQ == NULL) {
        fprintf(stderr, "IRQ resource alloc error\n");
        goto error;
    }

    int len;
    VXB_FDT_DEV *fdtDev = vxbFdtDevGet(pInst);
    const UINT32 *pVal = vxFdtPropGet(fdtDev->offset, "gpio-irq-bit", &len);
    if (pVal == NULL || len != sizeof(*pVal)) {
        fprintf(stderr, "vxFdtPropGet(gpio-irq-bit) error\n");
        goto error;
    }
    pMotor->gpioIrqBit = vxFdt32ToCpu(*pVal);
    printf("gpioIrqBit=%#x\n", pMotor->gpioIrqBit);

    /* Associate out psrMotor structure (software context) with the device */
    vxbDevSoftcSet(pInst, pMotor);

    // Configure GPIO
    GPIO_INT_STAT(pMotor) |= pMotor->gpioIrqBit; /* reset status */
    GPIO_DIR(pMotor) &= ~pMotor->gpioIrqBit;     /* set GPIO as input */
    GPIO_INT_TYPE(pMotor) |= pMotor->gpioIrqBit; /* generate IRQ on edge */
    GPIO_INT_POL(pMotor) &= ~pMotor->gpioIrqBit; /* falling edge */
    GPIO_INT_ANY(pMotor) &= ~pMotor->gpioIrqBit; /* enable IRQ only on one pin */

    // Connect the interrupt handler
    STATUS s1 = vxbIntConnect(pInst, pResIRQ, motorISR, pMotor);
    if (s1 == ERROR) {
        fprintf(stderr, "vxbIntConnect error\n");
        goto error;
    }
    // Enable interrupts
    if (vxbIntEnable(pInst, pResIRQ) == ERROR) {
        fprintf(stderr, "vxbIntEnable error\n");
        vxbIntDisconnect(pInst, pResIRQ);
        goto error;
    }

    GPIO_INT_EN(pMotor) = pMotor->gpioIrqBit;

    return OK;

error:
    // Free any allocated resources
    if (pResIRQ != NULL) {
        vxbResourceFree(pInst, pResIRQ);
    }
    if (pResGPIO != NULL) {
        vxbResourceFree(pInst, pResGPIO);
    }
    if (pMotor != NULL) {
        vxbMemFree(pMotor);
    }
    return ERROR;
}

struct psrMotor *motor;
void motorShutdown(void)
{
	FPGA_PWM_DUTY(motor) = 0;	
	GPIO_INT_DIS(motor) |= motor->gpioIrqBit;
}


int steps = 0;

struct psrMotor *motorInit()
{
	VXB_DEV_ID motorDrv = vxbDevAcquireByName("pmod1", 0);
	if (motorDrv == NULL) {
		fprintf(stderr, "vxbDevAcquireByName failed\n");
		return NULL;
	}
	
	steps = 0; // reset step counter
	motorProbe(motorDrv);
	motorAttach(motorDrv);
	
	struct psrMotor *pMotor = vxbDevSoftcGet(motorDrv);
	
	// set PWM freq
	FPGA_PWM_PERIOD(pMotor) |= MOTOR_PWM_PERIOD;
	FPGA_CR(pMotor) |= FPGA_CR_PWM_EN;
	
	// clear PWM VALUES
	FPGA_PWM_DUTY(pMotor) &= FPGA_PWM_PERIOD_MASK; 
	FPGA_PWM_DUTY(pMotor) &= ~FPGA_PWM_PERIOD_MASK;
		
	return pMotor;
}


SEM_ID* update_sem_ptr;

static void motorISR(struct psrMotor *pMotor)
{
	static UINT32 last_A = 0, last_B = 0;
	UINT32 curr_A, curr_B;
	GPIO_INT_STAT(pMotor) = pMotor->gpioIrqBit;
	
	
	curr_A = (FPGA_SR(pMotor) >> 8) & 0x1;
	curr_B = (FPGA_SR(pMotor) >> 9) & 0x1;
	
	if ((curr_A == 1 && last_A == 1 && curr_B == 0 && last_B == 1) ||
		(curr_A == 0 && last_A == 0 && curr_B == 1 && last_B == 0) ||
		(curr_B == 1 && last_B == 1 && curr_A == 1 && last_A == 0) ||
		(curr_B == 0 && last_B == 0 && curr_A == 0 && last_A == 1)) {
		
		steps++;
	}
	
	if ((curr_A == 1 && last_A == 1 && curr_B == 1 && last_B == 0) ||
		(curr_A == 0 && last_A == 0 && curr_B == 0 && last_B == 1) ||
		(curr_B == 1 && last_B == 1 && curr_A == 0 && last_A == 1) ||
		(curr_B == 0 && last_B == 0 && curr_A == 1 && last_A == 0)) {
		
		steps--;
	}
	
	last_A = curr_A;
	last_B = curr_B;
	
	semGive(*update_sem_ptr);
}

// void motorTask(int* pos, int *end_tasks)


void motorRegulator(int target_steps)
{
	UINT32 pwm_val;
	
	if (target_steps == steps) {
		FPGA_PWM_DUTY(motor) = 0;
		return;
	}
	
	int speed;
	// set direction
	
	pwm_val = (target_steps > steps) ? 0x80000000 : 0x40000000;
	
		
	speed = 10*abs(target_steps - steps)+50;
	pwm_val += speed;
	
	if (speed > MOTOR_PWM_PERIOD - 1) speed = MOTOR_PWM_PERIOD - 1; 
	
	
	FPGA_PWM_DUTY(motor) = pwm_val;
}

void motorTask(SEM_ID *update_sem, int **target_step, int *end_tasks, int is_slave)
{
	motor = motorInit();
	update_sem_ptr = update_sem;
	printf("Starting motor task - %s\n", is_slave ? "slave" : "master");
	
	
	if (is_slave) {
		while(!(*end_tasks)) {
			semTake(*update_sem, WAIT_FOREVER);
			
			motorRegulator(**target_step);
			printf("target: %3d, current: %3d  \n", **target_step, steps);
		}
	}
	else {
		*target_step = &steps; // do nothing, ISR updates value
	}
}
