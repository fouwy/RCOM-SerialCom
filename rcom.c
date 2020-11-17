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
volatile int Nr;

int sendData(int fd, char *buffer, int length)
{
	int res;
	char buf[length * 2 + 6];
	char *stuffedBuffer;
	int lengthStuffed = length * 2 + 6; //x2 para o stuffed, e +6 para header

	//initialize buf
	for (int i = 0; i < lengthStuffed; i++)
		buf[i] = (char)0;

	stuffedBuffer = (char *)calloc(lengthStuffed - 6, sizeof(char));
	
	for (int i=0; i<(length); i++) {
		stuffedBuffer[i] = buffer[i];
	}
	byteStuffer(stuffedBuffer, lengthStuffed-6);	//isto nao funciona

	buf[0] = FLAG_RCV;
	buf[1] = A_TRS;
	buf[2] = ctrField();
	buf[3] = buf[1] ^ buf[2];

	for (int i = 4; i < lengthStuffed - 2; i++)
	{
		buf[i] = stuffedBuffer[i - 4];
	}

	buf[lengthStuffed - 2] = getBcc(buffer, length);
	buf[lengthStuffed - 1] = FLAG_RCV;

	res = write(fd, buf, lengthStuffed);

	free(stuffedBuffer);
	return res;
}

char ctrField()
{	
	char control = 0x02;

	if (Ns == 1)
		control = 0x02;
	else
		control = 0x00;
		
	return control;
}

char currR(int positiveACK)
{
	if (positiveACK)
	{
		if (Nr == 0)
			return RR_1;
		else
			return RR_0;
	}
	else
	{
		if (Nr == 0)
			return REJ_1;
		else
			return REJ_0;
	}
}

char currR_TRANS(int positiveACK)
{
	if (positiveACK)
	{
		if (Ns == 1)
			return RR_1;
		else
			return RR_0;
	}
	else
	{
		if (Ns == 1)
			return REJ_1;
		else
			return REJ_0;
	}
}

char currS()
{
	if (Nr == 0)
		return 0x02;
	else
		return 0x00;
}

char getBcc(char *block, int length)
{
	unsigned char bcc = block[0];

	for (int i = 1; i < length; i++)
		bcc ^= block[i];

	return bcc;
}

void byteStuffer(char *buf, int length)
{
	char stuffedBuf[length];
	int offset = 0, numFlags = 0;

	//initialize array
	for (int i=0; i<length; i++)
		stuffedBuf[i] = 0;

	// for(int i=0; i<length/2;i++){
	// 	if ((buf[i] == FLAG_RCV) || (buf[i] == ESC))
	// 		numFlags++;
	// }
	
	for (int i = 0; i < length/2; i++)
	{
		if(buf[i]==FLAG_RCV || buf[i]==ESC) {
			stuffedBuf[i + offset] = ESC;
			stuffedBuf[i + offset +1] = buf[i]^0x20;
			++offset;
		}
		else {
			stuffedBuf[i + offset] = buf[i];
		}
		
	}
	// for (int i = 0; i < length/2; i++)
	// 	stuffedBuf[i] = buf[i];

	for (int i = 0; i<length; i++)
		buf[i] = stuffedBuf[i];

}

void byteDestuffer(char *buf, int length)
{
	char destuffed[length/2];
	int offset = 0, numFlags = 0;

	//initialise destuffed
	for (int i=0; i<length/2; i++)
		destuffed[i] = 0;

	for(int i = 0; i < length; i++) {
		if(buf[i] == ESC)
			numFlags++;
	}

	for (int i=0; i<(length/2 + numFlags); i++) {
		if (buf[i] == ESC) {
			destuffed[i - offset] = buf[i+1] ^ 0x20;
			++i;
			++offset;
		}
		else {
			destuffed[i - offset] = buf[i];
		}
	}

	for (int i=0; i<length/2; i++) {
		buf[i] = destuffed[i];
	}
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
	//		printf("A\n");
			if (buffer[pos] == DISC)
			{ //Disconnect
				discFlag = TRUE;
				pos++;
				state = C;
			}
			else if (buffer[pos] == currR_TRANS(TRUE))
			{ //positive ACK
				pos++;
				state = C;
			}
			else if (buffer[pos] == currR_TRANS(FALSE))		
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
	//		printf("C\n");
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
	//		printf("Bcc\n");
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
	//		printf("STOP\n");
			STOP = TRUE;
			// printf("ACK received\n");
			break;
		}
	}

	for (int i = 0; i < 3; i++)
		rbuf[i] = buffer[i];

	recACK = TRUE;

	return REJ_FLAG;
}

void receiveUA(int fd, char *rbuf, int role)
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
			//TRANSMITTER
			if (role == 0) {
				if (buffer[pos] == A_RCV)
				{	
					// printf("t_here\n");
					pos++;
					state = A;
				}
			} //RECEIVER
			else if (role == 1) {
				if (buffer[pos] == A_TRS)
				{
					// printf("rec_here\n");
					pos++;
					state = A;
				}
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
			// printf("A\n");
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
			if (buffer[pos] == (buffer[0] ^ C_UA))
			{	
				// printf("here\n");
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

int sendUA(int fd, int role)
{
	int res;
	char buf[5];

	buf[0] = FLAG_RCV;
	if (role == 0)	
		buf[1] = A_TRS;
	else			
		buf[1] = A_RCV;
	buf[2] = C_UA;			  //C--UA
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
			// printf("SET received\n");
			break;
		}
	}

	strcpy(rbuf, buffer);
	// for (int i = 0; i < 3; i++)
	// 	rbuf[i] = buffer[i];
}

int receiveData(int fd, char *rbuf)
{
	int pos = 0, dataPos = 0, res = 0, end_of_data = 0, destuff_data_size;
	char buffer[6] = {};	//to store stuffed buffer
	char character;
	char *destuffed_data;
	char data[MAX_DATA_SIZE*2];


	states state = START;
	REJ_FLAG = FALSE;
	STOP = FALSE;
	discFlag = FALSE;

	while (STOP == FALSE)
	{ /* loop for input */

		res = read(fd, &character, 1);
		if (res < 0)
			printf("Error reading\n");

		else if ((res == 0) && (state == BCC1)) {
			// printf("end_of_data\n");
			// fflush(stdout);
			end_of_data = 1;
		}

		buffer[pos] = character;
		//printf("%02Xh\n", buffer[pos]);

		switch (state)
		{
		case START:
			dataPos = 0;
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
	//		printf("A \n");
			if (buffer[pos] == currS())
			{ //mudar para currS()
				pos++;
				state = C;
			}
			else if (buffer[pos] == DISC) {
				discFlag = TRUE;
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
			if (buffer[pos] == A_TRS ^ buffer[pos - 1])
			{
				pos++;
				// if (discFlag == TRUE)
				// 	state = BCC2;
				// else
					state = BCC1;
			}
			else if (buffer[pos] == FLAG_RCV)
			{
				pos = 1;
				state = FLAG;
			}
			else
			{
				REJ_FLAG = TRUE;
				STOP = TRUE;
			}
			break;

		case BCC1:
			//adicionar if case para FLAG maybe
			//printf("BCC1 %d: %02Xh\n", pos, buffer[pos]);
			if ((dataPos < MAX_DATA_SIZE*2) && (end_of_data == 0))
			{
				data[dataPos] = buffer[pos];
				dataPos++;
			}
			else
			{	
				pos++;
				state = DATA;
			}
			break;

		case DATA:
			if (end_of_data == 1) {
				buffer[pos-1] = data[dataPos-2];
				buffer[pos] = data[dataPos-1];
				dataPos = dataPos - 2;
			}
			// printf("DataPos: %d, -> %d___ Pos: %d\n", dataPos, end_of_data, pos);
			destuff_data_size = dataPos/2;
			destuffed_data = (char *)calloc(destuff_data_size, sizeof(char));

			byteDestuffer(data, dataPos);
			for (int i = 0; i<destuff_data_size; i++) {
				destuffed_data[i] = data[i];
			}

			if (buffer[pos-1] == getBcc(destuffed_data, destuff_data_size))
			{
				pos++;
				state = BCC2;
			}
			else
			{
				printf("BCC2 Error\n");
				REJ_FLAG = TRUE;
				STOP = TRUE;
			}
			break;

		case BCC2:
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
	//		printf("stop \n");
			STOP = TRUE;
			// printf("Data received\n");
			break;
		}
	}

	if(!REJ_FLAG) {
		for (int i=0; i<destuff_data_size; i++)
			rbuf[i] = destuffed_data[i];
	}
	else {
		destuff_data_size = 0;
	}
	
	free(destuffed_data);

	return destuff_data_size;
}

int sendDISC(int fd, int role) {
	int res;
	char buf[5];

	buf[0] = 0x7E;			  //FLAG
	if (role == 0)	buf[1] = A_TRS;
	else			buf[1] = A_RCV;
	buf[2] = DISC;			  //C--DISC
	buf[3] = buf[1] ^ buf[2]; //Bcc--XOR
	buf[4] = 0x7E;			  //FLAG

	res = write(fd, buf, 5);

	return res;
}

int receiveDISC(int fd, char *rbuf, int role) {
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
			//TRANSMITTER
			if (role == 0) {
				if (buffer[pos] == A_RCV)
				{	
					// printf("t_here\n");
					pos++;
					state = A;
				}
			} //RECEIVER
			else if (role == 1) {
				if (buffer[pos] == A_TRS)
				{
					// printf("rec_here\n");
					pos++;
					state = A;
				}
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
	//		printf("A\n");
			if (buffer[pos] == DISC)
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
	//		printf("C\n");
			if (buffer[pos] == (buffer[0] ^ DISC))
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
	//		printf("Bcc\n");
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
			printf("DISC received\n");
			STOP = TRUE;
			break;
		}
	}

	for (int i = 0; i < 3; i++)
		rbuf[i] = buffer[i];

	recACK = TRUE;
	return 1;
}