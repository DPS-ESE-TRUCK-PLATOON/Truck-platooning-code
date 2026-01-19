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
    // switch (truck.state) {
    // case Delinked:
      
    //   break;
    // case Linked:
      
    //   break;
    // }

    command = read_item_from_q(&in_queue_mut, &incoming);

    // TODO: add big switch statement for possible packets and handle them
    // appropriately state machine of state machiens goes her
    if (command != "") {
      response = truck.processCmd(command);
      //std::cout << "response is " << response << std::endl;
      out_q_push(&out_queue_mut, &outgoing, response);
    }
  }
  close(sockfd);
  network.join();
  return 0;
}
