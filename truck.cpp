#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using std::string;

enum State { Linked, Delinked };

class location {
public:
  float x;
  float y;
};

struct TruckInfo {
  string ipv6addr;
  int port;
  int position;
  State state;
  string cmd;
  int pos;
  location loc;
  float speed;
  float heading;
  std::vector<std::tuple<time_t, location, float, float>> location_of_lead;

  TruckInfo(const string &addr, int p)
    : ipv6addr(addr), port(p), position(-1), state(Delinked) {}

  string processCmd(const string &command) {
    std::istringstream ss(command);

    ss >> cmd;
    std::cout << cmd << std::endl;
    if (cmd == "LINK") {
      ss >> pos;
      if (state == Linked) {
        return "ERROR Already linked";
      }
      state = Linked;
      position = pos;
      return "ACK LINK " + std::to_string(position);
    } else if (cmd == "DELINK") {
      if (state != Linked) {
        return "ERROR Already delinked";
      }
      state = Delinked;
      position = -1;
      return "ACK DELINK";
    } else {
      return "ERROR Unknown command";
    }
  }
  
  void platoonStandard(time_t current_time) {
    std::tuple test = location_of_lead.front();
    // we do not have an actual server, we just simulate things here
    std::cout << "truck: " << position << " at: x-" << loc.x << " y-" << loc.y
              << " speed: " << speed << " heading: " << heading << std::endl;
    
  }
};
