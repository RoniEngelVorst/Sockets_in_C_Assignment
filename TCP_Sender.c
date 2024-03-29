#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <netinet/tcp.h>


#define BUFFER_SIZE 1024

//given function in the assignment file. generates a random file with a given size.
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


//main function
int main(int argc, char** argv) {

    //check if the number of the arguments is indeed 7.
    if (argc != 7) {
        fprintf(stderr, "Usage: %s <port> <algorithm>\n", argv[0]);
        return 1;
    }

    //defining the ip to be the input ip
    char* SERVER_IP = argv[2];

    //defining the port to be the input port
    int SERVER_PORT = atoi(argv[4]);
    
    //checking that it is a valid port
    if (SERVER_PORT <= 0 || SERVER_PORT > 65535) {
        fprintf(stderr, "Invalid port number: %s\n", argv[4]);
        return 1;
    }
    
    //defining the algorithm to be the imput algorithm
    char *algorithm = argv[6];

    //checking that this is a valid algorithm
    if (strcmp(algorithm, "reno") != 0 && strcmp(algorithm, "cubic") != 0) {
        fprintf(stderr, "Invalid algorithm: %s\n", algorithm);
        return 1;
    }
    //defining the lengh of the algorithm name
    socklen_t len = strlen(algorithm);

    //creating a socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    //checking that the socket created succesfully
    if (sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //setting the algorithm in the socket to be the input algorithm
    if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, algorithm, len) != 0) {
        perror("setsockopt");
        close(sock);
        exit(EXIT_FAILURE);
    }


    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_address.sin_port = htons(SERVER_PORT);
    
    //connecting to the server
    if (connect(sock, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(EXIT_FAILURE);
    }

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

        // Send the file
        file = fopen("file.txt", "rb");
        if (file == NULL) {
            perror("File opening failed");
            exit(EXIT_FAILURE);
        }
        char buffer[BUFFER_SIZE];
        size_t bytes_read;
        //reading the data from the file
        while ((bytes_read = fread(buffer, sizeof(char), BUFFER_SIZE, file)) > 0) {
            if (send(sock, buffer, bytes_read, 0) < 0) {
                perror("Send failed");
                exit(EXIT_FAILURE);
            }
        }
        fclose(file);

        // Cleanup
        free(data);

        // Prompt user for decision if it wants to send the file again or not
        char choice;
        printf("Do you want to send the file again? (y/n): ");
        scanf(" %c", &choice);
        if (choice != 'y') {
            break; // Exit the loop
        }
    }
    close(sock);

    printf("Client ended.\n");
    
    return 0;
}
