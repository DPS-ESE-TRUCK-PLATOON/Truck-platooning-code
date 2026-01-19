#include "truck.cpp"
#include <arpa/inet.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <unistd.h>

int sockfd;

// possibly move into their own file
std::string read_item_from_q(std::mutex *in_lock,
                             std::queue<std::string> *in_q) {
  // this is only safe because the channel is single consumer (this thread)
  // so we cannot learn that it is not empty, then it becomes empty before we
  // lock it
  string command = "";
  if (!in_q->empty()) {
    in_lock->lock();
    command = in_q->front();
    in_q->pop();
    in_lock->unlock();
  }
  return command;
}
void out_q_push(std::mutex *out_lock, std::queue<std::string> *out_q, std::string item) {
  out_lock->lock();
  out_q->push(item);
  out_lock->unlock();
}

int init_socket(TruckInfo truck) {
  sockfd = socket(AF_INET6, SOCK_STREAM, 0);
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

void network_thread(std::mutex *out_queue_mut, std::mutex *in_queue_mut,
                    std::queue<std::string> *incoming,
                    std::queue<std::string> *outgoing, int sockfd) {
  char buffer[1024];
  string command;
  string response;
  struct sockaddr_in6 cliaddr;
  int connfd;
  socklen_t len = sizeof(cliaddr);
  int n;
  connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
  while (true) {
    // TODO: add things for the other socket to the server not just the truck
    len = sizeof(cliaddr);
    // if there is no connection, create it
    if (connfd < 0) {
      connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
    }
    if (connfd < 0) {
      // TODO: add something here for connection failiure
      std::cerr << "Accept failed" << std::endl;
      continue;
    }
    n = recv(connfd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
      buffer[n] = '\0';
      command = buffer;
      in_queue_mut->lock();
      incoming->push(command);
      in_queue_mut->unlock();
    }
    // if there is a response to be sent, send it
    if (!outgoing->empty()) {
      out_queue_mut->lock();
      response = outgoing->front();
      outgoing->pop();
      out_queue_mut->unlock();
      send(connfd, response.c_str(), response.size(), 0);
    }
    // possibly needs to be added somewhere?
    // close(connfd);
  }
}
