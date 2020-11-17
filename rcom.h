// Funcões para o LAB1 de RCOM
// Pedro Martins

#ifndef RCOM_H_
#define RCOM_H_

#define FALSE 0
#define TRUE 1
#define FLAG_RCV 0x7E
#define A_RCV 0x01 //A--Rx->Tx
#define A_TRS 0x03 //A--Tx->Rx
#define C_UA 0x07  //C--UA
#define C_SET 0x03
#define DISC 0x0B  //disconnect C flag
#define RR_0 0x01  //receiver ready, N = 0
#define RR_1 0x21  //receiver ready, N = 1
#define REJ_0 0x05 //reject, N = 0
#define REJ_1 0x25 //reject, N = 1
#define ESC 0x7D   //ESCAPE character
#define MAX_DATA_SIZE 1000

extern volatile int timeout;
extern volatile int recACK;
extern volatile int recUA;
extern volatile int STOP;
extern volatile int discFlag;
extern volatile int REJ_FLAG;
extern volatile int Ns;
extern volatile int Nr;

typedef enum
{
    START,
    FLAG,
    A,
    C,
    BCC1,
    BCC2,
    DATA,
    S_STOP
} states;

//COMMON
char getBcc(char *block, int length);

int sendDISC(int fd, int role);

int receiveDISC(int fd, char *rbuf, int role);

int sendUA(int fd, int role);

void receiveUA(int fd, char *rbuf, int role);

//TRANSMITTER
int sendSET(int fd);

int sendData(int fd, char *buffer, int length);

void sig_handler();

char ctrField();

void byteStuffer(char *buf, int length);

int receiveACK(int fd, char *rbuf); //returns 0 if posACK, returns 1 if negACK


char currR_TRANS(int positiveACK);

//RECEIVER
int sendACK(int fd, char control);

char currR(int positiveACK);

void receiveSET(int fd, char *rbuf);

int receiveData(int fd, char *rbuf);

void byteDestuffer(char *buf, int length);

char currS();
#endif