#pragma once
#include <cstdint>

namespace proto {

constexpr uint8_t PROTOCOL_VERSION = 1;

// Message Types
enum class MessageType : uint8_t {
  // Topology / control (TCP)
  ADD = 1,
  REMOVE = 2,
  BRAKE = 3,
  RELEASE = 4,

  // Data (UDP)
  STATE = 10,

  // Topology info (TCP)
  INFO = 11,

  // Acknowledgements (TCP)
  ACK_ADD = 20,
  ACK_REMOVE = 21,
  ACK_INFO = 22,
};

inline const char *ToString(MessageType v) {
  switch (v) {
  case MessageType::ADD:
    return "ADD";
  case MessageType::REMOVE:
    return "REMOVE";
  case MessageType::BRAKE:
    return "BRAKE";
  case MessageType::RELEASE:
    return "RELEASE";

  case MessageType::STATE:
    return "STATE";
  case MessageType::INFO:
    return "INFO";

  case MessageType::ACK_ADD:
    return "ACK_ADD";
  case MessageType::ACK_REMOVE:
    return "ACK_REMOVE";
  case MessageType::ACK_INFO:
    return "ACK_INFO";

  default:
    return "[Unknown MessageType]";
  }
}

// Packet Header
#pragma pack(push, 1)
struct PacketHeader {
  uint8_t protocol_version;
  uint8_t type;
  uint8_t flags;
  uint16_t length; // payload length (network byte order)
};
#pragma pack(pop)

// Common IPv6 type
#pragma pack(push, 1)
struct IPv6Address {
  uint8_t bytes[16]; // network byte order
};
#pragma pack(pop)

// Payloads

// ADD (TCP)
#pragma pack(push, 1)
struct AddPayload {
  uint32_t truck_id;
  int pos;
};
#pragma pack(pop)

// ACK_ADD (TCP)
#pragma pack(push, 1)
struct AckAddPayload {
  int pos;
};
#pragma pack(pop)

// REMOVE (TCP)
#pragma pack(push, 1)
struct RemovePayload {
  uint32_t truck_id;
};
#pragma pack(pop)

// INFO - authoritative topology (TCP)
#pragma pack(push, 1)
struct InfoPayload {
  uint32_t self_id;

  uint32_t front_id;
  uint32_t behind_id;

  IPv6Address front_ip;
  uint16_t front_port; // UDP port for receiving STATE from front truck

  IPv6Address behind_ip;
  uint16_t behind_port; // UDP port where behind truck receives STATE
};
#pragma pack(pop)

// ACK_INFO (TCP)
#pragma pack(push, 1)
struct AckInfoPayload {
  uint32_t self_id;
};
#pragma pack(pop)

// STATE - position/velocity data (UDP)
// Enhanced with sequence number for packet loss detection
#pragma pack(push, 1)
struct StatePayload {
  uint32_t truck_id;     // Who sent this
  uint32_t sequence_num; // Monotonically increasing
  uint64_t timestamp_ns; // When this was sent

  float heading;      // radians
  float speed;        // m/s
  float acceleration; // m/s^2
  double x;           // meters
  double y;           // meters
};
#pragma pack(pop)

} // namespace proto
