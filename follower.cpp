#include "network.cpp"
#include "packet-proc.cpp"
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

enum packet_type { Data, Control };
enum command { Join, Disjoin, Brake };

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
  time_t currentTime;
  while (true) {
    // TODO: shift the command processing out of the truck, make a class for it,
    // switch on it after processing the command
    // after doing this it will be easier to integrate other things
    time(&currentTime);
    command = read_item_from_q(&in_queue_mut, &incoming);
    if (command != "") {
      switch (truck.state) {
      case Delinked:
        response = truck.processCmd(command);
        out_q_push(&out_queue_mut, &outgoing, response);
        break;
      case Linked:
        // this contains the info from the pos packet
        posPacket com =
            processPacket(read_item_from_q(&in_queue_mut, &incoming));
        truck.location_of_lead.emplace_back(com.timeStamp, com.pos, com.speed,
                                            com.heading);
        truck.platoonStandard(currentTime);
        break;
      }
    } else {
      truck.platoonStandard(currentTime);
    }
  }
  close(sockfd);
  network.join();
  return 0;
}
