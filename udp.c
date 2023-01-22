/*
 * udp.c
 *
 *  Created on: Jan 21, 2023
 *      Author: karanmus
 */

#include "udp.h"
#include "motor.h"

UDP *initudp(char *ip_address, int port)
{
    UDP *udp = NULL;
    udp = (UDP *)malloc(sizeof(udp));
    if (udp == NULL) {
    	fprintf(stderr, "Memory Allocation Error for UDP! [udp.C]\n");
    	exit(1);
   }
   if (ip_address[0] == ' '){
	   /*Receiver Mode*/
       udp->isSender = false;
       
       /* Create a UDP socket */
       udp->sockd = socket(AF_INET, SOCK_DGRAM, 0);
       if (udp->sockd == -1) {
    	   perror("Socket creation error! [udp.C]\n");
    	   exit(1);
       }
       /* Configure server address */
       udp->my_addr.sin_family = AF_INET;
       udp->my_addr.sin_addr.s_addr = INADDR_ANY;
       udp->my_addr.sin_port = htons(port);
       udp->status = bind(udp->sockd, (struct sockaddr *)&udp->my_addr,
                         sizeof(udp->my_addr));
       if (udp->status == -1){
    	   close(udp->sockd);
           printf ("bind error: %s",strerror(errno));
           };
   } else {
       /*Sender Mode*/
       udp->isSender = true;
       /* Create a udp socket */
       udp->sockd = socket(AF_INET, SOCK_DGRAM, 0);
       if (udp->sockd == -1) {
          fprintf(stderr, "ERROR: Socket creation error! [udp.c]\n");
          exit(1);
       }
       /* Configure client address */
       udp->my_addr.sin_family = AF_INET;
       udp->my_addr.sin_addr.s_addr = INADDR_ANY;
       udp->my_addr.sin_port = htons(port);

       udp->status = bind(udp->sockd, (struct sockaddr *)&udp->my_addr,
                                sizeof(udp->my_addr));
       if (udp->status == -1){
       		close(udp->sockd);
       		printf ("bind error: %s",strerror(errno));
       	};

       /* Set server address */
       udp->srv_addr.sin_family = AF_INET;
       inet_aton(ip_address, &udp->srv_addr.sin_addr);
       udp->srv_addr.sin_port = htons(port);
    }
   return udp;
}

void getDesiredPos(UDP *udp, int payload, int end)
{
	/*int *data = NULL;
	data = (int *)malloc(sizeof(int) * payload);
	if (data == NULL) {
		fprintf(stderr, "Memory Allocation Error for data to be sent [udp.C]\n");
	    exit(1);
	}*/
	while(!end){
		int desired_pos = getMotorPos();
		printf("Controller motor position is: %d\n", desired_pos);
		sendUDPData(udp, desired_pos, payload);	
	}
}

void sendUDPData(UDP *udp, int data, int payload)
{
   if (!udp->isSender)
      printf("WARNING: Only senders can send. [UDP.c]\n");
   /* Send data */
   int sent_bytes = sendto(udp->sockd, data, payload, 0, 
		   (struct sockaddr *)&udp->srv_addr, sizeof(udp->srv_addr));
   if (sent_bytes == -1){
	   printf ("send error: %s\n", strerror(errno));
   }
}


void recieveUDPData(UDP *udp, int data, int payload)
{
   if (udp->isSender)
      printf("WARNING: Only receivers can receive. [UDP.c]\n");

   udp->addrlen = sizeof(udp->srv_addr);

   int rcvd_bytes = recvfrom(udp->sockd, data, payload, 0, 
		   (struct sockaddr*)&udp->srv_addr, &udp->addrlen);
   
   if (rcvd_bytes == -1){
	   printf("receive error: %s \n", strerror(errno));
	   exit(1);
   }
}

void assertPos(UDP *udp, int payload, int end)
{
	/*Allocate an array to store received values.*/
	int *data = NULL;
	data = (int *)malloc(sizeof(int) * payload);
	while(!end){
		recieveUDPData(udp, data, payload);
		udp->desired_pos = data[0];
		printf("Recieved data position is: %d\n", udp->desired_pos);
	}
	free(data);
	data = NULL;
}


void closeUDP(UDP *udp)
{
   close(udp->sockd);
   free(udp);
   udp = NULL;
}
