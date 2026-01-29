#include "network.hpp"
#include "truck.cpp"
#include "network.cpp"
#include <atomic>
#include <csignal>
#include <iostream>

// Truck state (atomic for thread safety)
std::atomic<uint32_t> truck_id{0};
std::atomic<bool> linked{false};

// Truck instance
Truck truck;

// Atomic copies for network thread
std::atomic<float> heading{0.0f};
std::atomic<float> speed{0.0f};
std::atomic<float> accel{0.0f};
std::atomic<double> pos_x{0.0};
std::atomic<double> pos_y{0.0};

std::atomic<bool> running{true};

void signal_handler(int) { running = false; }

void sync_to_atomics() {
  // Copy truck state to atomics for network thread
  heading = truck.getHeading();
  speed = truck.getSpeed();
  accel = truck.getAccel();
  pos_x = truck.getX();
  pos_y = truck.getY();
}

void process_lead_messages() {
  proto::DecodedMessage msg;
  while (network::pop_from_lead(msg)) {
    switch (msg.type) {
    case proto::MessageType::INFO: {
      const auto &info = proto::Decoder::as_info(msg);
      truck_id = info.self_id;
      linked = true;

      std::cout << "INFO: ID=" << info.self_id 
                << " Front=" << info.front_id
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
      truck.setAccel(-5.0f);
      break;

    case proto::MessageType::RELEASE:
      std::cout << "RELEASE\n";
      truck.setAccel(0.0f);
      break;

    default:
      break;
    }
  }
}

void process_front_messages() {
  proto::DecodedMessage msg;
  while (network::pop_from_front(msg)) {
    if (msg.type == proto::MessageType::STATE) {
      const auto &state = proto::Decoder::as_state(msg);

      // Simple platooning: match front truck's speed
      float target_speed = state.speed;
      float speed_diff = target_speed - truck.getSpeed();

      float new_accel = speed_diff * 0.5f; // Proportional control
      if (new_accel > 2.0f)
        new_accel = 2.0f;
      if (new_accel < -5.0f)
        new_accel = -5.0f;

      truck.setAccel(new_accel);

      // Debug
      static int count = 0;
      if (++count % 60 == 0) {
        std::cout << "Front: speed=" << state.speed 
                  << " our=" << truck.getSpeed() << "\n";
      }
    }
  }
}

void update_physics(float dt) {
  truck.simulateFrame(dt);
  sync_to_atomics();
}

int main(int argc, char **argv) {
  int port = 8080;

  for (int i = 1; i < argc; i++) {
    if (std::string(argv[i]) == "--port" && i + 1 < argc) {
      port = std::stoi(argv[++i]);
    }
  }

  signal(SIGINT, signal_handler);

  if (!network::init(port)) {
    std::cerr << "Network init failed\n";
    return 1;
  }

  // Start threads
  std::thread t1(network::lead_thread, std::ref(running));
  std::thread t2(network::front_rx_thread, std::ref(running));
  std::thread t3(network::back_tx_thread, std::ref(running), std::ref(truck_id),
                 std::ref(heading), std::ref(speed), std::ref(accel),
                 std::ref(pos_x), std::ref(pos_y));

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

      // Status print
      static int tick = 0;
      if (++tick % 120 == 0) {
        std::cout << "ID=" << truck_id.load() << " linked=" << linked.load()
                  << " speed=" << truck.getSpeed() 
                  << " pos=(" << truck.getX() << "," << truck.getY() << ")\n";
      }
    }

    std::this_thread::sleep_for(milliseconds(1));
  }

  // Cleanup
  running = false;
  t1.join();
  t2.join();
  t3.join();
  network::cleanup();

  return 0;
}
