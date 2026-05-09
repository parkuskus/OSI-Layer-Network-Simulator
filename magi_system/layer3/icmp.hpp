#ifndef MAGI_LAYER3_ICMP_HPP
#define MAGI_LAYER3_ICMP_HPP

#include <cstdint>
#include <vector>

namespace magi {

constexpr uint8_t kICMPEchoReply = 0;
constexpr uint8_t kICMPDestinationUnreachable = 3;
constexpr uint8_t kICMPEchoRequest = 8;
constexpr uint8_t kICMPTimeExceeded = 11;

struct ICMPMessage {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequenceNumber;
    std::vector<uint8_t> payload;

    ICMPMessage();

    std::vector<uint8_t> toBytes() const;
    void fromBytes(const std::vector<uint8_t>& bytes);

    uint16_t calculateChecksum() const;
    void updateChecksum();
    bool validateChecksum() const;
};

}

#endif