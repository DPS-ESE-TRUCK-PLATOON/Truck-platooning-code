#include "network.hpp"
#include "decoder.hpp"
#include "encoder.hpp"
#include "protocol.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <mutex>
#include <netinet/in.h>
#include <poll.h>
#include <queue>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

// Sockets
int lead_tcp_sock = -1;
int front_udp_sock = -1;
int back_udp_sock = -1;

// Ports
int lead_tcp_port = 0;
int front_udp_port = 0;

// Back truck destination (for UDP STATE sending)
sockaddr_in6 back_addr;
bool back_configured = false;

// Queues
std::mutex lead_out_mut, lead_in_mut, front_in_mut;
std::queue<std::vector<uint8_t>> lead_out_q;
std::queue<proto::DecodedMessage> lead_in_q, front_in_q;

// State sequence counter
std::atomic<uint32_t> state_seq{0};

void network::queue_to_lead(std::vector<uint8_t> msg) {
  std::lock_guard<std::mutex> lock(lead_out_mut);
  lead_out_q.push(std::move(msg));
}
bool network::pop_from_lead(proto::DecodedMessage &msg) {
  std::lock_guard<std::mutex> lock(lead_in_mut);
  if (lead_in_q.empty())
    return false;
  msg = std::move(lead_in_q.front());
  lead_in_q.pop();
  return true;
}
bool network::pop_from_front(proto::DecodedMessage &msg) {
  std::lock_guard<std::mutex> lock(front_in_mut);
  if (front_in_q.empty())
    return false;
  msg = std::move(front_in_q.front());
  front_in_q.pop();
  return true;
}
bool network::init(int tcp_port) {
  // Create sockets
  lead_tcp_sock = socket(AF_INET6, SOCK_STREAM, 0);
  front_udp_sock = socket(AF_INET6, SOCK_DGRAM, 0);
  back_udp_sock = socket(AF_INET6, SOCK_DGRAM, 0);

  if (lead_tcp_sock < 0 || front_udp_sock < 0 || back_udp_sock < 0) {
    std::cerr << "Socket creation failed\n";
    return false;
  }

  // Set options
  int opt = 1;
  setsockopt(lead_tcp_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  sockaddr_in6 addr = {};
  addr.sin6_family = AF_INET6;
  addr.sin6_addr = in6addr_any;

  // Bind and listen on TCP
  addr.sin6_port = htons(tcp_port);
  if (bind(lead_tcp_sock, (sockaddr *)&addr, sizeof(addr)) < 0 ||
      listen(lead_tcp_sock, 5) < 0) {
    std::cerr << "TCP bind/listen failed\n";
    return false;
  }
  lead_tcp_port = tcp_port;

  // Bind UDP receive socket
  addr.sin6_port = 0; // OS chooses port
  if (bind(front_udp_sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
    std::cerr << "UDP bind failed\n";
    return false;
  }

  socklen_t len = sizeof(addr);
  getsockname(front_udp_sock, (sockaddr *)&addr, &len);
  front_udp_port = ntohs(addr.sin6_port);

  std::cout << "Network initialized:\n"
            << "  TCP port: " << lead_tcp_port << "\n"
            << "  UDP port: " << front_udp_port << "\n";

  return true;
}
void network::set_back_truck(const std::string &ipv6, uint16_t port) {
  if (ipv6.empty() || port == 0) {
    back_configured = false;
    std::cout << "Back truck cleared\n";
    return;
  }

  back_addr = {};
  back_addr.sin6_family = AF_INET6;
  back_addr.sin6_port = htons(port);

  if (inet_pton(AF_INET6, ipv6.c_str(), &back_addr.sin6_addr) > 0) {
    back_configured = true;
    std::cout << "Back truck configured: " << ipv6 << ":" << port << "\n";
  } else {
    std::cerr << "Invalid back truck address\n";
    back_configured = false;
  }
}
bool network::send_tcp(int sock, const uint8_t *data, size_t len) {
  size_t sent = 0;
  while (sent < len) {
    ssize_t n = send(sock, data + sent, len - sent, 0);
    if (n <= 0)
      return false;
    sent += n;
  }
  return true;
}
bool network::recv_tcp(int sock, uint8_t *buf, size_t len) {
  size_t got = 0;
  while (got < len) {
    ssize_t n = recv(sock, buf + got, len - got, 0);
    if (n <= 0)
      return false;
    got += n;
  }
  return true;
}
void network::lead_thread(std::atomic<bool> &running) {
  int conn = -1;

  while (running) {
    // Accept if not connected
    if (conn < 0) {
      sockaddr_in6 client;
      socklen_t len = sizeof(client);
      conn = accept(lead_tcp_sock, (sockaddr *)&client, &len);
      if (conn >= 0) {
        std::cout << "Lead connected\n";
        fcntl(conn, F_SETFL, O_NONBLOCK);
      } else {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        continue;
      }
    }

    // Poll for read and write
    pollfd pfd = {conn, POLLIN, 0};
    {
      std::lock_guard<std::mutex> lock(lead_out_mut);
      if (!lead_out_q.empty())
        pfd.events |= POLLOUT;
    }

    int ret = poll(&pfd, 1, 100);
    if (ret < 0)
      break;

    // Receive
    if (pfd.revents & POLLIN) {
      proto::PacketHeader hdr;
      if (!recv_tcp(conn, (uint8_t *)&hdr, sizeof(hdr))) {
        std::cout << "Lead disconnected\n";
        close(conn);
        conn = -1;
        continue;
      }

      uint16_t payload_len = ntohs(hdr.length);
      std::vector<uint8_t> payload(payload_len);
      if (payload_len > 0 && !recv_tcp(conn, payload.data(), payload_len)) {
        close(conn);
        conn = -1;
        continue;
      }

      proto::DecodedMessage msg{(proto::MessageType)hdr.type,
                                std::move(payload)};
      {
        std::lock_guard<std::mutex> lock(lead_in_mut);
        lead_in_q.push(std::move(msg));
      }
    }

    // Send
    if (pfd.revents & POLLOUT) {
      std::vector<uint8_t> msg;
      {
        std::lock_guard<std::mutex> lock(lead_out_mut);
        if (!lead_out_q.empty()) {
          msg = std::move(lead_out_q.front());
          lead_out_q.pop();
        }
      }

      if (!msg.empty() && !send_tcp(conn, msg.data(), msg.size())) {
        close(conn);
        conn = -1;
      }
    }
  }

  if (conn >= 0)
    close(conn);
}
void network::front_rx_thread(std::atomic<bool> &running) {
  uint8_t buf[1024];
  uint32_t last_seq = 0;
  bool first = true;

  while (running) {
    sockaddr_in6 sender;
    socklen_t len = sizeof(sender);
    ssize_t n = recvfrom(front_udp_sock, buf, sizeof(buf), 0,
                         (sockaddr *)&sender, &len);

    if (n < (ssize_t)sizeof(proto::PacketHeader)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }

    proto::PacketHeader *hdr = (proto::PacketHeader *)buf;
    uint16_t payload_len = ntohs(hdr->length);

    if (n != (ssize_t)(sizeof(proto::PacketHeader) + payload_len))
      continue;

    std::vector<uint8_t> payload(buf + sizeof(proto::PacketHeader), buf + n);
    proto::DecodedMessage msg{(proto::MessageType)hdr->type,
                              std::move(payload)};

    // Check sequence (basic loss detection)
    if (msg.type == proto::MessageType::STATE) {
      const auto &state = proto::Decoder::as_state(msg);
      if (!first && state.sequence_num > last_seq + 1) {
        std::cout << "Lost " << (state.sequence_num - last_seq - 1)
                  << " packets\n";
      }
      if (!first && state.sequence_num <= last_seq)
        continue; // Out of order, skip
      last_seq = state.sequence_num;
      first = false;
    }

    {
      std::lock_guard<std::mutex> lock(front_in_mut);
      front_in_q.push(std::move(msg));
    }
  }
}
void network::back_tx_thread(std::atomic<bool> &running,
                             std::atomic<uint32_t> &truck_id,
                             std::atomic<float> &heading,
                             std::atomic<float> &speed,
                             std::atomic<float> &accel, std::atomic<double> &x,
                             std::atomic<double> &y) {

  using namespace std::chrono;
  auto next = steady_clock::now();
  const auto interval = milliseconds(16); // ~60 Hz

  while (running) {
    auto now = steady_clock::now();
    if (now < next) {
      std::this_thread::sleep_for(milliseconds(1));
      continue;
    }

    if (back_configured) {
      proto::StatePayload state;
      state.truck_id = truck_id.load();
      state.sequence_num = state_seq.fetch_add(1);
      state.timestamp_ns =
          duration_cast<nanoseconds>(system_clock::now().time_since_epoch())
              .count();
      state.heading = heading.load();
      state.speed = speed.load();
      state.acceleration = accel.load();
      state.x = x.load();
      state.y = y.load();

      auto msg = proto::Encoder::state(state);
      sendto(back_udp_sock, msg.data(), msg.size(), 0, (sockaddr *)&back_addr,
             sizeof(back_addr));
    }
    next += interval;
  }
}
void network::cleanup() {
  if (lead_tcp_sock >= 0)
    close(lead_tcp_sock);
  if (front_udp_sock >= 0)
    close(front_udp_sock);
  if (back_udp_sock >= 0)
    close(back_udp_sock);
}
