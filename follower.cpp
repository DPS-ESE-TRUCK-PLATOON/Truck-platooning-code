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
    // this is only safe because the channel is single consumer (this thread)
    // so we cannot learn that it is not empty, then it becomes empty before we
    // lock
    if (!incoming.empty()) {
      if (in_queue_mut.try_lock()) {
        // TODO: break out into function to use simply, returns string
        command = incoming.front();
        incoming.pop();
        in_queue_mut.unlock();

        // TODO: add big switch statement for possible packets and handle them
        // appropriately
        response = truck.processCmd(command);

        // TODO: break out to function so use is easier from other places, takes
        // string
        out_queue_mut.lock();
        outgoing.push(response);
        out_queue_mut.unlock();
      }
    }
  }
  close(sockfd);
  network.join();
  return 0;
}
