#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5060
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

int main(void) {
    while (1) {
        // Generate random data
        unsigned int file_size = 2 * 1024 * 1024; // 2MB
        char *data = util_generate_random_data(file_size);

        // Write data to a file
        FILE *file = fopen("file.txt", "wb");
        if (file == NULL) {
            perror("File opening failed");
            exit(EXIT_FAILURE);
        }
        fwrite(data, sizeof(char), file_size, file);
        fclose(file);

        // Create a TCP socket
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Connect to the receiver
        struct sockaddr_in server_address;
        memset(&server_address, 0, sizeof(server_address));
        server_address.sin_family = AF_INET;
        server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
        server_address.sin_port = htons(SERVER_PORT);
        if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            perror("Connection failed");
            exit(EXIT_FAILURE);
        }

        // Send the file
        file = fopen("file.txt", "rb");
        if (file == NULL) {
            perror("File opening failed");
            exit(EXIT_FAILURE);
        }
        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
            if (send(sock, buffer, bytes_read, 0) < 0) {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
        }
        fclose(file);

        // Cleanup
        free(data);
        close(sock);

        // Prompt user for decision
        char choice;
        printf("Do you want to send the file again? (y/n): ");
        scanf(" %c", &choice);
        if (choice != 'y') {
            // If user chooses not to send again, send exit message
            char *exit_message = "EXIT"; // Define your exit message
            if (send(sock, exit_message, strlen(exit_message), 0) < 0) {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
            break; // Exit the loop
        }
    }

    printf("Server end.");
    
    return 0;
}
