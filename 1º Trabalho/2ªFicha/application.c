#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "application.h"

void llopen(char *port, int appStatus)
{
    struct termios oldtio, newtio;

    app.fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (app.fd < 0) {
        perror(port);
        exit(-1);
    }

    if (tcgetattr(app.fd, &oldtio) == -1) {
        perror("tcgetattr");
        exit(-1);
    }


    bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;

    newtio.c_cc[VTIME] = 30; // time to time-out in deciseconds
    newtio.c_cc[VMIN] = 5;  // min number of chars to read

    if (tcsetattr(app.fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    // ---

    switch (appStatus) {
        case TRANSMITTER:;
            printf("SET\n");
            frame_t setFrame;
            buildSETFrame(&setFrame, true);
            sendMessage(setFrame);
            destroyFrame(&setFrame);

            printf("RESPONDING\n");
            frame_t responseFrame;
            prepareToReceive(&responseFrame, 5);
            receiveNotIMessage(&responseFrame);

            destroyFrame(&responseFrame);
            printf("Done, Transmitter Ready\n");
            break;
        case RECEIVER:;
            printf("RECEIVING\n");
            frame_t receiverFrame;
            prepareToReceive(&receiverFrame, 5);
            receiveNotIMessage(&receiverFrame);
            destroyFrame(&receiverFrame);

            printf("UA\n");
            frame_t uaFrame;
            buildUAFrame(&uaFrame, true);
            sendMessage(uaFrame);

            destroyFrame(&uaFrame);
            printf("Done, Receiver Ready\n");
            break;
    }
}

int llwrite(int fd, char * buffer, int length)
{
    frame_t *info = NULL, *responseFrame = NULL;
    prepareI(buffer, length, info); //Prepara a trama de informação
    
    do
    {
        sendMessage(*info);

        receiveNotIMessage(responseFrame);
    }while(responseFrame->bytes[2] != RR);

    destroyFrame(info);
    destroyFrame(responseFrame);
    
    return length;
}

void auxStuffing(frame_t * frame, int * stuffingCounter, char byte, int i)
{

    if(byte == FLAG){//do byte stuffing
        frame->bytes[4 + i + (*stuffingCounter)] = STUFFING_FLAG;
        frame->bytes[4 + i + (++(*stuffingCounter))] = FLAG;
    }
    else if(byte == STUFFING_FLAG){//do byte stuffing
        frame->bytes[4 + i + (*stuffingCounter)] = STUFFING_FLAG;
        frame->bytes[4 + i + (++(*stuffingCounter))] = STUFFING_FLAG;
    }
    else{
        frame->bytes[4 + i + (*stuffingCounter)] = byte;
    }
}

int prepareI(char* data, int size, frame_t* info) //Testar
{
    info->size = sizeof(u_int8_t) * (4 + size + 2);
    info->bytes = malloc(info->size);


    info->bytes[0] = FLAG; //F
    info->bytes[1] = TRANSMITTER_TO_RECEIVER; //A
    info->bytes[2] = 0; //C: ID da trama, suposto mudar depois
    info->bytes[3] = bccCalculator(info->bytes, 1, 2); //BCC1, calculado com A e C

    int stuffingCounter = 0;
    //Talvez colocar o tamanho da mensagem como primeiro byte?
    info->bytes[4] = data[0];

    for(unsigned int i = 1; i < size; i++) 
    {
        auxStuffing(info, &stuffingCounter, data[i], i);
    }

    int bcc2_byte = 4 + 1 + size + stuffingCounter;

    info->bytes[bcc2_byte] = bccCalculator(info->bytes, 4, size);
    info->bytes[bcc2_byte + 1] = FLAG;
    return bcc2_byte + 2;
}

void receiveNotIMessage(frame_t *frame)
{
    uint8_t c;
    receive_state_t state = INIT;

    do {
        read(app.fd, &c, 1);
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
            if (c == bccCalculator(frame->bytes, 1, 2)) {
                state = RCV_BCC;
                frame->bytes[3] = c;
            }
            else if (c == FLAG)
                state = RCV_FLAG;
            else
                state = INIT;
            break;
        case RCV_BCC:
            if (c == FLAG) {
                state = RCV_FLAG;
                frame->bytes[4] = c;
            }
            else if (c == FLAG)
                state = COMPLETE;
            else
                state = INIT;
            break;
        case COMPLETE:
            break;
        }
        sleep(1);
    } while (state != COMPLETE);

    if (!bccVerifier(frame->bytes, 1, 2, frame->bytes[2])) {
        perror("bcc doesn't match in receiver");
        exit(2);
    }
}

void sendMessage(frame_t frame) {
    int attempts = 0;
    int sentBytes = 0;

    while (sentBytes != frame.size) {
        if (attempts >= MAX_ATTEMPTS) {
            perror("Too many failed attempts to send. Time out!\n");
            exit(-1);
        }

        printf("Write attempt %d\n", attempts);
        while (sentBytes != frame.size) {
            sentBytes += write(app.fd, frame.bytes, frame.size);
            printf("%d bytes sent\n", sentBytes);
        }
        attempts++;
    }
}

// ---

uint8_t getBit(uint8_t byte, uint8_t bit)
{
    return (byte >> bit) & BIT(0);
}

uint8_t bccCalculator(uint8_t bytes[], int start, size_t length)
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

bool bccVerifier(uint8_t bytes[], int start, size_t length, uint8_t parity)
{
    if (bccCalculator(bytes, start, length) == parity)
        return true;
    return false;
}

void buildSETFrame(frame_t *frame, bool transmitterToReceiver)
{
    frame->bytes = malloc(5);
    frame->size = 5;
    frame->bytes[0] = FLAG;
    if (transmitterToReceiver)
        frame->bytes[1] = TRANSMITTER_TO_RECEIVER;
    else
        frame->bytes[1] = RECEIVER_TO_TRANSMITTER;
    frame->bytes[2] = SET;
    frame->bytes[3] = 0;    // BCC
    frame->bytes[4] = FLAG;
}

void buildUAFrame(frame_t *frame, bool transmitterToReceiver)
{
    frame->bytes = malloc(5);
    frame->size = 5;
    frame->bytes[0] = FLAG;
    if (transmitterToReceiver)
        frame->bytes[1] = TRANSMITTER_TO_RECEIVER;
    else
        frame->bytes[1] = RECEIVER_TO_TRANSMITTER;
    frame->bytes[2] = SET;
    frame->bytes[3] = 1;    // BCC
    frame->bytes[4] = FLAG;
}

void destroyFrame(frame_t *frame)
{
    free(frame->bytes);
}

void printString(char *str)
{
    printf("\nStarting printString...\n\tSize: %ld\n", strlen(str));
    for (int i = 0; i < strlen(str); i++)
    {
        printf("\tstr[%d]: %c\n", i, str[i]);
    }
    printf("printString ended\n");
}

int prepareToReceive(frame_t *frame, size_t size)
{
    frame->size = size;
    return (frame->bytes = malloc(frame->size)) == NULL;
}
