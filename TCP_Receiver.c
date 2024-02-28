#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/tcp.h>

#define MAX_CLIENTS 1
#define BUFFER_SIZE 1024

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port> <algorithm>\n", argv[0]);
        return 1;
    }

    int RECEIVER_PORT = atoi(argv[2]);
    if (RECEIVER_PORT <= 0 || RECEIVER_PORT > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[2]);
        return 1;
    }

    char *algorithm = argv[4];
    if (strcmp(algorithm, "reno") != 0 && strcmp(algorithm, "cubic") != 0) {
        fprintf(stderr, "Invalid algorithm: %s\n", algorithm);
        return 1;
    }
    socklen_t len = strlen(algorithm) + 1;

    struct timeval start_time, end_time;
    double total_time = 0;
    unsigned int total_bytes_received = 0;
    int sock = -1;
    struct sockaddr_in sender;
    struct sockaddr_in receiver;
    socklen_t sender_len = sizeof(sender);
    memset(&sender, 0, sizeof(sender));
    memset(&receiver, 0, sizeof(receiver));

    printf("Starting Receiver....\n");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket(2)");
        return 1;
    }

    if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algorithm, len) != 0) {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    receiver.sin_addr.s_addr = INADDR_ANY;
    receiver.sin_family = AF_INET;
    receiver.sin_port = htons(RECEIVER_PORT);

    if (bind(sock, (struct sockaddr *)&receiver, sizeof(receiver)) < 0) {
        perror("bind(2)");
        close(sock);
        return 1;
    }

    if (listen(sock, MAX_CLIENTS) < 0) {
        perror("listen(2)");
        close(sock);
        return 1;
    }

    printf("Waiting for TCP connections...\n");
    int total = 0;
    int run = 1;
    while (1) {
        int client_sock = accept(sock, (struct sockaddr *)&sender, &sender_len);
        if (client_sock < 0) {
            perror("accept(2)");
            close(sock);
            return 1;
        }

        printf("Sender connected, beginning to receive file for Run #%d...\n", run);

        gettimeofday(&start_time, NULL);

        FILE *file = fopen("received_file.txt", "wb");
        if (file == NULL) {
            perror("File opening failed");
            exit(EXIT_FAILURE);
        }

        total_bytes_received = 0; // Reset total bytes received for this run
        char buffer[BUFFER_SIZE];
        int bytes_received = 1;
        unsigned int expectedBytes = 1<<21;
        while(bytes_received){
            bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
            if(bytes_received < 0){
                perror("recv");
                exit(EXIT_FAILURE);
            } else if (bytes_received == 0) {
                // Socket closed by sender
                break;
            }
            total_bytes_received += bytes_received;
            total += bytes_received;
            if(total_bytes_received >= expectedBytes){
                // Print statistics and reset for next chunk
                gettimeofday(&end_time, NULL);
                double elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
                elapsed_time += (end_time.tv_usec - start_time.tv_usec) / 1000.0;
                double total_bandwidth_fn = (total_bytes_received / (1024 * 1024)) / (elapsed_time / 1000); // Convert bytes/ms to MB/s
                printf(" - File transfer completed for Run #%d.\n", run);
                printf(" - Run #%d Data: Time=%.2fms; Speed=%.2fMB/s\n", run, elapsed_time, total_bandwidth_fn);
                total_time += elapsed_time;
                run++;
                printf("Waiting for Sender response...\n");
                total_bytes_received = 0; // Reset for next chunk
                gettimeofday(&start_time, NULL);
            }

        }

        fclose(file);
        close(client_sock);



        if (bytes_received == 0) {
            printf("Sender closed the connection.\n");
            break; // Exit the loop if sender closed the connection
        } else if (bytes_received < 0) {
            perror("recv");
            exit(EXIT_FAILURE);
        }
    }

    double average_time = total_time / (run - 1); // Exclude the run when exit message was received
    double total_bandwidth = (total / (1024 * 1024)) / (total_time / 1000); // Convert bytes/ms to MB/s
    
    printf("----------------------------------\n");
    printf("Statistics for the entire program:\n");
    printf("- Average time: %.2fms\n", average_time);
    printf("- Total average bandwidth: %.2fMB/s\n", total_bandwidth);
    printf("----------------------------------\n");
    printf("Receiver end.\n");
    return 0;
}
