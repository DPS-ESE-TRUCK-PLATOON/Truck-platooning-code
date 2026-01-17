#include "network.cpp"
#include <arpa/inet.h>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <thread>
#include <unistd.h>

std::mutex out_queue_mut;
std::mutex in_queue_mut;
std::queue<std::string> incoming;
std::queue<std::string> outgoing;

int main(int argc, char **argv) {

  string ip;
  int port;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--ip" && i + 1 < argc) {
      ip = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    }
  }

  TruckInfo truck(ip, port);

  if (init_socket(truck) == -1)
    return -1;
  std::thread network(*network_thread, &out_queue_mut, &in_queue_mut, &incoming,
                      &outgoing, sockfd);
  string command;
  string response;
  while (true) {

    if (in_queue_mut.try_lock()) {
      if (!incoming.empty()) {
        command = incoming.front();
        response = truck.processCmd(command);
        incoming.pop();
        in_queue_mut.unlock();
        out_queue_mut.lock();
        outgoing.push(response);
        out_queue_mut.unlock();
      } else {
        in_queue_mut.unlock();
      }
    }
  }
  close(sockfd);
  network.join();
  return 0;
}
