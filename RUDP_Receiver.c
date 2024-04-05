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

    // Create a RUDP socket (server mode)
    RUDP_Socket * server_sock = rudp_socket(true, RECEIVER_PORT);
    if (server_sock == NULL) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    printf("Waiting for RUDP connections..\n");

    // Accept incoming connections
    if (!rudp_accept(server_sock)) {
        perror("Accept failed");
        rudp_close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Connection accepted. Ready to receive data.\n");

    int run = 1;
    while (1) {
        printf("Waiting for packet for Run #%d...\n", run);

        char buffer[BUFFER_SIZE];
        int bytes_received = rudp_recv(server_sock, buffer, BUFFER_SIZE);
        if (bytes_received < 0) {
            perror("recvfrom");
            rudp_close(server_sock);
            return 1;
        }

        printf("Received packet for Run #%d...\n", run);

        // Process received data...

        total_bytes_received += bytes_received;

        if (total_bytes_received >= (1 << 21)) {
            gettimeofday(&end_time, NULL);
            double elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
            elapsed_time += (end_time.tv_usec - start_time.tv_usec) / 1000.0;
            double total_bandwidth_fn = (total_bytes_received / (1024 * 1024)) / (elapsed_time / 1000);
            printf(" - File transfer completed for Run #%d.\n", run);
            printf(" - Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", run, elapsed_time, total_bandwidth_fn);
            total_time += elapsed_time;
            run++;
            printf("Waiting for Sender response...\n");
            total_bytes_received = 0;
            gettimeofday(&start_time, NULL);
        }
    }

    rudp_disconnect(server_sock);
    // Close the socket
    rudp_close(server_sock);

    double average_time = total_time / (run - 1);
    printf("----------------------------------\n");
    printf("Statistics for the entire program:\n");
    printf("- Average time: %.2fms\n", average_time);
    printf("----------------------------------\n");
    printf("Receiver end.\n");
    return 0;
}
