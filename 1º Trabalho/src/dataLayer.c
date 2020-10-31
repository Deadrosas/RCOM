#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "dataLayer.h"
#include "dataLayerPrivate.h"
extern applicationLayer application;

extern int idFrameSent;

int llopen(char *port, int appStatus)
{
    struct termios oldtio, newtio;

    application.fd = open(port, O_RDWR | O_NOCTTY);
    if (application.fd < 0) {
        perror(port);
        return -1;
    }

    if (tcgetattr(application.fd, &oldtio) == -1) {
        perror("tcgetattr");
        return -2;
    }


    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 0; // time to time-out in deciseconds
    newtio.c_cc[VMIN] = 0;  // min number of chars to read

    if (tcsetattr(application.fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -3;
    }

    frame_t setFrame;
    frame_t responseFrame;
    frame_t receiverFrame;
    frame_t uaFrame;

    switch (appStatus) {
        case TRANSMITTER:;
            for (int i = 0;; i++) {
                if (i == MAX_FRAME_RETRANSMISSIONS) {
                    printf("Max Number of retransmissions reached. Exiting program.\n");
                    return -1;
                }

                buildSETFrame(&setFrame, true);
                if (sendNotIFrame(&setFrame)) return -5;

                prepareToReceive(&responseFrame, 5);
                int responseReceive = receiveNotIMessage(&responseFrame); 
                if (responseReceive == -1 || responseReceive == -2) continue;         // in a timeout or wrong bcc, retransmit frame
                else if (responseReceive < -2) return -7;
                if (!isUAFrame(&responseFrame)) continue;       // wrong frame received

                break;
            }
            break;
        case RECEIVER:;
            prepareToReceive(&receiverFrame, 5);
            if (receiveNotIMessage(&receiverFrame)) return -7;
            if (!isSETFrame(&receiverFrame)) return -8;

            buildUAFrame(&uaFrame, true);
            if (sendNotIFrame(&uaFrame)) return -5;
            
            break;
    }

    printf("LLOPEN DONE\n");
    return application.fd;
}

int llclose(int fd) {

    frame_t discFrame;
    frame_t receiveFrame;
    frame_t uaFrame;

    int receiveReturn;
    switch (application.status) {
        case TRANSMITTER:;
            for (int i = 0;; i++) {
                if (i == MAX_FRAME_RETRANSMISSIONS) {
                    printf("Max Number of retransmissions reached. Exiting program.\n");
                    return -1;
                }

                buildDISCFrame(&discFrame, true);
                if (sendNotIFrame(&discFrame)) return -2;

                prepareToReceive(&receiveFrame, 5);
                receiveReturn = receiveNotIMessage(&receiveFrame);
                if (receiveReturn == -1 || receiveReturn == -2) continue;        //in a timeout or wrong bcc, retransmit frame
                else if (receiveReturn < -2) return -4;
                if (!isDISCFrame(&receiveFrame)) continue;      // wrong frame received

                buildUAFrame(&uaFrame, true);
                if (sendNotIFrame(&uaFrame)) return -2;
                
                break;
            }
        break;
        case RECEIVER:;
            prepareToReceive(&receiveFrame, 5);
            if (receiveNotIMessage(&receiveFrame)) return -7;
            if (!isDISCFrame(&receiveFrame)) return -5;

            buildDISCFrame(&discFrame, true);
            if (sendNotIFrame(&discFrame)) return -2;

            prepareToReceive(&receiveFrame, 5);
            if (receiveNotIMessage(&receiveFrame)) return -4;
            if (!isUAFrame(&receiveFrame)) return -5;

        break;
    }
    if (close(fd) == -1) return -8;
    printf("LLCLOSE DONE\n");
    return 1;
}

int llread(int fd, char * buffer){
    frame_t frame;
    int receiveIMessageReturn;
    while (1) {
        receiveIMessageReturn = receiveIMessage(&frame);
        if (receiveIMessageReturn == 0 || receiveIMessageReturn == -1) break;
        else if (receiveIMessageReturn == 1) continue;
        else {
            printf("receiveIMessage returned unexpected value\n");
            return -1;
        }
        
        frame_t response;
        if (receiveIMessageReturn == 0)
            prepareResponse(&response, true, ((frame.bytes[2] >> 6) + 1) % 2);
        else if (receiveIMessageReturn == -1)
            prepareResponse(&response, false, ((frame.bytes[2] >> 6) + 1) % 2);

        sendNotIFrame(&response);
    }

    printFrame(&frame);
    
    return 0;
}

int llwrite(int fd, char * buffer, int length)
{
    frame_t **info = NULL;
    int framesToSend = prepareI(buffer, length, &info); //Prepara a trama de informação
    
    for (int i = 0; i < framesToSend; i++) {
        printFrame(info[i]);
        sendIFrame(info[i]);
    }
    idFrameSent = (idFrameSent + 1) % 2;
    return 0;
}


int clearSerialPort(char *port) {
    int auxFd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (auxFd == -1) {
        perror("error clearing serialPort");
        return 1;
    }
    char c;
    while (read(auxFd, &c, 1) != 0) printf("byte cleared: %x\n", c);
    if (close(auxFd) == -1) return 2;
    return 0;
}
