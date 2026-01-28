#include "decoder.hpp"
#include "protocol.hpp"
#include "truck.cpp"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <queue>
#include <unistd.h>

int sockfd;

std::mutex lead_out_mut, lead_in_mut, behind_out_mut, front_in_mut;
std::queue<std::vector<uint8_t>> lead_out_queue, behind_out_queue;
std::queue<proto::DecodedMessage> lead_in_queue, front_in_queue;

void lead_out(std::vector<uint8_t> item) {
  std::lock_guard<std::mutex> lock(lead_out_mut);
  lead_out_queue.push(item);
}

void behind_out(std::vector<uint8_t> item) {
  std::lock_guard<std::mutex> lock(behind_out_mut);
  behind_out_queue.push(item);
}

bool lead_in(proto::DecodedMessage &out) {
  // returns true if there is input returned on the pointer
  std::lock_guard<std::mutex> lock(lead_in_mut);
  if (lead_in_queue.empty())
    return false;
  out = std::move(lead_in_queue.front());
  lead_in_queue.pop();
  return true;
}

bool front_in(proto::DecodedMessage &out) {
  // returns true if there is input that is returned on the pointer
  std::lock_guard<std::mutex> lock(front_in_mut);
  if (front_in_queue.empty())
    return false;
  out = std::move(front_in_queue.front());
  front_in_queue.pop();
  return true;
}

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
void out_q_push(std::mutex *out_lock, std::queue<std::string> *out_q,
                std::string item) {
  out_lock->lock();
  out_q->push(item);
  out_lock->unlock();
}
// sockets need to be initialized for each peer (maybe make this generic)
// there will be one front truck peer, one back, and the lead and back can be
// initialized initially, but front must be initialized after the data for the
// front peer it given by the lead truck, definitely add ports as variables in
// this file
int init_socket_front_truck(in6_addr ip, uint16_t port) {
  sockfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (sockfd < 0) {
    std::cerr << "Socket creation failed" << std::endl;
    return -1;
  }
  struct sockaddr_in6 servaddr;
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_port = port;
  servaddr.sin6_addr = ip;
}

// using sockets with multiple threads is safe if they are only strictly
// sending/receiving no mixing
void in_thread() {}
void out_thread() {}

int init_socket(Truck truck) {
  sockfd = socket(AF_INET6, SOCK_STREAM, 0);
  if (sockfd < 0) {
    std::cerr << "Socket creation failed" << std::endl;
    return -1;
  }
  struct sockaddr_in6 servaddr;
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_port = htons(truck.port);
  servaddr.sin6_addr = in6addr_any;
  int flags = fcntl(sockfd, F_GETFL, 0);
  fcntl(sockfd, F_SETFL, flags | O_NONBLOCK);

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
  socklen_t len = sizeof(cliaddr);
  int n;
  int connfd = -1;
  pollfd fds[2];
  while (true) {
    int nfds = 0;
    // Listening socket
    fds[nfds].fd = sockfd;
    fds[nfds].events = POLLIN;
    nfds++;
    // Client socket (if connected)
    if (connfd >= 0) {
      fds[nfds].fd = connfd;
      fds[nfds].events = POLLIN;
      // enable POLLOUT only if we have data to send
      // if (!outgoing->empty())
      fds[nfds].events |= POLLOUT;
      nfds++;
    }
    // std::cout << "blocking till something" << std::endl;
    int ret = poll(fds, nfds, -1); // block until something happens
    // std::cout << "something" << std::endl;
    if (ret < 0) {
      perror("poll");
      continue;
    }

    int idx = 0;

    // New incoming connection
    if (fds[idx].revents & POLLIN) {
      sockaddr_in6 cliaddr;
      socklen_t len = sizeof(cliaddr);

      int newfd = accept(sockfd, (sockaddr *)&cliaddr, &len);
      if (newfd >= 0) {
        fcntl(newfd, F_SETFL, O_NONBLOCK);
        connfd = newfd;
        // std::cout << "Client connected\n";
      }
    }
    idx++;

    // Client socket events
    if (connfd >= 0) {
      // Incoming data
      if (fds[idx].revents & POLLIN) {
        ssize_t n = recv(connfd, buffer, sizeof(buffer) - 1, 0);
        if (n > 0) {
          buffer[n] = '\0';

          std::lock_guard<std::mutex> lock(*in_queue_mut);
          incoming->push(buffer);
        } else if (n == 0) {
          // std::cout << "Client disconnected\n";
          close(connfd);
          connfd = -1;
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
          perror("recv");
        }
      }

      // Outgoing data
      if (fds[idx].revents & POLLOUT) {
        std::string response;
        {
          std::lock_guard<std::mutex> lock(*out_queue_mut);
          if (!outgoing->empty()) {
            response = outgoing->front();
            outgoing->pop();
          }
        }

        if (!response.empty()) {
          ssize_t sent = send(connfd, response.c_str(), response.size(), 0);
          if (sent < 0 && errno != EAGAIN) {
            perror("send");
          }
        }
      }
    }
  }
}
