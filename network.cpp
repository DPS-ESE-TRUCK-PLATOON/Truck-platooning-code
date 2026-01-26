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
      //if (!outgoing->empty())
        fds[nfds].events |= POLLOUT;
      nfds++;
    }
    //std::cout << "blocking till something" << std::endl;
    int ret = poll(fds, nfds, -1); // block until something happens
    //std::cout << "something" << std::endl;
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
        //std::cout << "Client connected\n";
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
          //std::cout << "Client disconnected\n";
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
