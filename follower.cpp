#include <arpa/inet.h>
#include <iostream>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <network.cpp>

using namespace std;
int sockfd;

int main(int argc, char **argv) {

  string ip;
  int port;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--ip" && i + 1 < argc) {
      ip = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = stoi(argv[++i]);
    }
  }

  TruckInfo truck(ip, port);

  if (init_socket(truck) == -1)
    return -1;

  while (true) {
    struct sockaddr_in6 cliaddr;
    socklen_t len = sizeof(cliaddr);
    int connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
    if (connfd < 0) {
      cerr << "Accept failed" << endl;
      continue;
    }

    char buffer[1024];
    int n = recv(connfd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
      buffer[n] = '\0';
      string command(buffer);
      string response = truck.processCmd(command);
      send(connfd, response.c_str(), response.size(), 0);
    }
    close(connfd);
  }
  close(sockfd);

  return 0;
}
