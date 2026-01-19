#include <iostream>
#include <sstream>
#include <string>

using std::string;

enum State { Linked, Delinked } state;

struct TruckInfo {
  string ipv6addr;
  int port;
  int position;
  State state;
  string cmd;
  int pos;

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
};
