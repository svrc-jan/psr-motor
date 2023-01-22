# Real-Time systems programming - Motor Control
This is the semetral project for [Real-Time systems programming course](https://rtime.felk.cvut.cz/psr/cviceni/semestralka/) on FEE CTU.

## Assignement
The goal of the semestral work is to create a digital motor controller. This program controls the position of the motor according to the set-point given by the position of another motor, moved by hand (steer-by-wire). The set-point will be transferred between the two motor controllers using UDP messages.

## Compilation and usage

The program is created for the embedded system [Zynq-7000](https://www.xilinx.com/support/documentation/user_guides/ug585-Zynq-7000-TRM.pdf) and provided electrical motor, compiled and loaded onto the board using **Wind River Workbench IDE**. The program is downloadable kernel module, started using entry point `start`, with parameters `ip_address` and `port`. If the progrems is started with `ip_address` as `NULL` or `""`, it will start in the **slave** mode, controlling the motor with incomming information from the UDP on specified port, otherwise it will start in the **master** mode, sending information about the motor via UDP to the specified `ip_address` and `port`.

![connection](https://rtime.felk.cvut.cz/psr/cviceni/semestralka/connection.png)

## Solution description

The program runs in two modes 
