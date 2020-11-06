/*Non-Canonical Input Processing*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rcom.h"

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */

int main(int argc, char **argv)
{
	int fd, c, res;
	struct termios oldtio, newtio;
	char buf[255];
	char testBuf[18];
	char character, control;
	int i = 0, flag_reg;

	STOP = FALSE;
	if ((argc < 2) ||
		((strcmp("/dev/ttyS2", argv[1]) != 0) &&
		 (strcmp("/dev/ttyS1", argv[1]) != 0)))
	{
		printf("Usage:\tnserial SerialPort\n\tex: nserial /dev/ttyS1\n");
		exit(1);
	}

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

	//_____START OF CODE_______

	//----------ESTABLISHMENT----------
	receiveSET(fd, buf);

	for (int i = 0; i < 3; i++)
		printf("Message received: 0x%02x\n", (unsigned char)buf[i]);

	printf("Bytes received %d bytes\n", strlen(buf) + 1);

	if ((buf[0] ^ buf[1]) != buf[2])	//isto é redundante pq receiveSET nao retorna
		printf("Error in Bcc check\n"); //se houver erro no Bcc

	else
	{
		//sleep(7);
		res = sendUA(fd);
		printf("%d bytes written\n", res);
	}

	//----------DATA TRANSFER----------

	Ns = 1;
	flag_reg = receiveData(fd, testBuf);

	control = currR(!flag_reg);

	sleep(7);		//test timeout
	res = sendACK(fd, control);
	printf("%d bytes written\n", res);
	// for ( int i = 0; i<6; i++)
	// 	printf("%d:Message received: 0x%02x\n", i, testBuf[i]);

	//----------DISCONNECT------------

	tcsetattr(fd, TCSANOW, &oldtio);
	close(fd);
	return 0;
}

// STOP = FALSE;
// i = 0;
// res = 0;
// while (STOP == FALSE) {
// 	res = read(fd, &character, 1);
// 	if (res > 0) {
// 		testBuf[i] = character;
// 		i++;
// 	}
// 	if (i == 18)
// 		STOP=TRUE;
// }
