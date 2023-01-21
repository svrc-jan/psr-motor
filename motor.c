#include "motor.h"

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

void motorISR(struct psrMotor *pMotor);
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

void motorShutdown(struct psrMotor *pMotor)
{
	GPIO_INT_DIS(pMotor) |= pMotor->gpioIrqBit;
}

struct psrMotor *motorInit()
{
	VXB_DEV_ID motorDrv = vxbDevAcquireByName("pmod1", 0);
	if (motorDrv == NULL) {
		fprintf(stderr, "vxbDevAcquireByName failed\n");
		return NULL;
	}
	
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

struct motor_enc {
	int A;
	int B;
};
struct motor_enc last_motor_enc = {0, 0};

int steps = 0;

void motorISR(struct psrMotor *pMotor)
{
	GPIO_INT_STAT(pMotor) = pMotor->gpioIrqBit;
	struct motor_enc curr, last = last_motor_enc;
	
	curr.A = (FPGA_SR(pMotor) & FPGA_SR_IRC_A_MON) ? 1 : 0;
	curr.B = (FPGA_SR(pMotor) & FPGA_SR_IRC_B_MON) ? 1 : 0;
	
	if (curr.A == last.A && curr.B != last.B) {
		steps++;
	}
	
	if (curr.A != last.A && curr.B == last.B) {
		steps--;
	}
	
	last_motor_enc = curr;
}


void motorTask(int *end_tasks)
{
	struct psrMotor *pMotor = motorInit();
  
	steps = 0;
	while(!(*end_tasks)) {
		printf("steps: %d    \r", steps);
		taskDelay(50);
	}

	motorShutdown(pMotor);
}
