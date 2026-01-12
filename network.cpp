#include <arpa/inet.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <truck.cpp>
#include <unistd.h>

int init_socket(TruckInfo truck) {
  int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (sockfd < 0) {
    std::cerr << "Socket creation failed" << std::endl;
    return -1;
  }

  struct sockaddr_in6 servaddr;
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_port = htons(truck.port);
  servaddr.sin6_addr = in6addr_any;

  if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    std::cerr << "Bind failed" << std::endl;
    return -1;
  }

  if (listen(sockfd, 5) < 0) {
    std::cerr << "Listen failed" << std::endl;
    return -1;
  }

  std::cout << "Truck listening on [" << truck.ipv6addr << "]:" << truck.port
            << std::endl;

  return 1;
};

void networ_thread(std::mutex out_queue_mut, std::mutex in_queue_mut,
                   std::queue<std::string> incoming,
                   std::queue<std::string> outgoing, int sockfd) {
  while (true) {
    struct sockaddr_in6 cliaddr;
    socklen_t len = sizeof(cliaddr);
    int connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
    if (connfd < 0) {
      std::cerr << "Accept failed" << std::endl;
      continue;
    }

    char buffer[1024];
    int n = recv(connfd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
      buffer[n] = '\0';
      string command(buffer);
      in_queue_mut.lock();
      incoming.push(command);
      in_queue_mut.unlock();

      while (outgoing.empty())
        ;

      out_queue_mut.lock();
      string response = outgoing.front();
      outgoing.pop();
      out_queue_mut.unlock();

      // string response = truck.processCmd(command);
      send(connfd, response.c_str(), response.size(), 0);
    }
    close(connfd);
  }
}
