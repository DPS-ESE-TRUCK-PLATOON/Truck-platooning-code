#include "encoder.hpp"
#include "protocol.hpp"
#include "truck.cpp"
#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <netinet/in.h>
#include <sstream>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <vector>

using namespace std;

struct TruckInfo {
  string ipv6addr;
  int tcp_port;
  int udp_port;
  uint32_t id;
  int position;
};

class PlatoonLeader {
public:
  vector<TruckInfo> platoon;
  uint32_t next_truck_id = 1;
  Truck truck = Truck();

private:
  int godotSocket = -1;
  bool godotConnected = false;

public:
  void sendStateToGodot() {
    if (!godotConnected) {
      cerr << "Not connected to Godot." << endl;
      return;
    }
    string cmd = "STATE " + to_string(truck.getX()) + " " +
                 to_string(truck.getY()) + " " + to_string(truck.getHeading()) +
                 " " + to_string(truck.getAccel());

    sendToGodot(cmd);
  }

  void addTruck(const string &ipv6addr, int tcp_port, int udp_port) {
    auto it = find_if(platoon.begin(), platoon.end(),
                      [&ipv6addr, tcp_port](const TruckInfo &truck) {
                        return truck.ipv6addr == ipv6addr &&
                               truck.tcp_port == tcp_port;
                      });
    if (it != platoon.end()) {
      cout << "Truck already in platoon." << endl;
      return;
    }

    int position = platoon.size() + 1;
    uint32_t truck_id = next_truck_id++;

    platoon.push_back({ipv6addr, tcp_port, udp_port, truck_id, position});

    cout << "Adding truck ID=" << truck_id << " at position " << position
         << " (UDP:" << udp_port << ")" << endl;

    sendInfoToTruck(platoon.back());

    if (position > 1) {
      sendInfoToTruck(platoon[position - 2]);
    }

    if (godotConnected) {
      sendToGodot("LINK " + to_string(position));
    }

    printPlatoon();
  }

  void removeTruck(const string &ipv6addr, int tcp_port) {
    auto it = find_if(platoon.begin(), platoon.end(),
                      [&ipv6addr, tcp_port](const TruckInfo &truck) {
                        return truck.ipv6addr == ipv6addr &&
                               truck.tcp_port == tcp_port;
                      });

    if (it == platoon.end()) {
      cout << "Truck not found in platoon." << endl;
      return;
    }

    int removedPosition = it->position;
    uint32_t removedId = it->id;

    cout << "Removing truck ID=" << removedId << " at position "
         << removedPosition << endl;

    sendRemoveToTruck(*it);
    platoon.erase(it);

    for (size_t i = 0; i < platoon.size(); i++) {
      platoon[i].position = i + 1;
    }

    if (removedPosition == 1 && !platoon.empty()) {
      sendInfoToTruck(platoon[0]);
    } else if (removedPosition > 1 &&
               removedPosition <= (int)platoon.size() + 1) {
      if (removedPosition - 2 < (int)platoon.size()) {
        sendInfoToTruck(platoon[removedPosition - 2]);
      }
    }

    if (godotConnected) {
      sendToGodot("DELINK " + to_string(removedPosition));
    }

    printPlatoon();
  }

  void sendBrake() {
    cout << "Sending BRAKE to all trucks" << endl;
    for (auto &truck : platoon) {
      sendBrakeToTruck(truck);
    }
  }

  void sendRelease() {
    cout << "Sending RELEASE to all trucks" << endl;
    for (auto &truck : platoon) {
      sendReleaseToTruck(truck);
    }
  }

  bool connectToGodot(const string &ipv6addr, int port) {
    godotSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (godotSocket < 0) {
      cerr << "Godot socket creation failed" << endl;
      return false;
    }

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipv6addr.c_str(), &servaddr.sin_addr) <= 0) {
      cerr << "Invalid Godot IPv4 address: " << ipv6addr << endl;
      close(godotSocket);
      return false;
    }

    if (connect(godotSocket, (struct sockaddr *)&servaddr, sizeof(servaddr)) <
        0) {
      cerr << "Connection to Godot " << ipv6addr << ":" << port << " failed"
           << endl;
      close(godotSocket);
      return false;
    }

    godotConnected = true;
    return true;
  }

  void disconnectFromGodot() {
    if (godotSocket >= 0) {
      close(godotSocket);
      godotConnected = false;
    }
  }

  void sendDriveCommand(char key) {
    if (!godotConnected) {
      cerr << "Not connected to Godot." << endl;
      return;
    }

    const double accelStep = 1;
    const float headingStep = 2;

    switch (key) {
    case 'w':
      truck.setAccel(truck.getAccel() + accelStep);
      break;
    case 's':
      truck.setAccel(truck.getAccel() - accelStep);
      break;
    case 'a':
      truck.setHeading(truck.getHeading() - headingStep);
      break;
    case 'd':
      truck.setHeading(truck.getHeading() + headingStep);
      break;
    case 'z':
      truck.setAccel(0.0);
      break;
    case ' ': // emergency brake to all trucks
      sendBrake();
      truck.setAccel(-9999.0);
      break;
    }
  }

  bool isConnectedToGodot() const { return godotConnected; }

  void sendInfoToTruck(const TruckInfo &truck) {
    proto::InfoPayload info = {};
    info.self_id = truck.id;

    if (truck.position > 1) {
      const auto &front = platoon[truck.position - 2];
      info.front_id = front.id;
      inet_pton(AF_INET6, front.ipv6addr.c_str(), &info.front_ip);
      info.front_port = htons(front.udp_port);
    } else {
      info.front_id = 0;
    }

    if (truck.position < (int)platoon.size()) {
      const auto &behind = platoon[truck.position];
      info.behind_id = behind.id;
      inet_pton(AF_INET6, behind.ipv6addr.c_str(), &info.behind_ip);
      info.behind_port = htons(behind.udp_port);
    } else {
      info.behind_id = 0;
    }

    auto msg = proto::Encoder::info(info);
    sendToTruck(truck, msg);

    cout << "Sent INFO to truck " << truck.id << ": front=" << info.front_id
         << " behind=" << info.behind_id << endl;
  }

  void printPlatoon() {
    cout << "Current platoon:" << endl;
    for (const auto &truck : platoon) {
      cout << "  Pos " << truck.position << ": ID=" << truck.id << " "
           << truck.ipv6addr << ":" << truck.tcp_port
           << " (UDP:" << truck.udp_port << ")" << endl;
    }
  }

private:
  void sendBrakeToTruck(const TruckInfo &truck) {
    auto msg = proto::Encoder::brake();
    sendToTruck(truck, msg);
  }

  void sendReleaseToTruck(const TruckInfo &truck) {
    auto msg = proto::Encoder::release();
    sendToTruck(truck, msg);
  }

  void sendRemoveToTruck(const TruckInfo &truck) {
    proto::RemovePayload payload;
    payload.truck_id = truck.id;
    auto msg = proto::Encoder::remove(payload);
    sendToTruck(truck, msg);
  }

  bool sendToTruck(const TruckInfo &truck, const vector<uint8_t> &msg) {
    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0) {
      cerr << "Socket creation failed" << endl;
      return false;
    }

    struct sockaddr_in6 servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin6_family = AF_INET6;
    servaddr.sin6_port = htons(truck.tcp_port);
    if (inet_pton(AF_INET6, truck.ipv6addr.c_str(), &servaddr.sin6_addr) <= 0) {
      cerr << "Invalid IPv6 address: " << truck.ipv6addr << endl;
      close(sockfd);
      return false;
    }

    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
      cerr << "Connection to " << truck.ipv6addr << ":" << truck.tcp_port
           << " failed" << endl;
      close(sockfd);
      return false;
    }

    size_t sent = 0;
    while (sent < msg.size()) {
      ssize_t n = send(sockfd, msg.data() + sent, msg.size() - sent, 0);
      if (n <= 0) {
        cerr << "Send failed" << endl;
        close(sockfd);
        return false;
      }
      sent += n;
    }

    close(sockfd);
    return true;
  }

  void sendToGodot(const string &message) {
    if (!godotConnected) {
      cerr << "Not connected to Godot." << endl;
      return;
    }

    string msg = message + "\n";
    send(godotSocket, msg.c_str(), msg.size(), 0);
  }
};

void setNonBlockingInput(bool enable) {
  static struct termios oldstate, newstate;

  if (enable) {
    tcgetattr(STDIN_FILENO, &oldstate);
    newstate = oldstate;
    newstate.c_lflag &= ~ICANON;
    newstate.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &newstate);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
  } else {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldstate);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
  }
}

void drivingMode(PlatoonLeader &leader) {
  if (!leader.isConnectedToGodot()) {
    cerr << "Not connected to Godot. Cannot enter driving mode." << endl;
    return;
  }

  cout << "Entering driving mode. Use W/A/S/D to control, Q to quit." << endl;
  setNonBlockingInput(true);

  bool driving = true;
  char ch;

  auto last_time = chrono::steady_clock::now();

  while (driving) {
    auto current_time = chrono::steady_clock::now();
    double dt = chrono::duration<double>(current_time - last_time).count();
    last_time = current_time;

    leader.truck.simulateFrame(dt);
    leader.sendStateToGodot();

    ssize_t n = read(STDIN_FILENO, &ch, 1);

    if (n > 0) {
      ch = tolower(ch);
      switch (ch) {
      case 'w':
      case 'a':
      case 's':
      case 'd':
      case 'z':
      case ' ':
        leader.sendDriveCommand(ch);
        break;
      case 'q':
        driving = false;
        leader.sendDriveCommand(' ');
        break;
      default:
        break;
      }
    }
    usleep(10000);
  }

  setNonBlockingInput(false);
}

void platoonManagementMode(PlatoonLeader &leader) {
  string line;
  while (true) {
    cout << "\nCommands: ADD <ipv6> <tcp_port> <udp_port> | REMOVE <ipv6> "
            "<tcp_port> | BRAKE | RELEASE | DRIVE | EXIT\n> ";
    getline(cin, line);
    stringstream ss(line);
    string command, ipv6addr;
    int tcp_port, udp_port;

    ss >> command;
    if (command == "ADD") {
      ss >> ipv6addr >> tcp_port >> udp_port;
      leader.addTruck(ipv6addr, tcp_port, udp_port);
    } else if (command == "REMOVE") {
      ss >> ipv6addr >> tcp_port;
      leader.removeTruck(ipv6addr, tcp_port);
    } else if (command == "BRAKE") {
      leader.sendBrake();
    } else if (command == "RELEASE") {
      leader.sendRelease();
    } else if (command == "DRIVE") {
      drivingMode(leader);
    } else if (command == "EXIT") {
      break;
    } else {
      cout << "Invalid command." << endl;
    }
  }
}

int main(int argc, char **argv) {
  PlatoonLeader leader;
  string ip;
  int port = 0;

  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--ip" && i + 1 < argc) {
      ip = argv[++i];
    } else if (arg == "--port" && i + 1 < argc) {
      port = stoi(argv[++i]);
    }
  }

  if (!ip.empty()) {
    if (!leader.connectToGodot(ip, port)) {
      cerr << "Failed to connect to Godot at " << ip << ":" << port << endl;
      return -1;
    }
  }

  while (true) {
    cout << "\n===================================" << endl;
    cout << "      Platoon Management Mode      " << endl;
    cout << "===================================" << endl;
    cout << "Select mode:" << endl;
    cout << " 1. Platoon Management" << endl;
    cout << " 2. Driving Mode" << endl;
    cout << " 3. Exit" << endl;
    cout << "> ";

    string line;
    getline(cin, line);

    if (line == "1") {
      platoonManagementMode(leader);
    } else if (line == "2") {
      drivingMode(leader);
    } else if (line == "3") {
      break;
    } else {
      cout << "Invalid choice." << endl;
    }
  }

  return 0;
}
