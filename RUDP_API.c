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
#include "RUDP_API.h"


#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 5 // Timeout in seconds for receiving acknowledgments

// Define flags for the RUDP protocol
#define SYN_FLAG    0x01
#define SYN_ACK_FLAG 0x02
#define ACK_FLAG    0x04


struct RUDPHeader{
    uint16_t length;    // 2 bytes for length
    uint16_t checksum;  // 2 bytes for checksum
    uint8_t flags;      // 1 byte for flags
};

typedef struct {
    uint32_t seq_num;   // Sequence number
    // Other fields as needed
    char data[BUFFER_SIZE];  // Data payload
} RUDP_Packet;

typedef struct _rudp_socket {
    int socket_fd; // UDP socket file descriptor
    bool isServer; // True if the RUDP socket acts like a server, false for client.
    bool isConnected; // True if there is an active connection, false otherwise.
    struct sockaddr_in dest_addr; // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;

// Allocates a new structure for the RUDP socket
RUDP_Socket* rudp_socket(bool isServer, unsigned short int listen_port) {
    RUDP_Socket *sock = malloc(sizeof(RUDP_Socket));
    if (sock == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }

    // Create UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sockfd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    sock->socket_fd = sockfd;
    sock->isServer = isServer;
    sock->isConnected = false;

    // Server binds to port
    if (isServer) {
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(listen_port);

        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Bind failed");
            exit(EXIT_FAILURE);
        }
    }

    return sock;
}

// Tries to connect to the other side via RUDP
int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port) {
    if (sockfd->isServer || sockfd->isConnected) {
        fprintf(stderr, "Invalid operation: Socket is already connected or set to server.\n");
        return 0;
    }

    sockfd->dest_addr.sin_family = AF_INET;
    sockfd->dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
    sockfd->dest_addr.sin_port = htons(dest_port);

    // Send SYN packet
    // Set up the SYN packet and send it to the server
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    struct RUDPHeader syn_packet;
    syn_packet.flags = SYN_FLAG;
    sendto(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));

    // Receive SYN-ACK packet
    struct RUDPHeader syn_ack_packet;
    socklen_t addr_len = sizeof(sockfd->dest_addr);
    ssize_t bytes_received = recvfrom(sockfd->socket_fd, &syn_ack_packet, sizeof(syn_ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);
    if (bytes_received <= 0 || syn_ack_packet.flags != SYN_ACK_FLAG) {
        fprintf(stderr, "Connection failed: SYN-ACK not received.\n");
        return 0;
    }

    // Send ACK packet
    struct RUDPHeader ack_packet;
    ack_packet.flags = ACK_FLAG;
    sendto(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));

    sockfd->isConnected = true;
    return 1;
}

// Accepts incoming connection request and completes the handshake
int rudp_accept(RUDP_Socket *sockfd) {
    if (!sockfd->isServer || sockfd->isConnected) {
        fprintf(stderr, "Invalid operation: Socket is already connected or not set to server.\n");
        return 0;
    }

    // Receive SYN packet
    struct RUDPHeader syn_packet;
    socklen_t addr_len = sizeof(sockfd->dest_addr);
    ssize_t bytes_received = recvfrom(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);
    if (bytes_received <= 0 || syn_packet.flags != SYN_FLAG) {
        fprintf(stderr, "Connection failed: SYN packet not received.\n");
        return 0;
    }

    // Send SYN-ACK packet
    struct RUDPHeader syn_ack_packet;
    syn_ack_packet.flags = SYN_ACK_FLAG;
    sendto(sockfd->socket_fd, &syn_ack_packet, sizeof(syn_ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));

    // Receive ACK packet
    struct RUDPHeader ack_packet;
    bytes_received = recvfrom(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);
    if (bytes_received <= 0 || ack_packet.flags != ACK_FLAG) {
        fprintf(stderr, "Connection failed: ACK packet not received.\n");
        return 0;
    }

    sockfd->isConnected = true;
    return 1;
}
// Receives data from the other side
int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size) {
    if (!sockfd->isConnected) {
        fprintf(stderr, "Invalid operation: Socket is not connected.\n");
        return -1;
    }

    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    int bytes_received = recvfrom(sockfd->socket_fd, buffer, buffer_size, 0, (struct sockaddr *)&sender_addr, &sender_len);
    if (bytes_received < 0) {
        perror("recvfrom");
        return -1;
    }
    return bytes_received;
}

// Sends data to the other side
int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size) {
    if (!sockfd->isConnected) {
        fprintf(stderr, "Invalid operation: Socket is not connected.\n");
        return -1;
    }

    int bytes_sent = sendto(sockfd->socket_fd, buffer, buffer_size, 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
    if (bytes_sent < 0) {
        perror("sendto");
        return -1;
    }
    return bytes_sent;
}

// Disconnects from an actively connected socket
int rudp_disconnect(RUDP_Socket *sockfd) {
    if (!sockfd->isConnected) {
        fprintf(stderr, "Invalid operation: Socket is not connected.\n");
        return 0;
    }

    // Simply close the underlying UDP socket
    close(sockfd->socket_fd);
    sockfd->isConnected = false;
    return 1;

}

// Closes the RUDP socket
int rudp_close(RUDP_Socket *sockfd) {
    if (sockfd != NULL) {
        close(sockfd->socket_fd);
        free(sockfd);
    }
    return 0;
}
