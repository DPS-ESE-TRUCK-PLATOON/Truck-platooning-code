#pragma once
#include "protocol.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <vector>

namespace proto {

class Encoder {
public:
  // Control
  static std::vector<uint8_t> brake() {
    return header_only(MessageType::BRAKE);
  }

  static std::vector<uint8_t> add(const AddPayload &p) {
    return with_payload(MessageType::ADD, &p, sizeof(p));
  }

  static std::vector<uint8_t> remove(const RemovePayload &p) {
    return with_payload(MessageType::REMOVE, &p, sizeof(p));
  }

  // ACKs
  static std::vector<uint8_t> ack_add(const AckAddPayload &p) {
    return with_payload(MessageType::ACK_ADD, &p, sizeof(p));
  }

  static std::vector<uint8_t> ack_remove() {
    return header_only(MessageType::ACK_REMOVE);
  }

  static std::vector<uint8_t> ack_info(const AckInfoPayload &p) {
    return with_payload(MessageType::ACK_INFO, &p, sizeof(p));
  }

  // Data
  static std::vector<uint8_t> info(const InfoPayload &p) {
    return with_payload(MessageType::INFO, &p, sizeof(p));
  }

  static std::vector<uint8_t> state(const StatePayload &p) {
    return with_payload(MessageType::STATE, &p, sizeof(p));
  }

private:
  static std::vector<uint8_t> header_only(MessageType type) {
    PacketHeader hdr{};
    hdr.protocol_version = PROTOCOL_VERSION;
    hdr.type = static_cast<uint8_t>(type);
    hdr.flags = 0;
    hdr.length = htons(0);

    std::vector<uint8_t> buf(sizeof(hdr));
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    return buf;
  }

  static std::vector<uint8_t>
  with_payload(MessageType type, const void *payload, size_t payload_size) {
    PacketHeader hdr{};
    hdr.protocol_version = PROTOCOL_VERSION;
    hdr.type = static_cast<uint8_t>(type);
    hdr.flags = 0;
    hdr.length = htons(static_cast<uint16_t>(payload_size));

    std::vector<uint8_t> buf(sizeof(hdr) + payload_size);
    std::memcpy(buf.data(), &hdr, sizeof(hdr));
    std::memcpy(buf.data() + sizeof(hdr), payload, payload_size);
    return buf;
  }
};

} // namespace proto
