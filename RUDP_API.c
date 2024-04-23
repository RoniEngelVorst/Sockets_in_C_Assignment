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


// #define BUFFER_SIZE 1024
#define TIMEOUT_SEC 5 // Timeout in seconds for receiving acknowledgments

// Define flags for the RUDP protocol
#define SYN_FLAG    0x01
#define SYN_ACK_FLAG 0x02
#define ACK_FLAG    0x04

#define MAX_UDP_PAYLOAD_SIZE 65507


typedef struct _rudp_socket {
    int socket_fd; // UDP socket file descriptor
    bool isServer; // True if the RUDP socket acts like a server, false for client.
    bool isConnected; // True if there is an active connection, false otherwise.
    struct sockaddr_in dest_addr; // Destination address. Client fills it when it connects via rudp_connect(), server fills it when it accepts a connection via rudp_accept().
} RUDP_Socket;

int sequence_number = 0; // Initial sequence number
unsigned expected_sequence_number = 0; // Initial expected sequence number


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

    // Set SO_REUSEADDR option
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Setting SO_REUSEADDR option failed");
        exit(EXIT_FAILURE);
    }

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

    // Set SO_REUSEADDR option
    int optval = 1;
    if (setsockopt(sockfd->socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("Setting SO_REUSEADDR option failed");
        exit(EXIT_FAILURE);
    }

    // Send SYN packet
    // Set up the SYN packet and send it to the server
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd->socket_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    RUDPHeader syn_packet;
    syn_packet.flags = SYN_FLAG;
    sendto(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));

    // Receive SYN-ACK packet
    RUDPHeader syn_ack_packet;
    socklen_t addr_len = sizeof(sockfd->dest_addr);
    ssize_t bytes_received = recvfrom(sockfd->socket_fd, &syn_ack_packet, sizeof(syn_ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);
    if (bytes_received <= 0 || syn_ack_packet.flags != SYN_ACK_FLAG) {
        fprintf(stderr, "Connection failed: SYN-ACK not received.\n");
        return 0;
    }

    // Send ACK packet
    RUDPHeader ack_packet;
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
    RUDPHeader syn_packet;
    socklen_t addr_len = sizeof(sockfd->dest_addr);
    ssize_t bytes_received = recvfrom(sockfd->socket_fd, &syn_packet, sizeof(syn_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);
    if (bytes_received <= 0 || syn_packet.flags != SYN_FLAG) {
        fprintf(stderr, "Connection failed: SYN packet not received.\n");
        return 0;
    }

    // Send SYN-ACK packet
    RUDPHeader syn_ack_packet;
    syn_ack_packet.flags = SYN_ACK_FLAG;
    sendto(sockfd->socket_fd, &syn_ack_packet, sizeof(syn_ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));

    // Receive ACK packet
    RUDPHeader ack_packet;
    bytes_received = recvfrom(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&(sockfd->dest_addr), &addr_len);
    if (bytes_received <= 0 || ack_packet.flags != ACK_FLAG) {
        fprintf(stderr, "Connection failed: ACK packet not received.\n");
        return 0;
    }

    sockfd->isConnected = true;
    return 1;
}

int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size) {
    if (!sockfd->isConnected) {
        fprintf(stderr, "Invalid operation: Socket is not connected.\n");
        return -1;
    }
    

    struct sockaddr_in sender_addr;
    socklen_t sender_len = sizeof(sender_addr);
    int total_bytes_received = 0;
    unsigned int total_data_bytes_received = 0;
    //check sequance number
    uint32_t expected_seq_number = 1;

    while(1){
        if(expected_seq_number == 33){
            RUDP_LAST_PACKET lpacket;
            int bytes_received = recvfrom(sockfd->socket_fd, &lpacket, sizeof(RUDP_LAST_PACKET), 0, (struct sockaddr *)&sender_addr, &sender_len);
            if (bytes_received < 0) {
                perror("recvfrom");
                return -1;
            }

            if(expected_seq_number != lpacket.seq_num){
                fprintf(stderr, "Invalid sequence number. Expected: %d, Received: %d\n", expected_seq_number, lpacket.seq_num);
                return -1;
            }

            // Extract header information
            unsigned short int received_checksum = lpacket.header.checksum;
            lpacket.header.checksum = 0; // Reset checksum field before calculating checksum
            unsigned short int calculated_checksum = calculate_checksum(lpacket.data, lpacket.header.length);

            // Verify checksum
            if (received_checksum != calculated_checksum) {
                fprintf(stderr, "Checksum verification failed.\n");
                return -1;
            }

            if (buffer == NULL) {
                fprintf(stderr, "Buffer pointer is null\n");
                return -1;
            }

            if ((unsigned int)(total_data_bytes_received + lpacket.header.length) > buffer_size) {
                printf("Buffer overflow prevented. Total bytes so far: %d, Current packet size: %d, Buffer size: %d\n", total_bytes_received, lpacket.header.length, buffer_size);
                return -1;
            }

            memcpy((char *)buffer + total_data_bytes_received, lpacket.data, lpacket.header.length);


            // Send ACK back to the sender
            RUDPHeader ack_packet;
            ack_packet.flags = ACK_FLAG;
            sendto(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&sender_addr, sender_len);
            printf("sending an ack. for packet %d\n", expected_seq_number);
            
            total_bytes_received = total_bytes_received + bytes_received;
            total_data_bytes_received = total_data_bytes_received + lpacket.header.length;
            printf("total data bytes received is %d\n", total_data_bytes_received);
            
            expected_seq_number++;

            if(total_data_bytes_received == buffer_size){
                fprintf(stderr, "Received enough bytes\n");
                break;
            }


        }
        // Receive the packet
        RUDP_Packet packet;
        int bytes_received = recvfrom(sockfd->socket_fd, &packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&sender_addr, &sender_len);
        if (bytes_received < 0) {
            perror("recvfrom");
            return -1;
        }

        
        if(expected_seq_number != packet.seq_num){
            fprintf(stderr, "Invalid sequence number. Expected: %d, Received: %d\n", expected_seq_number, packet.seq_num);
            return -1;
        }

        // Extract header information
        unsigned short int received_checksum = packet.header.checksum;
        packet.header.checksum = 0; // Reset checksum field before calculating checksum
        unsigned short int calculated_checksum = calculate_checksum(packet.data, packet.header.length);

        // Verify checksum
        if (received_checksum != calculated_checksum) {
            fprintf(stderr, "Checksum verification failed.\n");
            return -1;
        }

        if (buffer == NULL) {
            fprintf(stderr, "Buffer pointer is null\n");
            return -1;
        }

        if ((unsigned int)(total_data_bytes_received + packet.header.length) > buffer_size) {
            printf("Buffer overflow prevented. Total bytes so far: %d, Current packet size: %d, Buffer size: %d\n", total_bytes_received, packet.header.length, buffer_size);
            return -1;
        }

        memcpy((char *)buffer + total_data_bytes_received, packet.data, packet.header.length);


        // Send ACK back to the sender
        RUDPHeader ack_packet;
        ack_packet.flags = ACK_FLAG;
        sendto(sockfd->socket_fd, &ack_packet, sizeof(ack_packet), 0, (struct sockaddr *)&sender_addr, sender_len);
        printf("sending an ack. for packet %d\n", expected_seq_number);
        
        total_bytes_received = total_bytes_received + bytes_received;
        total_data_bytes_received = total_data_bytes_received + packet.header.length;
        printf("total data bytes received is %d\n", total_data_bytes_received);
        
        expected_seq_number++;

        if(total_data_bytes_received == buffer_size){
            // fprintf(stderr, "Received enough bytes\n");
            break;
        }
    }
    return total_bytes_received;

    
}


int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size) {
    if (sockfd == NULL) {
        fprintf(stderr, "Invalid RUDP socket\n");
        return -1;
    }

    if (!sockfd->isConnected) {
        fprintf(stderr, "Invalid operation: Socket is not connected.\n");
        return -1;
    }

    // Calculate the size of each data packet payload
    int data_in_packet_size = 65400;
    

    // Calculate the number of packets needed
    uint32_t numOfPackets = buffer_size / data_in_packet_size;

    // Account for remaining data
    int remaining_bytes = buffer_size % data_in_packet_size;
    if (remaining_bytes > 0) {
        numOfPackets++;
    }
    remaining_bytes = buffer_size;

    // Pointer to track the current position in the buffer
    char *current_position = (char *)buffer;

    printf("num of packets: %d\n", numOfPackets);

    // Loop through each packet
    for (uint32_t i = 1; i <= numOfPackets-1; i++) {
        RUDP_Packet packet;

        // Set sequence number
        packet.seq_num = i;

        // Calculate data size for this packet
        int data_size = data_in_packet_size;
        printf("data size for this packet is: %d\n",data_size);


        // Copy data from buffer to packet
        memcpy(packet.data, current_position, data_size);

        // Set header fields
        packet.header.length = data_size;
        packet.header.checksum = calculate_checksum(packet.data, data_size);
        packet.header.flags = 0; // Set appropriate flags if needed

        
        // Send the packet
        int bytes_sent = sendto(sockfd->socket_fd, &packet, sizeof(RUDP_Packet), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
        if (bytes_sent == -1) {
            perror("sendto() failed");
            return -1;  // Handle the error appropriately
        }

        remaining_bytes = remaining_bytes-data_size;

        // Move to the next portion of data in the buffer
        current_position += data_size;

        // Receive ACK for this packet
        RUDPHeader ack_packet;
        int ack_bytes_received = recv(sockfd->socket_fd, &ack_packet, sizeof(RUDPHeader), 0);
        if (ack_bytes_received == -1) {
            perror("recv() failed");
            return -1;  // Handle the error appropriately
        }

        // Check if the received packet is an ACK
        if (ack_packet.flags != ACK_FLAG) {
            fprintf(stderr, "Received packet is not an ACK\n");
            return -1;  // Handle the error appropriately
        }

        // ACK received successfully
        printf("ACK received for packet %u\n", packet.seq_num);
    

    }

    int data_size = 4352;
    RUDP_LAST_PACKET last_packet;
    last_packet.seq_num = (uint32_t)numOfPackets;
    memcpy(last_packet.data, current_position, data_size);
    printf("data size for this packet is: %d\n",data_size);

    // Set header fields
    last_packet.header.length = data_size;
    last_packet.header.checksum = calculate_checksum(last_packet.data, data_size);
    last_packet.header.flags = 0; // Set appropriate flags if needed

    // Send the packet
    int bytes_sent = sendto(sockfd->socket_fd, &last_packet, sizeof(RUDP_LAST_PACKET), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
    if (bytes_sent == -1) {
        perror("sendto() failed");
        return -1;  // Handle the error appropriately
    }

    // Receive ACK for this packet
    RUDPHeader ack_packet;
    int ack_bytes_received = recv(sockfd->socket_fd, &ack_packet, sizeof(RUDPHeader), 0);
    if (ack_bytes_received == -1) {
        perror("recv() failed");
        return -1;  // Handle the error appropriately
    }

    // Check if the received packet is an ACK
    if (ack_packet.flags != ACK_FLAG) {
        fprintf(stderr, "Received packet is not an ACK\n");
        return -1;  // Handle the error appropriately
    }

    // ACK received successfully
    printf("ACK received for packet %u\n", last_packet.seq_num);



    return 0; // Success
}

// Closes the RUDP socket
int rudp_close(RUDP_Socket *sockfd) {
    if (sockfd != NULL) {
        close(sockfd->socket_fd);
        free(sockfd);
    }
    return 0;
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

/*
* @brief A checksum function that returns 16 bit checksum for data.
* @param data The data to do the checksum for.
* @param bytes The length of the data in bytes.
* @return The checksum itself as 16 bit unsigned number.
* @note This function is taken from RFC1071, can be found here:
* @note https://tools.ietf.org/html/rfc1071
* @note It is the simplest way to calculate a checksum and is not very strong.
* However, it is good enough for this assignment.
* @note You are free to use any other checksum function as well.
* You can also use this function as such without any change.
*/
unsigned short int calculate_checksum(void *data, unsigned int bytes) {
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
    // Main summing loop
    while (bytes > 1) {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
    // Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);
    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    return (~((unsigned short int)total_sum));
}
