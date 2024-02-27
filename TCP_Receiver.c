#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>

#define RECEIVER_PORT 5566
#define MAX_CLIENTS 1
#define BUFFER_SIZE 1024

int main() {
    struct timeval start_time, end_time;
    double total_time = 0;
    unsigned int total_bytes_received = 0;
    int sock = -1;
    struct sockaddr_in sender;
    struct sockaddr_in receiver;
    socklen_t sender_len = sizeof(sender);
    int opt = 1;
    memset(&sender, 0, sizeof(sender));
    memset(&receiver, 0, sizeof(receiver));

    printf("Starting Receiver....\n");

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket(2)");
        return 1;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
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

        char buffer[BUFFER_SIZE];
        int bytes_received;
        while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, sizeof(char), bytes_received, file);
            total_bytes_received += bytes_received;
        }

        // Check for exit message
        char exit_message[BUFFER_SIZE] = {0};
        recv(client_sock, exit_message, BUFFER_SIZE, 0);
        if (strcmp(exit_message, "EXIT") == 0) {
            printf("Received exit message from sender.\n");
            break;
        }

        gettimeofday(&end_time, NULL);
        fclose(file);
        close(client_sock);

        printf("File transfer completed for Run #%d.\n", run);

        double elapsed_time = (end_time.tv_sec - start_time.tv_sec) * 1000.0;
        elapsed_time += (end_time.tv_usec - start_time.tv_usec) / 1000.0;
        total_time += elapsed_time;
        
        run++;
    }

    double average_time = total_time / (run - 1); // Exclude the run when exit message was received
    double total_bandwidth = (total_bytes_received / (1024 * 1024)) / (total_time / 1000); // Convert bytes/ms to MB/s

    printf("----------------------------------\n");
    printf("- * Statistics * -\n");
    printf("- Run #1 Data: Time=%.2fms; Speed=%.2fMB/s\n", total_time, total_bandwidth);
    printf("-\n");
    printf("- Average time: %.2fms\n", average_time);
    printf("- Total average bandwidth: %.2fMB/s\n", total_bandwidth);
    printf("----------------------------------\n");
    printf("Receiver end\n");

    return 0;
}
