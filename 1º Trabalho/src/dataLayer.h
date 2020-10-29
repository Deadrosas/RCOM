#pragma once
#include "utils.h"

typedef struct {
    int fd; // serial port
    int status; // TRANSMITTER | RECEIVER
} applicationLayer;


typedef enum {
    INIT,
    RCV_FLAG,
    RCV_A,
    RCV_C,
    RCV_BCC,
    COMPLETE
} receive_state_t;

typedef struct {
    u_int8_t *bytes;
    int size;
} frame_t;


// ---

int llopen(char *port, int appStatus);
int llclose(int fd);

int receiveNotIMessage(frame_t *frame, bool isResponse);
int sendMessage(frame_t frame);
int clearSerialPort(char *port);

u_int8_t bccCalculator(u_int8_t bytes[], int start, size_t length);
bool bccVerifier(u_int8_t bytes[], int start, size_t length, u_int8_t parity);

int buildSETFrame(frame_t *frame, bool transmitterToReceiver);
bool isSETFrame(frame_t *frame);
int buildUAFrame(frame_t * frame, bool transmitterToReceiver);
bool isUAFrame(frame_t *frame);
int buildDISCFrame(frame_t * frame, bool transmitterToReceiver);
bool isDISCFrame(frame_t *frame);

int prepareToReceive(frame_t *frame, size_t size);
void destroyFrame(frame_t *frame);

