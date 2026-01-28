#include "decoder.hpp"
#include "network.cpp"
#include "packet-proc.cpp"
#include <arpa/inet.h>
#include <cfenv>
#include <mutex>
#include <netinet/in.h>
#include <queue>
#include <string>
#include <thread>
#include <unistd.h>


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

std::mutex out_queue_mut;
std::mutex in_queue_mut;
std::queue<std::string> incoming;
std::queue<std::string> outgoing;
const double tickrate = 60;

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

  Truck truck(ip, port);

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
        truck.locationOfTruckAhead.emplace_back(com.timeStamp, com.pos, com.speed,
                                            com.heading);
        truck.platoonStandard(1/tickrate);
        break;
      }
    } else {
      truck.platoonStandard(1/tickrate);
    }
  }
  close(sockfd);
  network.join();
  return 0;
}
