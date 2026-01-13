#include <sstream>
#include <string>

using std::string;

enum State { Idle, Linked, Delinked };
struct TruckInfo {
  string ipv6addr;
  int port;
  int position;
  State state;
  string cmd;
  int pos;

  TruckInfo(const string &addr, int p)
      : ipv6addr(addr), port(p), position(-1), state(Idle) {}

  string processCmd(const string &command) {
    std::istringstream ss(command);

    if (!(ss >> cmd >> pos)) {
      return "ERROR Invalid command format";
    }

    if (cmd == "LINK") {
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
