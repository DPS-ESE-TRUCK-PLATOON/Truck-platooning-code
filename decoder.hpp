#pragma once
#include "protocol.hpp"
#include <arpa/inet.h>
#include <stdexcept>
#include <vector>

namespace proto {

struct DecodedMessage {
  MessageType type;
  std::vector<uint8_t> payload;
};

class Decoder {
public:
  template <typename ReadExactFn>
  static DecodedMessage read(ReadExactFn read_exact) {
    PacketHeader hdr;
    read_exact(&hdr, sizeof(hdr));

    if (hdr.protocol_version != PROTOCOL_VERSION)
      throw std::runtime_error("Unsupported protocol version");

    uint16_t payload_len = ntohs(hdr.length);
    std::vector<uint8_t> payload(payload_len);

    if (payload_len > 0)
      read_exact(payload.data(), payload_len);

    return {static_cast<MessageType>(hdr.type), std::move(payload)};
  }

  // Typed accessors
  static const AddPayload &as_add(const DecodedMessage &m) {
    check(m, MessageType::ADD, sizeof(AddPayload));
    return *reinterpret_cast<const AddPayload *>(m.payload.data());
  }

  static const AckAddPayload &as_ack_add(const DecodedMessage &m) {
    check(m, MessageType::ACK_ADD, sizeof(AckAddPayload));
    return *reinterpret_cast<const AckAddPayload *>(m.payload.data());
  }

  // static const RemovePayload &as_remove(const DecodedMessage &m) {
  //   check(m, MessageType::REMOVE, sizeof(RemovePayload));
  //   return *reinterpret_cast<const RemovePayload *>(m.payload.data());
  // }

  static const InfoPayload &as_info(const DecodedMessage &m) {
    check(m, MessageType::INFO, sizeof(InfoPayload));
    return *reinterpret_cast<const InfoPayload *>(m.payload.data());
  }

  static const AckInfoPayload &as_ack_info(const DecodedMessage &m) {
    check(m, MessageType::ACK_INFO, sizeof(AckInfoPayload));
    return *reinterpret_cast<const AckInfoPayload *>(m.payload.data());
  }

  static const StatePayload &as_state(const DecodedMessage &m) {
    check(m, MessageType::STATE, sizeof(StatePayload));
    return *reinterpret_cast<const StatePayload *>(m.payload.data());
  }

private:
  static void check(const DecodedMessage &m, MessageType expected,
                    size_t size) {
    if (m.type != expected)
      throw std::runtime_error("Wrong message type");
    if (m.payload.size() != size)
      throw std::runtime_error("Invalid payload size");
  }
};

} // namespace proto
