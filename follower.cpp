#include "encoder.hpp"
#include "network.cpp"
#include "network.hpp"
#include "protocol.hpp"
#include "truck.cpp"
#include <atomic>
#include <csignal>
#include <iostream>

//communication_loss comm_loss;

// Truck state (atomic for thread safety)
std::atomic<uint32_t> truck_id{0};
std::atomic<bool> linked{false};

// Truck instance
Truck truck;
bool warning;

std::atomic<bool> running{true};

void signal_handler(int) { running = false; }

void emergencybraking() {
  float deceleration, actspeed, front_dis;
  front_dis = truck.brakingDistance();

  actspeed = truck.getSpeed() * (1000.0 / 3600); // from km/h to m/s
  if (truck.getSpeed() > 0) {
    deceleration = pow(actspeed, 2) / (front_dis * 2); // m/s^2
    truck.setAccel(-deceleration);
  } else {
    truck.setAccel(0); // stopped
  }
  return;
}

void process_lead_messages() {
  proto::DecodedMessage msg;
  while (network::pop_from_lead(msg)) {

    //comm_loss.connection_recovery(); // amin

    switch (msg.type) {
    case proto::MessageType::INFO: {
      const auto &info = proto::Decoder::as_info(msg);
      truck_id = info.self_id;
      linked = true;

      std::cout << "INFO: ID=" << info.self_id << " Front=" << info.front_id
                << " Behind=" << info.behind_id << "\n";

      // Configure back truck (will reconfigure if topology changes)
      if (info.behind_id != 0) {
        char addr[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &info.behind_ip, addr, sizeof(addr));
        uint16_t port = ntohs(info.behind_port);

        std::cout << "  -> Sending STATE to " << addr << ":" << port << "\n";
        network::set_back_truck(addr, port);
      } else {
        // No truck behind us anymore
        std::cout << "  -> No truck behind (last in platoon)\n";
        network::set_back_truck("", 0); // Clear back truck
      }

      // TODO: Handle front truck connection changes
      // if (info.front_id != 0) {
      //   char addr[INET6_ADDRSTRLEN];
      //   inet_ntop(AF_INET6, &info.front_ip, addr, sizeof(addr));
      //   uint16_t port = ntohs(info.front_port);
      //   // Connect to front truck's UDP socket
      // }

      // Send ACK
      proto::AckInfoPayload ack;
      ack.self_id = truck_id;
      network::queue_to_lead(proto::Encoder::ack_info(ack));
      break;
    }

    case proto::MessageType::BRAKE:
      std::cout << "BRAKE!\n";
      // truck.setAccel(-9999.0f);
      warning = true;
      emergencybraking();
      network::queue_to_lead(proto::Encoder::brake());
      network::queue_back(proto::MessageType::BRAKE);
      break;

    case proto::MessageType::RELEASE:
      std::cout << "RELEASE\n";
      // truck.setAccel(0.0f);
      warning = false;
      break;

    case proto::MessageType::REMOVE:
      std::cout << "REMOVED FROM PLATOON\n";
      truck.setAccel(-9999.0f);
      linked = 0;
      break;
    default:
      std::cout << "unimplemented packet from leader: " << ToString(msg.type)
                << std::endl;
      break;
    }
  }
}

double distance(double x1, double y1, double x2, double y2) {
  return std::hypot(x1 - x2, y1 - y2);
}

double angle(double y1, double y2, double hypot) { return (y1 - y2) / hypot; }

void process_front_messages() {
  proto::DecodedMessage msg;
  while (network::pop_from_front(msg)) {

    //comm_loss.connection_recovery(); // amin

    if (msg.type == proto::MessageType::STATE) {
      const auto &state = proto::Decoder::as_state(msg);

      // Match front truck's speed
      float target_speed = state.speed;
      float speed_diff = target_speed - truck.getSpeed();

      auto frontX = state.x;
      auto frontY = state.y;
      auto x = truck.getX();
      auto y = truck.getY();
      auto distanceToFront = distance(x, y, frontX, frontY);
      auto angleToFront = angle(y, frontY, distanceToFront);
      auto min_distance = truck.brakingDistance();
      auto max_distance = min_distance + 10;

      if (distanceToFront < min_distance) {
        // truck.setAccel(-999);
        // emergency braking
        warning = true;
        emergencybraking();
      } else if (distanceToFront > max_distance) {
        truck.setAccel(999);
      } else {
        truck.setAccel(speed_diff * 0.8f); // proportional control
      }

      // match front truck's heading
      // if it's too far, head towards the truck
      auto angle_diff = std::abs(angleToFront - state.heading);
      if (angle_diff > 10) {
        truck.setHeading(angleToFront);
      } else {
        truck.setHeading(state.heading);
      }

      // Debug print
      static int count = 0;
      if (++count % 60 == 0) {
        std::cout << "Front: accel=" << state.acceleration
                  << " our=" << truck.getAccel()
                  << " Heading: " << truck.getHeading()
                  << " front heading: " << state.heading
                  << " Angle to front: " << angleToFront << std::endl;
      }
    } else if (msg.type == proto::MessageType::BRAKE) {
      warning = true;
      emergencybraking();
      // network::queue_to_lead(proto::Encoder::brake());
      network::queue_back(proto::MessageType::BRAKE);
    }
  }
}

void update_physics(float dt) {
  truck.simulateFrame(dt);
  if (!linked)
    truck.setAccel(-9999.0f);

  // Queue truck state for network thread
  proto::StatePayload state;
  state.heading = truck.getHeading();
  state.speed = truck.getSpeed();
  state.acceleration = truck.getAccel();
  state.x = truck.getX();
  state.y = truck.getY();
  network::queue_back_state(state);
}

int main(int argc, char **argv) {
  int port = 1234;

  // add a way to specify UDP port maybe?
  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    }
  }

  if (!network::init(port)) {
    std::cerr << "Network init failed\n";
    return 1;
  }

  // Start threads
  std::thread t1(network::lead_thread, std::ref(running));
  std::thread t2(network::front_rx_thread, std::ref(running));
  std::thread t3(network::back_tx_thread, std::ref(running),
                 std::ref(truck_id));

  std::cout << "Follower truck ready on port " << port << "\n";
  std::cout << "Waiting for lead truck...\n";

  // Main loop
  using namespace std::chrono;
  auto last = steady_clock::now();
  const float dt = 0.016f; // 60 Hz

  while (running) {
    auto now = steady_clock::now();
    if (duration<float>(now - last).count() >= dt) {
      last = now;
      process_lead_messages();
      process_front_messages();
      update_physics(dt);

      // Communication of a truck is permanently lost (added by amin)
      // if (linked && comm_loss.permanentConnection_loss()) {
      //   std::cout << "COMMUNICATION IS PERMANENTLY LOST, BRAKING AND STEERING "
      //                "TO SHOULDER TO PARK.\n";
      //   truck.setAccel(-9999.0f);
      //   linked = false;
      // }

      // Status print
      static int tick = 0;
      if (++tick % 120 == 0) {
        std::cout << "ID=" << truck_id.load() << " linked=" << linked.load()
                  << " speed=" << truck.getSpeed() << " pos=(" << truck.getX()
                  << "," << truck.getY() << ")\n";
      }
    }

    std::this_thread::sleep_for(milliseconds(1));
  }

  // Cleanup
  running = false;
  // t1.join();
  // t2.join();
  // t3.join();
  network::cleanup();

  return 0;
}
