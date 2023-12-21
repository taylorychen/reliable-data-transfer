#include <arpa/inet.h>
#include <deque>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

#include "utils.h"

using namespace std;

int main() {
  int listen_sockfd, send_sockfd;
  struct sockaddr_in server_addr, client_addr_from, client_addr_to;
  struct packet recv_pkt;
  socklen_t addr_size = sizeof(client_addr_from);
  unsigned short expected_seq_num = 0;
  // int recv_len;
  struct packet ack_pkt;

  // Create a UDP socket for sending
  send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (send_sockfd < 0) {
    perror("Could not create send socket");
    return 1;
  }

  // Create a UDP socket for listening
  listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (listen_sockfd < 0) {
    perror("Could not create listen socket");
    return 1;
  }

  // Configure the server address structure
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(SERVER_PORT);
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

  // Bind the listen socket to the server address
  if (bind(listen_sockfd, (struct sockaddr *)&server_addr,
           sizeof(server_addr)) < 0) {
    perror("Bind failed");
    close(listen_sockfd);
    return 1;
  }

  // Configure the client address structure to which we will send ACKs
  memset(&client_addr_to, 0, sizeof(client_addr_to));
  client_addr_to.sin_family = AF_INET;
  client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
  client_addr_to.sin_port = htons(CLIENT_PORT_TO);

  // Open the target file for writing (always write to output.txt)
  FILE *fp = fopen("output.txt", "wb");

  unordered_map<unsigned short, struct packet> buffer;
  unordered_set<unsigned short> seen_packets;
  const size_t buffer_capacity = 10;

  char last = 0;
  while (last != 1) {
    ssize_t recv_size =
        recvfrom(listen_sockfd, &recv_pkt, sizeof(recv_pkt), 0,
                 (struct sockaddr *)&client_addr_from, &addr_size);
    if (recv_size < 0) {
      perror("recvfrom failed");
      close(listen_sockfd);
      close(send_sockfd);
      return 1;
    }
    // printRecv(&recv_pkt);

    // drop repeat packets
    if (seen_packets.count(recv_pkt.seqnum) > 0) {
      // printf("Drop repeat: %d\n", recv_pkt.seqnum);
      build_packet(&ack_pkt, 0, expected_seq_num, recv_pkt.last, 1, 0, NULL);
      sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0,
             (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
      // printSend(&ack_pkt, 0);
      continue;
    } else {
      seen_packets.insert(recv_pkt.seqnum);
    }

    if (expected_seq_num != recv_pkt.seqnum) {
      // cumulative ACK
      // printf("last: %c\n", recv_pkt.last);
      build_packet(&ack_pkt, 0, expected_seq_num, 0, 1, 0, NULL);
      sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0,
             (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
      // printSend(&ack_pkt, 0);

      // if space in buffer, buffer out of order packet
      if (buffer.size() < buffer_capacity) {
        buffer[recv_pkt.seqnum] = recv_pkt;
      }
      // remove from seen if we drop packet
      else {
        if (seen_packets.count(recv_pkt.seqnum) > 0) {
          seen_packets.erase(recv_pkt.seqnum);
        }
      }
      continue;
    }
    last = recv_pkt.last;

    buffer[recv_pkt.seqnum] = recv_pkt; // add to buffer to start while loop

    // write as many buffered packets as possible, remove after write
    while (buffer.count(expected_seq_num) > 0) {
      struct packet *pkt = &(buffer[expected_seq_num]);

      fwrite(pkt->payload, sizeof(char), pkt->length, fp);

      // unsigned int length = pkt->length;
      buffer.erase(expected_seq_num);

      expected_seq_num = expected_seq_num + 1;
    }

    build_packet(&ack_pkt, 0, expected_seq_num, last, 1, 0, NULL);
    sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0,
           (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
    printSend(&ack_pkt, 0);
  }

  fclose(fp);
  close(listen_sockfd);
  close(send_sockfd);
  return 0;
}
