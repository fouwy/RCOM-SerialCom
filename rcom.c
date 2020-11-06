#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "rcom.h"

volatile int timeout;
volatile int recACK;
volatile int recUA;
volatile int STOP;
volatile int discFlag;
volatile int REJ_FLAG;
volatile int Ns;

int sendData(int fd, char *buffer, int length)
{
	int res;
	char buf[length * 2 + 6];
	char *stuffedBuffer;

	length = length * 2 + 6; //x2 para o stuffed, e +6 para header

	//initialize buf
	for (int i = 0; i < length; i++)
		buf[i] = (char)0;

	stuffedBuffer = (char *)calloc(length - 6, sizeof(char));
	stuffedBuffer = byteStuffer(buffer, length - 6);

	buf[0] = FLAG_RCV;
	buf[1] = A_TRS;
	buf[2] = ctrField();
	buf[3] = buf[1] ^ buf[2];

	for (int i = 4; i < length - 2; i++)
	{
		buf[i] = stuffedBuffer[i - 4];
	}

	buf[length - 2] = getBcc(buffer, length);
	buf[length - 1] = FLAG_RCV;

	// for ( int i = 0; i<length; i++)
	// 	printf("%d:Buffer: 0x%02hhX	 %c\n", i, buf[i], buf[i]);

	res = write(fd, buf, length);

	return res;
}

char ctrField()
{
	char control = 0x02;

	return control;
}

char getBcc(char *block, int length)
{
	unsigned char bcc = 0;

	for (int i = 0; i < length; i++)
		bcc ^= block[i];

	return bcc;
}

char *byteStuffer(char *buf, int length)
{
	char *stuffedBuf;
	int j = 0;

	stuffedBuf = (char *)calloc(length, sizeof(char));

	for (int i = 0; i < length / 2; i++)
	{

		if (buf[i] == FLAG_RCV)
		{
			stuffedBuf[j] = ESC;
			stuffedBuf[++j] = 0x5E;
		}
		else if (buf[i] == ESC)
		{
			stuffedBuf[j] = ESC;
			stuffedBuf[++j] = 0x5D;
		}
		else
			stuffedBuf[j] = buf[i];
		j++;
	}

	return stuffedBuf;
}

char *byteDestuffer(char *buf, int length)
{
	char *destuffed;
	int j = 0;
	destuffed = (char *)calloc(length, sizeof(char));

	for (int i = 0; i < length; i++)
	{

		if (buf[i] == ESC)
		{
			j++;
		}
		else
		{
			if (buf[i] == 0x5D)
				buf[i] = ESC;
			else if (buf[i] == 0x5E)
				buf[i] = FLAG_RCV;

			destuffed[i - j] = buf[i];
		}
	}

	return destuffed;
}

int sendSET(int fd)
{
	int res;
	char buf[5];

	buf[0] = 0x7E;			  //FLAG
	buf[1] = 0x03;			  //A--Tx->Rx
	buf[2] = 0x03;			  //C--SET
	buf[3] = buf[1] ^ buf[2]; //Bcc--XOR
	buf[4] = 0x7E;			  //FLAG

	res = write(fd, buf, 5);

	return res;
}

char currR(int positiveACK)
{
	if (positiveACK)
	{
		if (Ns == 0)
			return RR_1;
		else
			return RR_0;
	}
	else
	{
		if (Ns == 0)
			return REJ_1;
		else
			return REJ_0;
	}
}

char currS()
{
	if (Ns == 1)
		return 0x02;
	else
		return 0x00;
}

int receiveACK(int fd, char *rbuf)
{
	char character;
	int pos = 0;
	int res = 0;
	char buffer[3] = {};

	states state = START;
	STOP = FALSE;
	recACK = FALSE;

	while (STOP == FALSE)
	{ /* loop for input */

		if (timeout == TRUE)
			return 0;

		res = read(fd, &character, 1);
		if (res < 0)
			printf("Error reading\n");

		buffer[pos] = character;

		switch (state)
		{
		case START:
			if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
				state = START;
			break;

		case FLAG:
			if (buffer[pos] == A_RCV)
			{
				pos++;
				state = A;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case A:
			printf("A\n");
			if (buffer[pos] == DISC)
			{ //Disconnect
				discFlag = TRUE;
				pos++;
				state = C;
			}
			else if (buffer[pos] == currR(TRUE))		//mudar depois para currR(true)
			{ //positive ACK
				pos++;
				state = C;
			}
			else if (buffer[pos] == currR(FALSE))		
			{ //negative ACK
				REJ_FLAG = TRUE;
				pos++;
				state = C;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case C:
			printf("C\n");
			if (buffer[pos] == A_RCV ^ buffer[pos - 1])
			{ //MUDAR->C_RCV
				pos++;
				state = BCC1;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case BCC1:
			printf("Bcc\n");
			if (buffer[pos] == FLAG_RCV)
			{
				state = S_STOP;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case S_STOP:
			printf("STOP\n");
			STOP = TRUE;
			printf("ACK received\n");
			break;
		}
	}

	for (int i = 0; i < 3; i++)
		rbuf[i] = buffer[i];

	recACK = TRUE;

	return REJ_FLAG;
}

void receiveUA(int fd, char *rbuf)
{

	char character;
	int pos = 0;
	int res = 0;
	char buffer[3] = {};
	states state = START;
	STOP = FALSE;

	while (STOP == FALSE)
	{ /* loop for input */

		if (timeout == TRUE)
			return;

		res = read(fd, &character, 1);
		if (res < 0)
			printf("Error reading\n");

		buffer[pos] = character;

		switch (state)
		{
		case START:
			//printf("START 0x%02x\n", (unsigned char) buffer[pos]);
			if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
				state = START;
			break;

		case FLAG:
			//printf("FLAG\n");
			if (buffer[pos] == A_RCV)
			{
				pos++;
				state = A;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case A:
			//printf("A\n");
			if (buffer[pos] == C_UA)
			{
				pos++;
				state = C;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case C:
			//printf("C\n");
			if (buffer[pos] == A_RCV ^ C_UA)
			{
				pos++;
				state = BCC1;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case BCC1:
			//printf("Bcc\n");
			if (buffer[pos] == FLAG_RCV)
			{
				state = S_STOP;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case S_STOP:
			//printf("STOP\n");
			STOP = TRUE;
			printf("UA received\n");
			break;
		}
	}

	for (int i = 0; i < 3; i++)
		rbuf[i] = buffer[i];

	recUA = TRUE;
}

int sendACK(int fd, char control)
{	
	int res;
	char buf[5];

	buf[0] = FLAG_RCV;
	buf[1] = A_RCV;			  //A--Rx->Tx
	buf[2] = control;		  //C--ACK
	buf[3] = buf[1] ^ buf[2]; //Bcc
	buf[4] = FLAG_RCV;

	res = write(fd, buf, 5);
	return res;
}

int sendUA(int fd)
{
	int res;
	char buf[5];

	buf[0] = FLAG_RCV;
	buf[1] = 0x01;			  //A--Rx->Tx
	buf[2] = 0x07;			  //C--UA
	buf[3] = buf[1] ^ buf[2]; //Bcc
	buf[4] = FLAG_RCV;

	res = write(fd, buf, 5);

	return res;
}

void receiveSET(int fd, char *rbuf)
{
	char character;
	int pos = 0;
	int res = 0;
	char buffer[3] = {};
	states state = START;
	STOP = FALSE;

	while (STOP == FALSE)
	{ /* loop for input */

		res = read(fd, &character, 1);
		if (res < 0)
			printf("Error reading\n");

		buffer[pos] = character;

		switch (state)
		{
		case START:
			if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
				state = START;
			break;

		case FLAG:
			if (buffer[pos] == A_TRS)
			{
				pos++;
				state = A;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case A:
			if (buffer[pos] == C_SET)
			{
				pos++;
				state = C;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case C:
			if (buffer[pos] == A_TRS ^ C_SET)
			{
				pos++;
				state = BCC1;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case BCC1:
			if (buffer[pos] == FLAG_RCV)
			{
				state = S_STOP;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case S_STOP:
			STOP = TRUE;
			printf("SET received\n");
			break;
		}
	}

	for (int i = 0; i < 3; i++)
		rbuf[i] = buffer[i];
}

int receiveData(int fd, char *rbuf)
{
	char character;
	int pos = 0, dataPos = 0;
	int res = 0;
	char buffer[18] = {};
	char *data; //change to MAX_DATA_SIZE

	printf("in receiveData\n");

	data = (char *)calloc(12, sizeof(char));
	states state = START;
	REJ_FLAG = FALSE;
	STOP = FALSE;

	while (STOP == FALSE)
	{ /* loop for input */

		res = read(fd, &character, 1);
		if (res < 0)
			printf("Error reading\n");

		buffer[pos] = character;
		//printf("%02Xh\n", buffer[pos]);

		switch (state)
		{
		case START:
			if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
				state = START;
			break;

		case FLAG:
			//printf("flag ");
			if (buffer[pos] == A_TRS)
			{
				pos++;
				state = A;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case A:
			printf("A \n");
			if (buffer[pos] == currS())
			{ //mudar para currS()
				pos++;
				state = C;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case C:
			printf("C \n");
			if (buffer[pos] == A_TRS ^ buffer[pos - 1])
			{
				pos++;
				state = BCC1;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				printf("Bcc1 ERROR\n");
				REJ_FLAG = TRUE;
				STOP = TRUE;
			}
			break;

		case BCC1:
			printf("Bcc1 \n");
			if (dataPos < 12)
			{ //MAX_DATA_SIZE
				data[dataPos] = buffer[pos];
				dataPos++;
				pos++;
			}
			else
			{
				dataPos = 0;
				data = byteDestuffer(data, 12);
				if (buffer[pos] = getBcc(data, 12))
				{ //MAX_DATA_SIZE
					pos++;
					state = BCC2;
				}
				else
				{
					printf("BCC2 Error\n");
					REJ_FLAG = TRUE;
					STOP = TRUE;
				}
			}
			break;

		case BCC2:
			printf("Bcc2 \n");
			if (buffer[pos] == FLAG_RCV)
			{
				state = S_STOP;
			}
			else
			{
				pos = 0;
				state = START;
			}
			break;

		case S_STOP:
			printf("stop \n");
			STOP = TRUE;
			printf("Data received\n");
			break;
		}
		fflush(stdout);
	}

	for (int i = 0; i < 12; i++)
		rbuf[i] = data[i];

	return REJ_FLAG;
}