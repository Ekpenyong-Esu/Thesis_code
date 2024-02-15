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
#include <time.h>
#include <unistd.h>

#define FRAME_SIZE 1500

struct ThreadData {
  int sockfd_receive;
  int sockfd_send;
  FILE *communication_latency_file;
  pthread_mutex_t frame_mutex;
  cv::Mat received_frame; // shared data protected by mutex
};

double getCurrentTimeMillis();
cv::Mat decodeVideoFrame(const uint8_t *data, size_t size);
void *receiveTask(void *data);
void *processTask(void *data);

  int main() {
  int sockfd_receive;
  struct sockaddr_ll sa;
  pthread_mutex_t frame_mutex = PTHREAD_MUTEX_INITIALIZER;
  char frame[FRAME_SIZE];

  // Create a raw socket
  sockfd_receive = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
  if (sockfd_receive == -1) {
    perror("Failed to create socket");
    exit(EXIT_FAILURE);
  }

  // Set the network interface index
  memset(&sa, 0, sizeof(struct sockaddr_ll));
  sa.sll_family = AF_PACKET;
  sa.sll_protocol = htons(ETH_P_ALL);
  sa.sll_ifindex =
      if_nametoindex("enp0s31f6"); // Replace "enp0s3" with your interface name

  // Bind the receiving socket to the network interface
  if (bind(sockfd_receive, (struct sockaddr *)&sa,
           sizeof(struct sockaddr_ll)) == -1) {
    perror("Failed to bind socket");
    close(sockfd_receive);
    exit(EXIT_FAILURE);
  }

  // Open file for saving communication latency
  FILE *communication_latency_file =
      fopen("communication_latency_intermediate.txt", "w");
  if (communication_latency_file == NULL) {
    perror("Failed to open latency file");
    close(sockfd_receive);
    exit(EXIT_FAILURE);
  }

  // Initialize thread data
  struct ThreadData threadData;
  threadData.sockfd_receive = sockfd_receive;
  threadData.communication_latency_file = communication_latency_file;
  threadData.frame_mutex = frame_mutex;

  // Create threads
  pthread_t receiveThreadId, processThreadId;
  pthread_create(&receiveThreadId, NULL, receiveTask, &threadData);
  pthread_create(&processThreadId, NULL, processTask, &threadData);

  // Sleep for a while to allow threads to run (replace with your main logic)
  sleep(10);

  // Cancel threads
  pthread_cancel(receiveThreadId);
  pthread_cancel(processThreadId);

  // Join threads
  pthread_join(receiveThreadId, NULL);
  pthread_join(processThreadId, NULL);

  // Close the file
  fclose(communication_latency_file);

  // Close the socket
  close(sockfd_receive);

  return 0;
}

double getCurrentTimeMillis() {
  return (double)clock() / CLOCKS_PER_SEC * 1000;
}

cv::Mat decodeVideoFrame(const uint8_t *data, size_t size) {
  cv::Mat decoded_frame = cv::imdecode(
      cv::Mat(1, size, CV_8U, const_cast<uint8_t *>(data)), cv::IMREAD_COLOR);
  return decoded_frame;
}

void *receiveTask(void *data) {
  struct ThreadData *threadData = (struct ThreadData *)data;
  int sockfd_receive = threadData->sockfd_receive;
  FILE *communication_latency_file = threadData->communication_latency_file;

  while (1) {
    // Measure start time for communication latency
    double communication_start_time = getCurrentTimeMillis();

    char frame[FRAME_SIZE];
    ssize_t recv_len = recv(sockfd_receive, frame, FRAME_SIZE, 0);
    if (recv_len == -1) {
      perror("Failed to receive frame");
      close(sockfd_receive);
      fclose(communication_latency_file);
      exit(EXIT_FAILURE);
    }

    // Measure end time for communication latency
    double communication_end_time = getCurrentTimeMillis();

    // Calculate communication latency
    double communication_latency =
        communication_end_time - communication_start_time;

    // Protect the shared data (received frame) using a mutex
    pthread_mutex_lock(&threadData->frame_mutex);
    threadData->received_frame = decodeVideoFrame(frame, recv_len);
    pthread_mutex_unlock(&threadData->frame_mutex);

    fprintf(communication_latency_file, "%.2f\n", communication_latency);
    fflush(communication_latency_file);
  }

  return NULL;
}

void *processTask(void *data) {
  struct ThreadData *threadData = (struct ThreadData *)data;

  // Open the socket for sending outside the loop
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

  while (1) {

    // processing latency
    double processing_start_time = getCurrentTimeMillis();

    // Protect the shared data (received frame) using a mutex
    pthread_mutex_lock(&threadData->frame_mutex);
    // Assuming the received frame is in threadData->received_frame
    cv::Mat frame_to_send = threadData->received_frame.clone();
    pthread_mutex_unlock(&threadData->frame_mutex);

    // since the frame is an image
    std::vector<uint8_t> frame_data;
    cv::imencode(".jpg", frame_to_send, frame_data);

    ssize_t send_len =
        sendto(send_sockfd, frame_data.data(), frame_data.size(), 0,
               (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (send_len == -1) {
      perror("Failed to send frame");
      close(send_sockfd);
      exit(EXIT_FAILURE);
    }

    // processing latency
    double processing_end_time = getCurrentTimeMillis();
    double processing_latency = processing_end_time - processing_start_time;

    fprintf(threadData->processing_latency_file, "%.2f\n", processing_latency);
    fflush(threadData->processing_latency_file);
  }

  // Close the socket after the loop
  close(send_sockfd);

  return NULL;
}