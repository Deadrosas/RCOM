#pragma once

#define BAUDRATE B38400
#define _POSIX_SOURCE 1 /* POSIX compliant source */

#define FLAG 0x7e           // F

#define TRANSMITTER_TO_RECEIVER 0x03    // A
#define RECEIVER_TO_TRANSMITTER 0x01

#define SET 0x03        // C
#define DISC 0x0b
#define UA 0x07
#define RR 0x05  // 0b R 0 0 0 0 1 0 1
#define REJ 0x01 // 0b R 0 0 0 0 0 0 1
#define I 0x00   // 0b 0 S 0 0 0 0 0 0

#define DATA 1
#define START 2
#define END 3
#define FILENAME 0
#define FILESIZE 1
#define RR_MASK 0x7f
#define REJ_MASK 0x7f
#define I_MASK 0xbf

#define ESC 0x7d
#define FLAG_STUFFING 0x5e
#define ESC_STUFFING 0x5d
#define FLAG_MORE_FRAMES_TO_COME 0xaa
#define FLAG_LAST_FRAME 0xbb

#define RECEIVER 0
#define TRANSMITTER 1
#define SERIAL_PORT_1 "/dev/ttyS1"
#define SERIAL_PORT_2 "/dev/ttyS0"

#define MAX_WRITE_ATTEMPTS 3
#define MAX_READ_ATTEMPTS 3

#define MAX_FRAME_SIZE 256                               // minimum is 16
#define MAX_FRAME_DATA_LENGTH (MAX_FRAME_SIZE - 8)/2     // if all bytes are stuffed it takes the MAX_FRAME_SIZE
#define MAX_DATA_PACKET_LENGTH MAX_FRAME_DATA_LENGTH     // the max size of a data packet is the same as the max size of data
#define MAX_DATA_PACKET_DATA_LENGTH (MAX_DATA_PACKET_LENGTH - 4)
#define MAX_FRAME_RETRANSMISSIONS 3

#define RESPONSE_WITHOUT_ID -1
