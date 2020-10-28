#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dataLayer.h"
#include "macros.h"

int llopen(char *port, int appStatus)
{
    struct termios oldtio, newtio;

    app.fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (app.fd < 0) {
        perror(port);
        return -1;
    }

    if (tcgetattr(app.fd, &oldtio) == -1) {
        perror("tcgetattr");
        return -1;
    }


    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 30; // time to time-out in deciseconds
    newtio.c_cc[VMIN] = 0;  // min number of chars to read

    if (tcsetattr(app.fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        return -1;
    }

    // ---

    switch (appStatus) {
        case TRANSMITTER:;
            frame_t setFrame;
            if (buildSETFrame(&setFrame, true)) return -1;
            if (sendMessage(setFrame)) return -1;
            destroyFrame(&setFrame);

            frame_t responseFrame;
            if (prepareToReceive(&responseFrame, 5)) return -1;
            if (receiveNotIMessage(&responseFrame)) return -1;
            destroyFrame(&responseFrame);

            printf("Done, Transmitter Ready\n");
            break;
        case RECEIVER:;
            frame_t receiverFrame;
            if (prepareToReceive(&receiverFrame, 5)) return -1;
            if (receiveNotIMessage(&receiverFrame)) return -1;
            destroyFrame(&receiverFrame);

            frame_t uaFrame;
            if (buildUAFrame(&uaFrame, true)) return -1;
            if (sendMessage(uaFrame)) return -1;
            destroyFrame(&uaFrame);
            
            printf("Done, Receiver Ready\n");
            break;
    }
    return app.fd;
}

int llclose(int fd) {
    switch (app.status) {
        case TRANSMITTER:;
    }
}

// ---

int receiveNotIMessage(frame_t *frame)
{
    u_int8_t c;
    receive_state_t state = INIT;

    do {
        if (read(app.fd, &c, 1) == -1) {
            perror("read timeout");
            return 1;
        }
        printf("Byte read: %x    -    State: %d\n", c, state);
        switch (state)
        {
        case INIT:
            if (c == FLAG) {
                state = RCV_FLAG;
                frame->bytes[0] = c;
            }
            break;
        case RCV_FLAG:
            if (c == TRANSMITTER_TO_RECEIVER || c == RECEIVER_TO_TRANSMITTER) {
                state = RCV_A;
                frame->bytes[1] = c;
            }
            else
                state = INIT;
            break;
        case RCV_A:
            if (c == SET || c == UA || c == DISC || c == RR || c == REJ) {
                state = RCV_C;
                frame->bytes[2] = c;
            }
            else if (c == FLAG)
                state = RCV_FLAG;
            else
                state = INIT;
            break;
        case RCV_C:
            if (bccVerifier(frame->bytes, 1, 2, c)) {
                state = RCV_BCC;
                frame->bytes[3] = c;
            }
            else if (c == FLAG)
                state = RCV_FLAG;
            else {
                perror("BCC1 not correct\n");
                return 2;
            }
            break;
        case RCV_BCC:
            if (c == FLAG) {
                state = COMPLETE;
                frame->bytes[4] = c;
            }
            else
                state = INIT;
            break;
        case COMPLETE:
            break;
        }
        sleep(1);
    } while (state != COMPLETE);
    
    return 0;
}

int sendMessage(frame_t frame) {
    int attempts = 0;
    int sentBytes = 0;

    while (sentBytes != frame.size) {
        if (attempts >= MAX_WRITE_ATTEMPTS) {
            perror("Too many failed attempts to send. Time out!\n");
            return 1;
        }
        sentBytes += write(app.fd, frame.bytes, frame.size);
        printf("%d bytes sent\n", sentBytes);
        attempts++;
    }
    return 0;
}

// ---

u_int8_t bccCalculator(u_int8_t bytes[], int start, size_t length)
{
    int onesCounter = 0;
    for (int i = start; i < start + length; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            onesCounter += getBit(bytes[i], j);
        }
    }
    return onesCounter % 2;
}

bool bccVerifier(u_int8_t bytes[], int start, size_t length, u_int8_t parity)
{
    if (bccCalculator(bytes, start, length) == parity)
        return true;
    return false;
}

int buildSETFrame(frame_t *frame, bool transmitterToReceiver)
{
    if ((frame->bytes = malloc(5)) == NULL) return 1;
    frame->size = 5;
    frame->bytes[0] = FLAG;
    if (transmitterToReceiver)
        frame->bytes[1] = TRANSMITTER_TO_RECEIVER;
    else
        frame->bytes[1] = RECEIVER_TO_TRANSMITTER;
    frame->bytes[2] = SET;
    frame->bytes[3] = 0;    // BCC
    frame->bytes[4] = FLAG;

    return 0;
}

int buildUAFrame(frame_t *frame, bool transmitterToReceiver)
{
    if ((frame->bytes = malloc(5)) == NULL) return 1;
    frame->size = 5;
    frame->bytes[0] = FLAG;
    if (transmitterToReceiver)
        frame->bytes[1] = TRANSMITTER_TO_RECEIVER;
    else
        frame->bytes[1] = RECEIVER_TO_TRANSMITTER;
    frame->bytes[2] = UA;
    frame->bytes[3] = 1;    // BCC
    frame->bytes[4] = FLAG;

    return 0;
}

int buildDISCFrame(frame_t *frame, bool transmitterToReceiver)
{
    if ((frame->bytes = malloc(5)) == NULL) return 1;
    frame->size = 5;
    frame->bytes[0] = FLAG;
    if (transmitterToReceiver)
        frame->bytes[1] = TRANSMITTER_TO_RECEIVER;
    else
        frame->bytes[1] = RECEIVER_TO_TRANSMITTER;
    frame->bytes[2] = DISC;
    frame->bytes[3] = 1;    // BCC
    frame->bytes[4] = FLAG;

    return 0;
}


void destroyFrame(frame_t *frame)
{
    free(frame->bytes);
}

int prepareToReceive(frame_t *frame, size_t size)
{
    frame->size = size;
    return (frame->bytes = malloc(frame->size)) == NULL;
}
