#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "RUDP_API.h"

#define BUFFER_SIZE 1024

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int RECEIVER_PORT = atoi(argv[2]);
    if (RECEIVER_PORT <= 0 || RECEIVER_PORT > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        return 1;
    }

    struct timeval start_time, end_time;
    double total_time = 0;
    unsigned int total_bytes_received = 0;

    int sock = rudp_socket(false, RECEIVER_PORT); // Create a RUDP socket (client mode)
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Connection establishment for RUDP
    if (rudp_accept(sock) < 0) {
        fprintf(stderr, "Connection establishment failed\n");
        rudp_close(sock);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for UDP packets on port %d...\n", RECEIVER_PORT);

    int run = 1;
    while (1) {
        struct sockaddr_in sender;
        socklen_t sender_len = sizeof(sender);
        char buffer[BUFFER_SIZE];

        printf("Waiting for packet for Run #%d...\n", run);

        int bytes_received = rudp_recv(sock, buffer, BUFFER_SIZE);
        if (bytes_received < 0) {
            perror("recvfrom");
            rudp_close(sock);
            return 1;
        }

        gettimeofday(&end_time, NULL);
        double elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
        elapsed_time += (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        
        total_bytes_received += bytes_received;
        total_time += elapsed_time;

        double total_bandwidth = (total_bytes_received / (1024 * 1024)) / (total_time / 1000); // Convert bytes/ms to MB/s

        printf("Received packet for Run #%d:\n", run);
        printf("- Data: Time=%.2fms; Speed=%.2fMB/s\n", elapsed_time, total_bandwidth);

        run++;
    }

    rudp_close(sock);

    return 0;
}
