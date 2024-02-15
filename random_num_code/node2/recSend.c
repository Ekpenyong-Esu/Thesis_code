#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>





double getCurrentTimeMillis() {
  return (double)clock() / CLOCKS_PER_SEC * 1000;
}

#define FRAME_SIZE 52

int main() {
  int sockfd;
  struct sockaddr_ll sa;
  char frame[FRAME_SIZE];

  // Create a raw socket
  sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd == -1) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }

  // Set the network interface index
  memset(&sa, 0, sizeof(struct sockaddr_ll));
  sa.sll_family = AF_PACKET;
  sa.sll_protocol = htons(ETH_P_ALL);
  sa.sll_ifindex =
      if_nametoindex("enp0s31f6"); // Replace "enp0s3" with your interface name

  // Bind the socket to the network interface
  if (bind(sockfd, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll)) == -1) {
    perror("Failed to bind socket");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  /**
   * The processing latency is the time taken to process the received frame,
   * while the communication latency is the time taken to receive the frame over the network. The latency values are then saved to separate files
   */

  // Open the files for saving the latency
  FILE *processing_latency_file = fopen("processing_latency.txt", "w");
  FILE *communication_latency_file = fopen("communication_latency.txt", "w");
  if (processing_latency_file == NULL || communication_latency_file == NULL) {
    perror("Failed to open latency files");
    close(sockfd);
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Measure start time for communication latency
    double communication_start_time = getCurrentTimeMillis();

    // Receive the frame
    ssize_t recv_len = recv(sockfd, frame, FRAME_SIZE, 0);
    if (recv_len == -1) {
      perror("Failed to receive frame");
      close(sockfd);
      fclose(processing_latency_file);
      fclose(communication_latency_file);
      exit(EXIT_FAILURE);
    }

    // Measure end time for communication latency
    double communication_end_time = getCurrentTimeMillis();

    // Calculate communication latency
    double communication_latency = communication_end_time - communication_start_time;

    // Process the received frame
    struct ether_header *eth_header = (struct ether_header *)frame;
    uint16_t eth_type = ntohs(eth_header->ether_type);

    if (eth_type == ETH_P_IP) {
      // Measure start time for processing latency
      double processing_start_time = getCurrentTimeMillis();

      // IP packet
      struct iphdr *ip_header = (struct iphdr *)(eth_header + 1);
      uint8_t ip_header_len = (ip_header->ihl) * 4;
      struct udphdr *udp_header =
          (struct udphdr *)((uint8_t *)ip_header + ip_header_len);
      uint8_t *udp_payload = (uint8_t *)(udp_header + 1);

      // Retrieve the payload
      int payload;
      memcpy(&payload, udp_payload, sizeof(int));
      //payload = ntohl(payload);
      
      // Print the received payload and source/destination port
      printf("Received UDP packet:\n");
      printf("Source Port: %u\n", ntohs(udp_header->source));
      printf("Destination Port: %u\n", ntohs(udp_header->dest));
      printf("Payload: %d\n\n", payload);
      printf("communication_latency: %.2f ms\n", communication_latency);


      

      // Send the payload to the third device
      int send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
      if (send_sockfd == -1) {
        perror("Failed to create send socket");
        exit(EXIT_FAILURE);
       }

      struct sockaddr_in dest_addr;
      memset(&dest_addr, 0, sizeof(dest_addr));
      dest_addr.sin_family = AF_INET;
      dest_addr.sin_port = htons(100);

      if (inet_aton("192.168.191.234", &dest_addr.sin_addr) == 0) {
      perror("Invalid destination IP address");
      close(send_sockfd);
      exit(EXIT_FAILURE);
      }

      ssize_t send_len = sendto(send_sockfd, &payload, sizeof(int), 0,
                            (struct sockaddr *)&dest_addr, sizeof(dest_addr));
      if (send_len == -1) {
      perror("Failed to send payload");
      close(send_sockfd);
      exit(EXIT_FAILURE);
      }


        // Measure end time for processing latency
      double processing_end_time = getCurrentTimeMillis();

      // Calculate processing latency
      double processing_latency = processing_end_time - processing_start_time;

      // Save processing latency to file
      fprintf(processing_latency_file, "%.2f\n", processing_latency);
      fflush(processing_latency_file);

     close(send_sockfd);

    }

   // Save communication latency to file
   fprintf(communication_latency_file, "%.2f\n", communication_latency);
   fflush(communication_latency_file);
 }

   // Close the files
 fclose(processing_latency_file);
 fclose(communication_latency_file);

 // close the socket
 close(sockfd);

 return 0;
}
