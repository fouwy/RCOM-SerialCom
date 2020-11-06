/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "rcom.h"

#define BAUDRATE B38400
#define MODEMDEVICE "/dev/ttyS1"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
#define FALSE 0
#define TRUE 1

#define max_retran 5 //Max number of retransmitions
#define MAX_DATA_SIZE 255

int main(int argc, char **argv)
{
	int fd, c, res;
	struct termios oldtio, newtio;
	char buf[MAX_DATA_SIZE];
	char testBuf[6] = {}; //delete later
	char supervBuf[5];	  //Supervision Frames Buffer
	int i, sum = 0, speed = 0;
	int retran_count = 0;

	if ((argc < 2) ||
		((strcmp("/dev/ttyS1", argv[1]) != 0) &&
		 (strcmp("/dev/ttyS2", argv[1]) != 0)))
	{
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
	}

	//initialize flags
	timeout = FALSE;
	recACK = FALSE;
	recUA = FALSE;
	STOP = FALSE;
	discFlag = FALSE;
	REJ_FLAG = FALSE;
	Ns = 1;

	/*
    Open serial port device for reading and writing and not as controlling tty
    because we don't want to get killed if linenoise sends CTRL-C.
  */

	fd = open(argv[1], O_RDWR | O_NOCTTY);
	if (fd < 0)
	{
		perror(argv[1]);
		exit(-1);
	}

	if (tcgetattr(fd, &oldtio) == -1)
	{ /* save current port settings */
		perror("tcgetattr");
		exit(-1);
	}

	bzero(&newtio, sizeof(newtio));
	newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;

	newtio.c_cc[VTIME] = 0; /* 1 second timeout t=0.1*TIME s*/
	newtio.c_cc[VMIN] = 0;	/* read will be satisfied if a single 
									is read*/

	/* 
    VTIME e VMIN devem ser alterados de forma a proteger com um temporizador a 
    leitura do(s) pr�ximo(s) caracter(es)
  */
	tcflush(fd, TCIOFLUSH);

	if (tcsetattr(fd, TCSANOW, &newtio) == -1)
	{
		perror("tcsetattr");
		exit(-1);
	}
	printf("New termios structure set\n");

	//_______START OF CODE_______
	//Teste de byte Stuffer->Por a TRUE para testar
	if (FALSE)
	{
		printf("Byte Stuffer Test\n");

		char *testBuf = (char *)calloc(6, sizeof(char));

		testBuf[0] = FLAG_RCV;
		testBuf[1] = 0x7d;
		testBuf[2] = FLAG_RCV;

		printf("size of buffer: %d\n", strlen(testBuf) * 2);

		testBuf = byteStuffer(testBuf, 6);

		for (int l = 0; l < 6; l++)
		{
			printf("%02Xh\n", testBuf[l]);
		}
		return 0;
	}

	(void)signal(SIGALRM, sig_handler);

	//----------ESTABLISHMENT----------
	res = sendSET(fd);
	printf("%d bytes written\n", res);

	while ((recUA == FALSE) && (retran_count < max_retran))
	{

		if (timeout == FALSE && recUA == FALSE)
		{
			alarm(3); //3 second time-out
			receiveUA(fd, supervBuf);
		}

		if ((timeout == TRUE) && (recUA == FALSE))
		{
			res = sendSET(fd);
			retran_count++;
			printf("RESENDING...%d bytes written\n", res);
			timeout = FALSE;
		}
	}

	if (retran_count == max_retran)
		printf("Did not respond after %d tries\n", max_retran);

	for (i = 0; i < 3; i++)
		printf("Message received: 0x%02x\n", (unsigned char)supervBuf[i]);

	// //----------------------DATA TRANSMITION-------------------------
	if (recUA)
	{
		retran_count = 0;

		//testBuf
		testBuf[0] = FLAG_RCV;
		testBuf[1] = 0x7d;
		testBuf[2] = FLAG_RCV;
		testBuf[3] = 'a';
		testBuf[4] = 'b';
		testBuf[5] = 'c';
		res = sendData(fd, testBuf, 6); //change to buf later, and MAX_DATA_SIZE
		printf("%d bytes written\n", res);

		receiveACK(fd, supervBuf);

		while ((recACK == FALSE) && (retran_count < max_retran))
		{

			if (timeout == FALSE && recACK == FALSE)
			{
				alarm(3); //3 second time-out
				REJ_FLAG = receiveACK(fd, supervBuf);
			}

			if (((timeout == TRUE) && (recACK == FALSE)) || REJ_FLAG)
			{
				res = sendData(fd, testBuf, 6); //change to buf later, and MAX_DATA_SIZE
				retran_count++;
				if (REJ_FLAG)
					printf("(REJECTED)RESENDING...%d bytes written\n", res);
				else
					printf("(TIME-OUT)RESENDING...%d bytes written\n", res);
				timeout = FALSE;
			}
		}

		for (i = 0; i < 3; i++)
			printf("Message received: 0x%02x\n", (unsigned char)supervBuf[i]);

		printf("end of transmition\n");
	}

	//----------TERMINATOR----------
	//falta implementar

	if (tcsetattr(fd, TCSANOW, &oldtio) == -1)
	{
		perror("tcsetattr");
		exit(-1);
	}

	close(fd);
	return 0;
}

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
