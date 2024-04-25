#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include "RUDP_API.h"

#define BUFFER_SIZE 65507
#define TOTAL_DATA_SIZE 2097152 // 2MB in bytes


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

    // Create a large buffer to accumulate all received data.
    char *big_buffer = malloc(TOTAL_DATA_SIZE);
    if (big_buffer == NULL) {
        perror("Failed to allocate buffer");
        exit(EXIT_FAILURE);
    }

    struct timeval start_time, end_time;
    double total_time = 0;
    unsigned int total_bytes_received = 0;

    

    // Open a file to save received data
    FILE *file = fopen("received_data.bin", "wb");
    if (file == NULL) {
        perror("Failed to open file");
        free(big_buffer);
        exit(EXIT_FAILURE);
    }

    // Create a RUDP socket (server mode)
    RUDP_Socket * server_sock = rudp_socket(true, RECEIVER_PORT);
    if (server_sock == NULL) {
        perror("Socket creation failed");
        fclose(file);
        free(big_buffer);
        exit(EXIT_FAILURE);
    }

    printf("Waiting for RUDP connections..\n");

    // Accept incoming connections
    if (!rudp_accept(server_sock)) {
        perror("Accept failed");
        fclose(file);
        rudp_close(server_sock);
        exit(EXIT_FAILURE);
    }

    printf("Connection accepted. Ready to receive data.\n");

    

    int expectingEndSignal = 0;  // Flag to determine if we should check for an end signal
    int run = 1;
    int bytes_received = 1;
    while (bytes_received) {
        memset(big_buffer, 0, TOTAL_DATA_SIZE); // Clear the buffer at the start of each connection handling loop.
        printf("Waiting for packet for Run #%d...\n", run);
        gettimeofday(&start_time, NULL);

        // char buffer[BUFFER_SIZE];
        bytes_received = rudp_recv(server_sock, big_buffer, TOTAL_DATA_SIZE);
        if (bytes_received < 0) {
            perror("recvfrom");
            rudp_close(server_sock);
            return 1;
        }
        // else if (bytes_received == 0) {  // End signal received
        //     printf("End of transmission.\n");
        //     break;
        // }

        int written = fwrite(big_buffer, 1, bytes_received, file);
        if (written < bytes_received) {
            perror("Failed to write to file");
            break;
        }

        printf("Received packet for Run #%d...\n", run);

        

        // Process received data...

        total_bytes_received += bytes_received;

        if (total_bytes_received >= (1 << 21)) {
            expectingEndSignal = 1;
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
            // gettimeofday(&start_time, NULL);
        }

        // printf("bytes_received: %d\n", bytes_received);
        //add an if in order to stop the loop when sender said no
        
        if (expectingEndSignal && rudp_recv_end_signal(server_sock) == 1) {
            printf("Proper termination of the session confirmed.\n");
            break;
            // Perform cleanup or state reset here
        }
        else {
            printf("Continuing data reception...\n");
        }

        if (bytes_received == 0){
            printf("Sender closed the connection.\n");
            break; // Exit the loop if sender closed the connection
        }else if (bytes_received < 0){
            perror("recv");
            exit(EXIT_FAILURE);
        }
    }

    // Close the file and socket when done
        fclose(file);
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
