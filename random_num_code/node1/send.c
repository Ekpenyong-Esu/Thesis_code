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

#define FRAME_SIZE 1500

double getCurrentTimeMillis() {
    struct timeval currentTime;
    gettimeofday(&currentTime, NULL);
    double milliseconds = currentTime.tv_sec * 1000.0 + currentTime.tv_usec / 1000.0;
    return milliseconds;
}

// Function to generate a random number between min and max (inclusive)
int generateRandomNumber(int min, int max) {
  return min + rand() % (max - min + 1);
}

double get_time_milliseconds() {
  return (double)clock() / CLOCKS_PER_SEC * 1000;
}

int main() {
  int sockfd;
  struct sockaddr_ll sa;
  struct ether_header eth_header;
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

  // Set the Ethernet header
  struct ether_addr source_mac;
  struct ether_addr dest_mac;

  // Set the source MAC address
  ether_aton_r("3C:52:82:43.B2:29", &source_mac);
  memcpy(eth_header.ether_shost, &source_mac, ETH_ALEN);

  // Set the destination MAC address
  ether_aton_r("3C:52:82:43:A2:C8", &dest_mac);
  memcpy(eth_header.ether_dhost, &dest_mac, ETH_ALEN);

  eth_header.ether_type = htons(ETH_P_IP); // IP packet

  // Construct the frame with IP and UDP packet
  memcpy(frame, &eth_header, sizeof(struct ether_header));

  // IP packet
  struct iphdr ip_header;
  ip_header.version = 4;
  ip_header.ihl = 5;
  ip_header.tos = 0;
  ip_header.tot_len =
      htons(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(int));
  ip_header.id = 0;
  ip_header.frag_off = 0;
  ip_header.ttl = 64;
  ip_header.protocol = IPPROTO_UDP;
  ip_header.check = 0; // Set the checksum to 0 for simplicity
  ip_header.saddr =
      inet_addr("192.168.4.45"); // Replace with your source IP address
  ip_header.daddr =
      inet_addr("192.168.4.25"); // Replace with your destination IP address

  // Add IP header to the frame
  memcpy(frame + sizeof(struct ether_header), &ip_header, sizeof(struct iphdr));

  // UDP packet
  struct udphdr udp_header;
  udp_header.source = htons(200); // Replace with the desired source port
  udp_header.dest = htons(800);   // Replace with the desired destination port
  udp_header.len = htons(sizeof(struct udphdr) + sizeof(int)); // UDP header size + payload size
  udp_header.check = 0;                // Set the checksum to 0 for simplicity

  // Add UDP header to
  memcpy(frame + sizeof(struct ether_header) + sizeof(struct iphdr),
         &udp_header, sizeof(struct udphdr));

  srand(time(NULL)); // Seed the random number generator

  // Open the file for writing latency data
  FILE *latencyFile = fopen("./latency.txt", "w");
  if (latencyFile == NULL) {
    perror("Failed to open file");
    exit(EXIT_FAILURE);
  }

  while (1) {

     // Measure latency
    double start_time = getCurrentTimeMillis();

    // Generate random payload
    int payload = generateRandomNumber(0, 9999);
    // Add payload to the frame
    memcpy(frame + sizeof(struct ether_header) + sizeof(struct iphdr) +
               sizeof(struct udphdr),
           &payload, sizeof(int));

    // Measure latency
    //double start_time = get_time_milliseconds();

    // Send the frame
    if (sendto(sockfd, frame,
               sizeof(struct ether_header) + sizeof(struct iphdr) +
                   sizeof(struct udphdr) + sizeof(int),
               0, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll)) == -1) {
      perror("Failed to send frame");
      exit(EXIT_FAILURE);
    }

    
    printf("Frame sent successfully with payload: %d\n", payload);


    // Measure end time
    double end_time = getCurrentTimeMillis();

    // Calculate latency
    double latency = end_time - start_time;

    // Write latency to file
    fprintf(latencyFile, "%.2f\n", latency);
    fflush(
        latencyFile); // Flush the file stream to ensure immediate write to disk


    printf("Latency: %.2f ms\n", latency);

    sleep(1); // Delay before sending the next frame
    // Add payload to the frame
  }

  // Close the latency file
  fclose(latencyFile);

  // close file
  close(sockfd);

  return 0;
}




