#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "linklayer.h"
#include "rcom.h"

int fd;
linkLayer ll;
struct termios oldtio, newtio;

void sig_handler()
{
	// printf("sig\n");
	if (recUA == FALSE)
	{
		timeout = TRUE;
	}

	else if (recACK == FALSE)
	{
		timeout = TRUE;
	}
}


int llopen(linkLayer connectionParameters) {
    int c, res;
    int retranCount;

    ll = connectionParameters;

    if	((strcmp("/dev/ttyS10", ll.serialPort) != 0) &&
		 (strcmp("/dev/ttyS11", ll.serialPort) != 0))
	{
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		return -1;
	}

    //initialize flags
	timeout = FALSE;
	recACK = FALSE;
	recUA = FALSE;
	STOP = FALSE;
	discFlag = FALSE;
	REJ_FLAG = FALSE;
	Ns = 0;		//for transmitter
	Nr = 1;		//for receiver

    fd = open(ll.serialPort, O_RDWR | O_NOCTTY);

	if (fd < 0)
	{
		perror(ll.serialPort);
		exit(-1);
	}
    if (tcgetattr(fd, &oldtio) == -1)
	{ /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}
    bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = ll.baudRate | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;
	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;
	newtio.c_cc[VTIME] = 0; /* 1 second timeout t=0.1*TIME s*/
	newtio.c_cc[VMIN] = 0;

    tcflush(fd, TCIOFLUSH);
	if (tcsetattr(fd, TCSANOW, &newtio) == -1)
	{
		perror("tcsetattr");
		exit(-1);
	}
    printf("New termios structure set\n");

    //TRANSMITTER
    if (ll.role == TRANSMITTER) 
    {   
        char trash[5] = {};  //buffer for trash
        printf("TRANSMITTER\n");
        (void)signal(SIGALRM, sig_handler);

        res = sendSET(fd);
        printf("%d bytes written\n", res);

        while ((recUA == FALSE) && (retranCount < ll.numTries))
        {    
            if (timeout == FALSE && recUA == FALSE)
            {
                alarm(ll.timeOut); //timeout in seconds
                receiveUA(fd, trash, TRANSMITTER);
            }

            if ((timeout == TRUE) && (recUA == FALSE))
            {
                res = sendSET(fd);
                retranCount++;
                printf("RESENDING...%d bytes written\n", res);
                timeout = FALSE;
            }
        }

		if (retranCount == MAX_RETRANSMISSIONS_DEFAULT) {
			printf("Did not respond after %d tries\n", MAX_RETRANSMISSIONS_DEFAULT);
			return -1;
		}

		alarm(0); //cancel alarm
    }


    //RECEIVER
    else if (ll.role == RECEIVER)
    {	
        printf("RECEIVER\n");
        char trash[5];     //buffer for trash
        receiveSET(fd, trash);
            
        res = sendUA(fd, RECEIVER);
    }

    return fd;
}

int llwrite(char* buf, int bufSize) {
	int res, i, retranCount = 0;
	char supervBuf[5];	  //Supervision Frames Buffer
	
	REJ_FLAG = FALSE;
	recACK = FALSE;
	timeout = FALSE;

	if (bufSize > MAX_PAYLOAD_SIZE) {
		printf("Payload Size Greater than Max Allowed");
		return -1;
	}

	// (void)signal(SIGALRM, sig_handler);

	res = sendData(fd, buf, bufSize);
	// printf("%d bytes written\n", res);

	while ((recACK == FALSE) && (retranCount < ll.numTries))
	{
		
		if ((timeout == FALSE) && (REJ_FLAG == FALSE))
		{
			alarm(ll.timeOut);		//timeout in seconds
			REJ_FLAG = receiveACK(fd, supervBuf);
	
		}

		printf("Timeout= %d, REJ_FLAG = %d\n", timeout, REJ_FLAG);

		if ((timeout == TRUE) || (REJ_FLAG == TRUE))
		{	
			res = sendData(fd, buf, bufSize);
			retranCount++;
			if (REJ_FLAG) {
				printf("(REJECTED)RESENDING...%d bytes written\n", res);
				REJ_FLAG = FALSE;
				recACK = FALSE;
			}
			else
				printf("(TIME-OUT)RESENDING...%d bytes written\n", res);
			timeout = FALSE;
		}
	}

	printf("retransmittions: %d\n", retranCount);
	printf("Ns: %d\n", Ns);

	if (Ns == 1)
		Ns = 0;
	else
		Ns = 1;
	
	alarm(0);		//cancel alarm
	return res;
}

int llread(char* packet) {
	int bytes_read;
	char control;
	REJ_FLAG = FALSE;

	bytes_read = receiveData(fd, packet);
	printf("here\n");
	control = currR(!REJ_FLAG);
	sendACK(fd, control);
	
	printf("Nr: %d\n", Nr);

	if (REJ_FLAG == FALSE) {
		if (Nr == 1)
			Nr = 0;
		else
			Nr = 1;
	}
	
	return bytes_read;
}

int llclose(int showStatistics) {
	char trash[5];
	recACK = FALSE;

	if (ll.role == TRANSMITTER) {
		int retranCount = 0, res = 0;
		(void)signal(SIGALRM, sig_handler);
		sendDISC(fd, TRANSMITTER);

		while ((recACK == FALSE) && (retranCount < ll.numTries))
        {    
            if (timeout == FALSE && recACK == FALSE)
            {
                alarm(ll.timeOut); //timeout in seconds
                receiveDISC(fd,trash, TRANSMITTER);
            }

            if ((timeout == TRUE) && (recACK == FALSE))
            {
                res = sendDISC(fd, TRANSMITTER);
                retranCount++;
                printf("RESENDING...%d bytes written\n", res);
                timeout = FALSE;
            }
        }

		if (retranCount == MAX_RETRANSMISSIONS_DEFAULT) {
			printf("Did not respond after %d tries\n", MAX_RETRANSMISSIONS_DEFAULT);
			return -1;
		}
		
		sendUA(fd, TRANSMITTER);
	}
	else if (ll.role == RECEIVER) {
		receiveDISC(fd, trash, RECEIVER);
		sendDISC(fd, RECEIVER);
		receiveUA(fd, trash, RECEIVER);
	}

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
	{
		perror("tcsetattr");
		exit(-1);
	}

	return 1;
}