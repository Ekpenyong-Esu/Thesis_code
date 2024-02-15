#include <arpa/inet.h>
#include <netinet/in.h>
#include <opencv2/opencv.hpp>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define BUFFER_SIZE 1500

pthread_mutex_t communication_latency_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ThreadData {
  int sockfd;
  FILE *processing_latency_file;
  FILE *communication_latency_file;
  cv::Mat received_frame; // Mutex-protected received frame
  pthread_mutex_t frame_mutex;
};

cv::Mat decodeVideoFrame(const uint8_t *data, size_t size);
double getCurrentTimeMillis();
void *receiveTask(void *data);
void *processTask(void *data);

  int main() {
  int sockfd;
  struct sockaddr_in addr;
  int port = 100; // Use the same port as the sender code
  const char *ip_address =
      "192.168.191.234"; // Use the same IP address as the sender code

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

  // Initialize thread data
  struct ThreadData threadData;
  threadData.sockfd = sockfd;
  threadData.processing_latency_file = processing_latency_file;
  threadData.communication_latency_file = communication_latency_file;
  pthread_mutex_init(&threadData.frame_mutex, NULL); // Initialize the mutex

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

  // Destroy the mutex
  pthread_mutex_destroy(&threadData.frame_mutex);

  // Close the files
  fclose(processing_latency_file);
  fclose(communication_latency_file);

  // Close the socket
  close(sockfd);

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
  int sockfd = threadData->sockfd;
  FILE *communication_latency_file = threadData->communication_latency_file;

  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO);
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

  while (1) {
    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(sockfd, &readSet);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 10000; // 10 milliseconds

    double communication_start_time = getCurrentTimeMillis();

    int ready = select(sockfd + 1, &readSet, NULL, NULL, &timeout);

    if (ready > 0 && FD_ISSET(sockfd, &readSet)) {
      char buffer[BUFFER_SIZE];
      ssize_t recvLen = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, NULL, NULL);
      if (recvLen == -1) {
        perror("Failed to receive packet");
        close(sockfd);
        exit(EXIT_FAILURE);
      }

      // Decode the received video frame
      cv::Mat received_frame =
          decodeVideoFrame(reinterpret_cast<uint8_t *>(buffer), recvLen);

      double communication_end_time = getCurrentTimeMillis();

      double communication_latency =
          communication_end_time - communication_start_time; // in milliseconds

      // Lock the mutex before updating the shared frame
      pthread_mutex_lock(&threadData->frame_mutex);
      threadData->received_frame = received_frame.clone();
      pthread_mutex_unlock(&threadData->frame_mutex);

      fprintf(communication_latency_file, "%.2f\n", communication_latency);
      fflush(communication_latency_file);
    }
  }

  return NULL;
}

void *processTask(void *data) {
  struct ThreadData *threadData = (struct ThreadData *)data;
  FILE *processing_latency_file = threadData->processing_latency_file;

  struct sched_param param;
  param.sched_priority = sched_get_priority_max(SCHED_FIFO) - 1;
  pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);

  while (1) {
    double processing_start_time = getCurrentTimeMillis();
    // Lock the mutex before accessing the shared frame
    pthread_mutex_lock(&threadData->frame_mutex);
    cv::Mat received_frame = threadData->received_frame.clone();
    pthread_mutex_unlock(&threadData->frame_mutex);

    // Display the received frame (replace with actual display logic)
    cv::imshow("Received Frame", received_frame);
    cv::waitKey(1);

    // Save processing latency to file
    double processing_end_time = getCurrentTimeMillis();

    double processing_latency = processing_end_time - processing_start_time;
    fprintf(processing_latency_file, "%.2f\n", processing_latency);
    fflush(processing_latency_file);
  }

  return NULL;
}