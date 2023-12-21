#include <arpa/inet.h>
#include <deque>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "utils.h"

using namespace std;

int main(int argc, char *argv[]) {
  int listen_sockfd, send_sockfd;
  struct sockaddr_in client_addr, server_addr_to, server_addr_from;
  socklen_t addr_size = sizeof(server_addr_to);
  struct timeval tv;
  struct packet pkt;
  struct packet ack_pkt;
  // char buffer[PAYLOAD_SIZE];
  unsigned short seq_num = 0;
  unsigned short ack_num = 0;
  char last = 0;
  char ack = 0;

  // read filename from command line argument
  if (argc != 2) {
    printf("Usage: ./client <filename>\n");
    return 1;
  }
  char *filename = argv[1];

  // Create a UDP socket for listening
  listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (listen_sockfd < 0) {
    perror("Could not create listen socket");
    return 1;
  }

  // Create a UDP socket for sending
  send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (send_sockfd < 0) {
    perror("Could not create send socket");
    return 1;
  }

  // Configure the server address structure to which we will send data
  memset(&server_addr_to, 0, sizeof(server_addr_to));
  server_addr_to.sin_family = AF_INET;
  server_addr_to.sin_port = htons(SERVER_PORT_TO);
  server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

  // Configure the client address structure
  memset(&client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(CLIENT_PORT);
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // Bind the listen socket to the client address
  if (bind(listen_sockfd, (struct sockaddr *)&client_addr,
           sizeof(client_addr)) < 0) {
    perror("Bind failed");
    close(listen_sockfd);
    return 1;
  }

  // Open file for reading
  FILE *fp = fopen(filename, "rb");
  if (fp == NULL) {
    perror("Error opening file");
    close(listen_sockfd);
    close(send_sockfd);
    return 1;
  }

  tv.tv_sec = 0;
  tv.tv_usec = 250000;
  setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  unsigned int bytes_read;
  socklen_t server_addr_from_len = sizeof(server_addr_from);
  int cwnd = 1;
  int ssthresh = 15;
  int packets_in_flight = 0;
  int fast_recovery_ack = -1;
  int duplicate_ack_count = 0;
  int congestion_counter = 0;
  int ackedpackets;
  deque<struct packet> window;

  while (!feof(fp) || packets_in_flight > 0) {
    for (int i = packets_in_flight; i < cwnd; i++) {

      // NOTE: this is weird but avoids an extra payload copy
      // create packet with no payload
      build_packet(&pkt, seq_num, ack_num, last, ack, 0, NULL);
      window.push_back(pkt); // pkt gets copied on push_back
      // fread directly into payload and update length
      bytes_read = fread(window.back().payload, sizeof(char), PAYLOAD_SIZE, fp);
      window.back().length = bytes_read;
      seq_num = seq_num + 1;

      if (bytes_read < PAYLOAD_SIZE) {
        last = 1;
        window.back().last = last;
        break;
      }
    }

    for (size_t i = packets_in_flight; i < window.size(); i++) {
      // printf("packets in flight: %d cwnd: %d ssthresh: %d\n",
      // packets_in_flight, cwnd, ssthresh);
      // printSend(&window[i], 0);
      ssize_t sent_size =
          sendto(send_sockfd, &window[i], sizeof(struct packet), 0,
                 (struct sockaddr *)&server_addr_to, addr_size);
      if (sent_size < 0) {
        perror("send failed");
        return 1;
      }
      packets_in_flight++;
      // fprintf(stderr, "Client: sent %ld bytes\n", sent_size);

      // sleep between sends to reduce likelihood of out-of-order arrival
      usleep(150);
    }
    while (packets_in_flight >= cwnd) {
      ssize_t recv_size =
          recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                   (struct sockaddr *)&server_addr_from, &server_addr_from_len);
      if (recv_size < 0) {
        // timeout: retransmit first packet in window
        // printf("TIMEOUT\n");
        // printSend(&window[0], 1);
        ssthresh = max(cwnd / 2, 2);
        cwnd = 1;
        sendto(send_sockfd, &window[0], sizeof(struct packet), 0,
               (struct sockaddr *)&server_addr_to, addr_size);
        continue;
        // break;
      }
      // printRecv(&ack_pkt);
      // fprintf(stderr, "Client: received ACK %d \n", ack_pkt.acknum);

      // fast retransmit
      if (fast_recovery_ack == ack_pkt.acknum) {
        duplicate_ack_count++;
        // fast retransmit
        if (duplicate_ack_count == 3) {
          // printf("Fast Retransmit\n");
          ssthresh = max(cwnd / 2, 2);
          cwnd = ssthresh + 3;
          // printSend(&window[0], 1);
          sendto(send_sockfd, &window[0], sizeof(struct packet), 0,
                 (struct sockaddr *)&server_addr_to, addr_size);
          continue;
        }
        // fast recovery dup acks
        else if (duplicate_ack_count > 3) {
          // printf("fast recovery dup acks\n");
          cwnd++;
          continue;
        }
      }
      // fast recovery new ack
      else if (duplicate_ack_count >= 3 &&
               (fast_recovery_ack != ack_pkt.acknum)) {
        // printf("fast recovery new ack\n");
        duplicate_ack_count = 1;
        cwnd = ssthresh;
        fast_recovery_ack = ack_pkt.acknum;
        ackedpackets = 0;
        for (size_t i = 0; i < window.size(); i++) {
          if (ack_pkt.acknum - 1 == window[i].seqnum) {
            ackedpackets = i + 1;
            // printf("ACKED: %d\n", ackedpackets);
            break;
          }
        }
        if (ackedpackets == 0) {
          continue;
        } else {
          int popcount = 0;
          // printf("POPPING\n");
          while (popcount < ackedpackets) {
            window.pop_front();
            packets_in_flight--;
            popcount++;
          }
        }
        continue;
      }
      // regular case
      else {
        // printf("regular\n");
        duplicate_ack_count = 1;
        fast_recovery_ack = ack_pkt.acknum;
      }

      // slow start
      if (cwnd <= ssthresh) {
        cwnd++;
        congestion_counter = 0;
        // printf("slow start\n");
      }
      // congestion avoidance
      else {
        congestion_counter++;
        if (congestion_counter == cwnd) {
          cwnd++;
          congestion_counter = 0;
        }

        // printf("congestion avoidance,cwnd: %d ssthresh: %d congestion
        // counter: "%d\n", cwnd, ssthresh, congestion_counter);
      }

      // discard acks out of window
      ackedpackets = 0;

      for (size_t i = 0; i < window.size(); i++) {
        if (ack_pkt.acknum - 1 == window[i].seqnum) {
          ackedpackets = i + 1;
          // printf("ACKED: %d\n", ackedpackets);
          break;
        }
      }
      if (ackedpackets == 0) {
        continue;
      } else {
        int popcount = 0;
        // printf("POPPING\n");
        while (popcount < ackedpackets) {
          window.pop_front();
          packets_in_flight--;
          popcount++;
        }
      }
    }
  }

  fclose(fp);
  close(listen_sockfd);
  close(send_sockfd);
  return 0;
}
