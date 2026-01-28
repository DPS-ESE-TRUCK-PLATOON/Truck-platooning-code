#pragma once
#include <cstdint>

namespace proto {

constexpr uint8_t PROTOCOL_VERSION = 1;

// Message Types
enum class MessageType : uint8_t {
  // Topology / control
  ADD = 1,
  REMOVE = 2,
  BRAKE = 3,

  // Data
  STATE = 10,
  INFO = 11,

  // Acknowledgements
  ACK_ADD = 20,
  ACK_REMOVE = 21,
  ACK_INFO = 22,
};

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

// ADD
#pragma pack(push, 1)
struct AddPayload {
  uint32_t truck_id;
  int pos;
};
#pragma pack(pop)

// ACK_ADD
#pragma pack(push, 1)
struct AckAddPayload {
  int pos;
};
#pragma pack(pop)

// REMOVE
#pragma pack(push, 1)
struct RemovePayload {
  uint32_t truck_id;
};
#pragma pack(pop)

// INFO (authoritative topology)
#pragma pack(push, 1)
struct InfoPayload {
  uint32_t self_id;

  uint32_t front_id;
  uint32_t behind_id;

  IPv6Address front_ip;
  uint16_t front_port;

  IPv6Address behind_ip;
  uint16_t behind_port;
};
#pragma pack(pop)

// ACK_INFO
#pragma pack(push, 1)
struct AckInfoPayload {
  uint32_t self_id;
};
#pragma pack(pop)

// STATE
#pragma pack(push, 1)
struct StatePayload {
  float heading;
  float speed;
  float acceleration;
  double x;
  double y;
  uint64_t timestamp_ns;
};
#pragma pack(pop)

} // namespace proto
