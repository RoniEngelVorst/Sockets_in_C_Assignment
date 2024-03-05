#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/in.h>
#include "RUDP_API.h"

#define BUFFER_SIZE 1024

char *util_generate_random_data(unsigned int size) {
    char *buffer = (char *)malloc(size);
    if (buffer == NULL) {
        perror("Memory allocation failed");
        exit(EXIT_FAILURE);
    }
    srand(time(NULL));
    for (unsigned int i = 0; i < size; i++)
        buffer[i] = (char)(rand() % 256);
    return buffer;
}

int main(int argc, char** argv) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <port> <algorithm>\n", argv[0]);
        return 1;
    }

    char* SERVER_IP = argv[2];

    int SERVER_PORT = atoi(argv[4]);
    if (SERVER_PORT <= 0 || SERVER_PORT > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[4]);
        return 1;
    }

    int sock = rudp_socket(true, atoi(argv[2])); // Create a RUDP socket (server mode)
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_address.sin_port = htons(SERVER_PORT);

    // Connection establishment for RUDP
    if (rudp_connect(sock, SERVER_IP, SERVER_PORT) < 0) {
        fprintf(stderr, "Connection establishment failed\n");
        rudp_close(sock);
        exit(EXIT_FAILURE);
    }

    int sequence_number = 0;
    RUDP_Packet packet;

    while (1) {
        // Generate random data
        unsigned int file_size = 2 * 1024 * 1024; // 2MB
        char *data = util_generate_random_data(file_size);

        // Send the file
        size_t total_bytes_sent = 0;
        while (total_bytes_sent < file_size) {
            size_t bytes_to_send = BUFFER_SIZE;
            if (total_bytes_sent + bytes_to_send > file_size) {
                bytes_to_send = file_size - total_bytes_sent;
            }
            if (rudp_send(sock, data + total_bytes_sent, bytes_to_send) < 0) {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
            total_bytes_sent += bytes_to_send;
        }

        // Cleanup
        free(data);

        // Prompt user for decision
        char choice;
        printf("Do you want to send the file again? (y/n): ");
        scanf(" %c", &choice);
        if (choice != 'y') {
            break; // Exit the loop
        }
    }
    rudp_disconnect(sock);
    rudp_close(sock);

    printf("Client ended.\n");

    return 0;
}
