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

int main(int argc, char **argv)
{
    int fd;
    linkLayer link;

    //setup linkLayer parameters
    link.baudRate = B38400;
    link.numTries = 5;
    link.role = (int)strtol(argv[2], (char**)NULL, 10);   //0-Tran, 1-Rec
    strcpy(link.serialPort, argv[1]);
    link.timeOut = 3;


    fd = llopen(link);


    return 0;
}