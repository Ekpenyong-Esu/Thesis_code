#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#define FRAME_SIZE 1500

// Global variables
cv::Mat videoFrame;
pthread_mutex_t frameMutex = PTHREAD_MUTEX_INITIALIZER;

struct ether_header eth_header;
struct iphdr ip_header;
struct udphdr udp_header;
char frame[FRAME_SIZE];

int sockfd;        // Declare sockfd globally
FILE *latencyFile; // Declare latencyFile globally


double getCurrentTimeMillis() {
  return (double)clock() / CLOCKS_PER_SEC * 1000;
}

cv::Mat captureVideoFrame(cv::VideoCapture &cap) {
  cv::Mat frame;
  cap >> frame;
  return frame;
}

void *captureTask(void *arg) {
  // Set capture task to higher priority
  struct sched_param capture_param;
  capture_param.sched_priority =
      sched_get_priority_max(SCHED_FIFO); // Higher priority
  if (sched_setscheduler(0, SCHED_FIFO, &capture_param) == -1) {
    perror("Failed to set capture task priority");
    exit(EXIT_FAILURE);
  }

  cv::VideoCapture cap(0);
  if (!cap.isOpened()) {
    perror("Failed to open video capture");
    exit(EXIT_FAILURE);
  }

  while (1) {

    
    cv::Mat currentFrame = captureVideoFrame(cap);

    pthread_mutex_lock(&frameMutex);
    videoFrame = currentFrame.clone();
    pthread_mutex_unlock(&frameMutex);

    usleep(10000); // 10 milliseconds sleep for capture rate control
  }

  // Cleanup (optional)
  cap.release();
  pthread_exit(NULL);
}

void *sendTask(void *arg) {
  // Set send task to lower priority
  struct sched_param send_param;
  send_param.sched_priority =
      sched_get_priority_max(SCHED_FIFO) - 1; // Lower priority
  if (sched_setscheduler(0, SCHED_FIFO, &send_param) == -1) {
    perror("Failed to set send task priority");
    exit(EXIT_FAILURE);
  }

  while (1) {
    // Measure latency - Start time
    double start_time = getCurrentTimeMillis();

    pthread_mutex_lock(&frameMutex);
    cv::Mat currentFrame = videoFrame.clone();
    pthread_mutex_unlock(&frameMutex);

    // Measure latency - Start time
    // double start_time = getCurrentTimeMillis();  // Duplicate line, remove
    // this

    std::vector<uchar> buffer;
    cv::imencode(".jpg", currentFrame, buffer);
    ip_header.tot_len =
        htons(sizeof(struct iphdr) + sizeof(struct udphdr) + buffer.size());

    memcpy(frame + sizeof(struct ether_header) + sizeof(struct iphdr) +
               sizeof(struct udphdr),
           buffer.data(), buffer.size());

    if (sendto(sockfd, frame,
               sizeof(struct ether_header) + sizeof(struct iphdr) +
                   sizeof(struct udphdr) + buffer.size(),
               0, (struct sockaddr *)&sa, sizeof(struct sockaddr_ll)) == -1) {
      perror("Failed to send frame");
      exit(EXIT_FAILURE);
    }

    // Measure latency - End time
    double end_time = getCurrentTimeMillis();

    // Calculate and print latency
    double latency = end_time - start_time;

    // Write latency to file
    fprintf(latencyFile, "%.2f\n", latency);
    fflush(
        latencyFile); // Flush the file stream to ensure immediate write to disk

    printf("Frame sent successfully\n");

    usleep(10000); // 10 milliseconds sleep for send rate control
  }

  // Cleanup (optional)
  // close(sockfd);  // Remove this line, close the socket in the cleanup
  // section of main() fclose(latencyFile);  // Remove this line, close the file
  // in the cleanup section of main()
  pthread_exit(NULL);
}

int main() {
  // Create a socket
  sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd == -1) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }

  memset(&sa, 0, sizeof(struct sockaddr_ll));
  sa.sll_family = AF_PACKET;
  sa.sll_protocol = htons(ETH_P_ALL);
  sa.sll_ifindex =
      if_nametoindex("enp0s31f6"); // Replace with your interface name

  // Initialize Ethernet header
  struct ether_addr source_mac;
  struct ether_addr dest_mac;
  ether_aton_r("3C:52:82:43.B2:29", &source_mac);
  memcpy(eth_header.ether_shost, &source_mac, ETH_ALEN);
  ether_aton_r("3C:52:82:43:A2:C8", &dest_mac);
  memcpy(eth_header.ether_dhost, &dest_mac, ETH_ALEN);
  eth_header.ether_type = htons(ETH_P_IP); // IP packet

  // Initialize IP header
  ip_header.version = 4;
  ip_header.ihl = 5;
  ip_header.tos = 0;
  ip_header.id = 0;
  ip_header.frag_off = 0;
  ip_header.protocol = IPPROTO_UDP;
  ip_header.check = 0; // Set the checksum to 0 for simplicity
  ip_header.saddr =
      inet_addr("192.168.4.45"); // Replace with your source IP address
  ip_header.daddr =
      inet_addr("192.168.4.25"); // Replace with your destination IP address

  // Initialize UDP header
  udp_header.source = htons(200); // Replace with the desired source port
  udp_header.dest = htons(800);   // Replace with the desired destination port
  udp_header.check = 0;           // Set the checksum to 0 for simplicity

  // Construct the frame with IP, UDP, and video frame
  memcpy(frame, &eth_header, sizeof(struct ether_header));
  memcpy(frame + sizeof(struct ether_header), &ip_header, sizeof(struct iphdr));
  memcpy(frame + sizeof(struct ether_header) + sizeof(struct iphdr),
         &udp_header, sizeof(struct udphdr));

  // Open the file for writing latency data
  latencyFile = fopen("./latency.txt", "w");
  if (latencyFile == NULL) {
    perror("Failed to open file");
    exit(EXIT_FAILURE);
  }

  // Initialize mutex
  pthread_mutex_init(&frameMutex, NULL);

  // Create threads for capture and send tasks
  pthread_t captureThread, sendThread;
  pthread_create(&captureThread, NULL, captureTask, NULL);
  pthread_create(&sendThread, NULL, sendTask, NULL);

  // Main thread can also perform some other tasks or wait for threads to finish

  // Wait for threads to finish (optional)
  pthread_join(captureThread, NULL);
  pthread_join(sendThread, NULL);

  // Cleanup
  close(sockfd);
  fclose(latencyFile);
  pthread_mutex_destroy(&frameMutex);

  return 0;
}
