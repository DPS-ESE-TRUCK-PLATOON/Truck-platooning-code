#pragma once
#include "decoder.hpp"
#include <arpa/inet.h>
#include <atomic>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

namespace network {

// Queue helpers
void queue_to_lead(std::vector<uint8_t> msg);

bool pop_from_lead(proto::DecodedMessage &msg);

bool pop_from_front(proto::DecodedMessage &msg);

// Initialize sockets
bool init(int tcp_port);

// Configure where to send STATE messages
void set_back_truck(const std::string &ipv6, uint16_t port);

// Send all bytes on TCP socket
bool send_tcp(int sock, const uint8_t *data, size_t len);

// Receive exact bytes on TCP socket
bool recv_tcp(int sock, uint8_t *buf, size_t len);

// Thread 1: Handle lead truck connection (TCP bidirectional)
void lead_thread(std::atomic<bool> &running);

// Thread 2: Receive STATE from front truck (UDP)
void front_rx_thread(std::atomic<bool> &running);

// Thread 3: Send STATE to back truck (UDP)
void back_tx_thread(std::atomic<bool> &running, std::atomic<uint32_t> &truck_id,
                    std::atomic<float> &heading, std::atomic<float> &speed,
                    std::atomic<float> &accel, std::atomic<double> &x,
                    std::atomic<double> &y);

void cleanup();

} // namespace network
