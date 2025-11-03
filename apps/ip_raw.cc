#include "socket.hh"
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <sys/socket.h>

using namespace std;

int main()
{
  // construct an Internet or user datagram here, and send using a RawSocket
  // Create raw socket for protocol 5
  int sd = socket(AF_INET, SOCK_RAW, 5); // protocol 5
  if (sd < 0) {
      perror("socket");
      exit(EXIT_FAILURE);
  }
  const char* payload = "Hello Protocol 5";
  
  // Destination address
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost
  // Send the payload
  if (sendto(sd, payload, strlen(payload), 0,
             (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      perror("sendto");
  } else {
      std::cout << "Datagram sent successfully!" << std::endl;
  }
  close(sd);

   // Create raw socket for protocol 17
  int sock = socket(AF_INET, SOCK_RAW, 17);
  if (sock < 0) {
        perror("socket");
        exit(1);
  }

  // Destination address
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = inet_addr("127.0.0.1"); // localhost

  //
  char packet[4096];
  
  struct udphdr* udp = (struct udphdr*)packet;
  payload = "Hello Protocol 17";
  int payload_len = strlen(payload);
  udp->source = htons(12345); // source port
  udp->dest   = htons(5000);  // destination port
  udp->len    = htons(sizeof(struct udphdr) + payload_len);
  udp->check  = 0; 
  
  memcpy(packet + sizeof(struct udphdr), payload, payload_len);
  
  // Send the payload
  if (sendto(sock, packet, sizeof(struct udphdr) + payload_len, 0,
             (struct sockaddr *)&sin, sizeof(sin)) < 0) {
      perror("sendto");
  } else {
      std::cout << "Datagram sent successfully!" << std::endl;
  }
  close(sd);
}
