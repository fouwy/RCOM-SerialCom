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

void sig_handler()
{
	printf("sig\n");
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
    int fd, c, res;
    int retranCount;
    struct termios oldtio, newtio;
    linkLayer link = connectionParameters;

    if	((strcmp("/dev/ttyS1", link.serialPort) != 0) &&
		 (strcmp("/dev/ttyS2", link.serialPort) != 0))
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
	Ns = 1;

    fd = open(link.serialPort, O_RDWR | O_NOCTTY);

	if (fd < 0)
	{
		perror(link.serialPort);
		exit(-1);
	}
    if (tcgetattr(fd, &oldtio) == -1)
	{ /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}
    bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = link.baudRate | CS8 | CLOCAL | CREAD;
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
    if (link.role == TRANSMITTER) 
    {   
        printf("TRANSMITTER\n");
        char trash[5];  //buffer for trash
        (void)signal(SIGALRM, sig_handler);

        res = sendSET(fd);
        printf("%d bytes written\n", res);

        while ((recUA == FALSE) && (retranCount < link.numTries))
        {    
            if (timeout == FALSE && recUA == FALSE)
            {
                alarm(link.timeOut); //timeout in seconds
                receiveUA(fd, trash);
            }

            if ((timeout == TRUE) && (recUA == FALSE))
            {
                res = sendSET(fd);
                retranCount++;
                printf("RESENDING...%d bytes written\n", res);
                timeout = FALSE;
            }
        }

	if (link.numTries == MAX_RETRANSMISSIONS_DEFAULT)
		printf("Did not respond after %d tries\n", MAX_RETRANSMISSIONS_DEFAULT);

    for (int i = 0; i < 3; i++)
		printf("Message received: 0x%02x\n", (unsigned char)trash[i]);
    }


    //RECEIVER
    else if (link.role == RECEIVER)
    {
        printf("RECEIVER\n");
        char trash[5];     //buffer for trash
        receiveSET(fd, trash);

        for (int i = 0; i < 3; i++)
		    printf("Message received: 0x%02x\n", (unsigned char)trash[i]);
            
        res = sendUA(fd);
		printf("%d bytes written\n", res);
    }

    return fd;
}

int llwrite(int fd, char* buf, int bufSize) {
	int res, i, retranCount = 0;
	char supervBuf[5];	  //Supervision Frames Buffer
	linkLayer link;
	if (bufSize > MAX_PAYLOAD_SIZE) {
		printf("Payload Size Greater than Max Allowed");
		return -1;
	}

	res = sendData(fd, buf, bufSize);
	printf("%d bytes written\n", res);

	receiveACK(fd, supervBuf);

	while ((recACK == FALSE) && (retranCount < link.numTries))
	{
		
		if ((timeout == FALSE) && (REJ_FLAG == FALSE))
		{
			alarm(3); //3 second time-out
			REJ_FLAG = receiveACK(fd, supervBuf);
		}

		if ((timeout == TRUE) || REJ_FLAG)
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

	for (i = 0; i < 3; i++)
		printf("Message received: 0x%02x\n", (unsigned char)supervBuf[i]);

}