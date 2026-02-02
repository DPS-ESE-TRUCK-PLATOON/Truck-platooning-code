#include <algorithm>
#include <cmath>
#include <string>

using std::string;

constexpr float PI = 3.14159265358979323846f;

class Truck {
  float heading = 0;  // radians
  float acceleration = 0; // in m/s^2
  float speed = 0;    // in m/s (multiply by 3.6 for km/h)
  double x = 0.0;
  double y = 0.0;
  float max_speed = 70; // (max speed 70m/s ~= 250km/h)
  float max_accel = 2;  // in m/s^2
  float min_accel = -5; // in m/s^2
  float absolute_min_distance = 2;

public:
  double brakingDistance() {
    double speed_ms = (speed * 1000) / 3600;
    double min = std::abs(pow(speed_ms, 2) / (2 * min_accel));
    if (min < absolute_min_distance) {
      return absolute_min_distance;
    }
    return min;
  }
  
  void simulateFrame(float dt) {
    speed += acceleration * dt;
    speed = std::clamp(speed, 0.0f, max_speed);
    if ((speed < 0.0001f) & (speed > -0.0001f)) speed = 0;
    x += std::cos(heading) * speed * dt;
    y += std::sin(heading) * speed * dt;
  }

  void setAccel(float accel) {
    acceleration = std::clamp(accel, min_accel, max_accel);
  }

  void setHeading(float angle) {
    // set heading, takes angle in degrees
    float rad = angle * PI / 180.0f;
    heading = std::fmod(rad, 2.0f * PI);
    if (heading < 0.0f)
      heading += 2.0f * PI;
  }

  void setLocation(double px, double py) {
    x = px;
    y = py;
  }

  float getHeading() const {
    return heading * 180 / PI;
  }
  float getSpeed() const { return speed; }
  float getAccel() const { return acceleration; }
  double getX() const { return x; }
  double getY() const { return y; }

  string getData() const {
    return std::to_string(speed) + " " + std::to_string(heading * 180 / PI) +
           " " + std::to_string(x) + " " + std::to_string(y);
  }
};
