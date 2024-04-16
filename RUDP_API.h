// RUDP_API.h

#ifndef RUDP_API_H
#define RUDP_API_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
// #include "RUDP_API.h"

#define BUFFER_SIZE 1024

// typedef struct {
//     RUDP_Packet packet; // The packet
//     struct timeval sent_time; // Timestamp when the packet was sent
//     bool ack_received; // Flag to track acknowledgment
// } BufferedPacket;


typedef struct {
    uint16_t length;    // 2 bytes for length
    uint16_t checksum;  // 2 bytes for checksum
    uint8_t flags;      // 1 byte for flags
}RUDPHeader;

typedef struct {
    uint32_t seq_num;   // Sequence number
    char data[BUFFER_SIZE];  // Data payload
    RUDPHeader header; //The header
} RUDP_Packet;

// Define flags for the RUDP protocol
#define SYN_FLAG    0x01
#define SYN_ACK_FLAG 0x02
#define ACK_FLAG    0x04

// Structure representing the RUDP socket
typedef struct _rudp_socket RUDP_Socket;

// Allocates a new structure for the RUDP socket
RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port);

// Tries to connect to the other side via RUDP
int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port);

// Accepts incoming connection request and completes the handshake
int rudp_accept(RUDP_Socket *sockfd);

// Receives data from the other side
int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);

// Sends data to the other side
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size);

// Disconnects from an actively connected socket
int rudp_disconnect(RUDP_Socket *sockfd);

// Closes the RUDP socket
int rudp_close(RUDP_Socket *sockfd);

unsigned short int calculate_checksum(void *data, unsigned int bytes);

#endif