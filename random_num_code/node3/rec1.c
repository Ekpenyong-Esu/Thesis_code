#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <time.h>

#define BUFFER_SIZE 1500

double get_time_milliseconds() {
    return (double)clock() / CLOCKS_PER_SEC * 1000;
}

int main() {
    int sockfd;
    struct sockaddr_in addr;
    int port = 100;  // Use the same port as the sender code
    const char* ip_address = "192.168.191.234";  // Use the same IP address as the sender code

    // Create a UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    // Set up the address structure
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip_address, &(addr.sin_addr)) <= 0) {
        perror("Invalid address");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the address and port
    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("Failed to bind socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Open files for saving latency
    FILE *processing_latency_file = fopen("processing_latency.txt", "w");
    FILE *communication_latency_file = fopen("communication_latency.txt", "w");
    if (processing_latency_file == NULL || communication_latency_file == NULL) {
        perror("Failed to open latency files");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Receive and process incoming UDP packets
    while (1) {
        char buffer[BUFFER_SIZE];
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);

        double communication_start_time = get_time_milliseconds();

        // Measure start time for processing latency
        double processing_start_time = get_time_milliseconds();

        ssize_t recvLen = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&clientAddr, &clientAddrLen);
        if (recvLen == -1) {
            perror("Failed to receive packet");
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        double communication_end_time = get_time_milliseconds();
        double communication_latency = communication_end_time - communication_start_time;

        // Get the source IP address and port
        char clientIP[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddr.sin_addr), clientIP, INET_ADDRSTRLEN);
        int clientPort = ntohs(clientAddr.sin_port);

        // Process the received payload
        int payload;
        memcpy(&payload, buffer, sizeof(int));
        //payload = ntohl(payload);

        

        // Save processing latency to file
        double processing_latency = get_time_milliseconds() - processing_start_time;
        fprintf(processing_latency_file, "%.2f\n", processing_latency);
        fflush(processing_latency_file);

        // Save communication latency to file
        fprintf(communication_latency_file, "%.2f\n", communication_latency);
        fflush(communication_latency_file);

        // Print the received payload and source IP/port
    
        printf("Received UDP packet:\n");
        printf("Source IP: %s\n", clientIP);
        printf("Source Port: %d\n", clientPort);
        printf("Payload: %d\n", payload);
        printf("Communication Latency: %.2f ms\n", communication_latency);
        printf("Processing Latency: %.2f ms\n\n", processing_latency);
    }
    // Close the files
    fclose(processing_latency_file);
    fclose(communication_latency_file);
    // Close the socket
    close(sockfd);

 return 0;
}