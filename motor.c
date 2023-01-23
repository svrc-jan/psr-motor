/**
 * \file motor.c
 * \author Jan Svrcina
 * 
 * Motor control driver for reading and decodeing ICR and managing PWM/
*/



#include "motor.h"


struct psrMotor *motor; //!< motor struct to handle the device
int steps = 0;          //!< current postion step counter
SEM_ID* update_sem_ptr; //!< pointer to global `update_sem`

//! \brief driver structure to attach motor
LOCAL VXB_FDT_DEV_MATCH_ENTRY psrMotorMatch[] = {
    {"cvut,psr-motor", NULL},
    {} /* Empty terminated list */
};


//! \brief allocate and select device from vxb
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


/** \brief setup for motor, map registers, initialize IRC, map ISR
 *  \param pInst vxb device instantance struct 
 *  \return status of procedure
 */
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



/**
 * \brief shut down motor, stop interrupts and zero out PWM
*/
void motorShutdown(void)
{
	FPGA_PWM_DUTY(motor) = 0;	
	GPIO_INT_DIS(motor) |= motor->gpioIrqBit;
}



/**
 * \brief probe and attach motor, setup PWM freq and enable the counter
 * set default values and reset global `steps` counter
*/
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

/**
 * \brief Interrup Service request handler
 * decode ISR and increment or decrement the `steps` counter,
 * gives signal to `update_sem`
 * \param pMotor pointer to motor structure
*/
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

/**
 * \brief calculate and set the PWM duty (max. PWM counter) based on P regulator
 * \param target_steps value the motor should reach
 * \param pwm_duty pointer to pwm_duty value for report task
*/
void motorRegulator(int target_steps, float *pwm_duty)
{
	UINT32 pwm_val;
	
	if (target_steps == steps) {
		FPGA_PWM_DUTY(motor) = 0;
		*pwm_duty = 0;
		return;
	}
	
	int speed;
	// set direction
	
	pwm_val = (target_steps > steps) ? 0x80000000 : 0x40000000;
	
		
	speed = 10*abs(target_steps - steps)+100;
	pwm_val += speed;
	
	if (speed > MOTOR_PWM_PERIOD - 1) speed = MOTOR_PWM_PERIOD - 1; 
	
	FPGA_PWM_DUTY(motor) = pwm_val;
	*pwm_duty = (target_steps > steps) ? -(float)(speed)/MOTOR_PWM_PERIOD : (float)(speed)/MOTOR_PWM_PERIOD;
}

/**
 * \brief initializes the motor driver, registeres the Interrupt Service Request (ISR) for the IRC encoder
 *   - **slave** - in loop calculates and sets the PWM width, action value of the controller, using a simple P regulator based on value in `target_steps` and current step counter `steps`, waiting for signal from `update_sem`
 *   - **master** - sets the `target_steps` variable to the adress of `steps`, 
 * for master the target is the current position of the counter, 
 * no loop is required, new information is passed by `motorISR`
 * \param update_sem pointer to global synchronazition semaphore `updade_sem`
 * \param target_step pointer to `target_step` reference
 * \param end_task pointer to `end_task` global variable {0, 1}, signals to end all loops
 * \param is_slave binary value assigning mode, 0 for **master**, 1 for **slave**
 * \param pwm_dudy pointer to `pwm_duty` global variable for report task
 */

void motorTask(SEM_ID *update_sem, int **target_step, int *end_tasks, int is_slave, float *pwm_duty)
{
	motor = motorInit();
	update_sem_ptr = update_sem;
	printf("Starting motor task - %s\n", is_slave ? "slave" : "master");
	
	
	if (is_slave) {
		while(!(*end_tasks)) {
			semTake(*update_sem, WAIT_FOREVER);
			
			motorRegulator(**target_step, pwm_duty);
		}
	}
	else {
		*target_step = &steps; // do nothing, ISR updates value
	}
}
