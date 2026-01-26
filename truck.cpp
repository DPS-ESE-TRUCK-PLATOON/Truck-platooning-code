#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <sstream>
#include <string>
#include <vector>

using std::string;

constexpr float PI = 3.14159265358979323846f;
enum State { Linked, Delinked };

struct location {
  double x;
  double y;
  location(double x, double y) : x(x), y(y) {}
};

class Truck {
  float heading = 0;  // radians
  float acceleration; // in m/s^2
  float speed = 0;    // in m/s (multiply by 3.6 for km/h)
  location loc = location(0, 0);
  float max_speed = 70; // (max speed 70m/s ~= 250km/h)
  float max_accel = 2;  // in m/s^2
  float min_accel = -5; // in m/s^2
public:
  string ipv6addr;
  int port;
  int positionInPlatoon;
  State state;
  string cmd;
  std::vector<std::tuple<time_t, location, float, float>> locationOfTruckAhead;

  Truck(const string &addr, int p)
      : ipv6addr(addr), port(p), positionInPlatoon(-1), state(Delinked) {}

  void simulateFrame(float dt) {
    speed += std::max((acceleration * dt), 0.0f);
    loc.x += cos(heading) * speed * dt;
    loc.y += sin(heading) * speed * dt;
  }
  void setAccel(float accel) {
    // set acceleration of the truck (can be negative for braking)
    acceleration = std::clamp(accel, min_accel, max_accel);
  }
  void setHeading(float angle) {
    // set heading, takes angle in degrees
    float rad = angle * PI / 180.0f;
    heading = std::fmod(rad, 2.0f * PI);
    if (heading < 0.0f)
      heading += 2.0f * PI;
  }
  string getData() {
    return std::to_string(speed) + " " + std::to_string(heading*180/PI) + " " +
           std::to_string(loc.x) + " " + std::to_string(loc.y);
  }

  
  string processCmd(const string &command) {
    std::istringstream ss(command);

    ss >> cmd;
    std::cout << cmd << std::endl;
    if (cmd == "LINK") {
      if (state == Linked) {
        return "ERROR Already linked";
      }
      state = Linked;
      ss >> positionInPlatoon;
      return "ACK LINK " + std::to_string(positionInPlatoon);
    } else if (cmd == "DELINK") {
      if (state != Linked) {
        return "ERROR Already delinked";
      }
      state = Delinked;
      positionInPlatoon = -1;
      return "ACK DELINK";
    } else {
      return "ERROR Unknown command";
    }
  }

  void platoonStandard(time_t current_time) {
    std::tuple test = locationOfTruckAhead.front();
    // we do not have an actual server, we just simulate things here
    std::cout << "truck: " << positionInPlatoon << " at: x-" << loc.x << " y-"
              << loc.y << " speed: " << speed << " heading: " << heading
              << std::endl;
  }
};
