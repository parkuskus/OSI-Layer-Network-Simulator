#include "icmp.hpp"

#include <stdexcept>

namespace {

uint16_t internetChecksum(const std::vector<uint8_t>& bytes) {
    uint32_t sum = 0;
    size_t index = 0;

    while (index + 1 < bytes.size()) {
        sum += static_cast<uint16_t>((static_cast<uint16_t>(bytes[index]) << 8) |
                                     static_cast<uint16_t>(bytes[index + 1]));
        index += 2;
    }

    if (index < bytes.size()) {
        sum += static_cast<uint16_t>(bytes[index] << 8);
    }

    while ((sum >> 16) != 0) {
        sum = (sum & 0xFFFFu) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

}  // namespace

namespace magi {

ICMPMessage::ICMPMessage()
    : type(0), code(0), checksum(0), identifier(0), sequenceNumber(0) {
}

std::vector<uint8_t> ICMPMessage::toBytes() const {
    std::vector<uint8_t> bytes;
    bytes.reserve(8 + payload.size());
    bytes.push_back(type);
    bytes.push_back(code);
    bytes.push_back(static_cast<uint8_t>((checksum >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(checksum & 0xFF));
    bytes.push_back(static_cast<uint8_t>((identifier >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(identifier & 0xFF));
    bytes.push_back(static_cast<uint8_t>((sequenceNumber >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(sequenceNumber & 0xFF));
    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

void ICMPMessage::fromBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 8) {
        throw std::runtime_error("ICMP message too short");
    }

    type = bytes[0];
    code = bytes[1];
    checksum = static_cast<uint16_t>((static_cast<uint16_t>(bytes[2]) << 8) |
                                     static_cast<uint16_t>(bytes[3]));
    identifier = static_cast<uint16_t>((static_cast<uint16_t>(bytes[4]) << 8) |
                                       static_cast<uint16_t>(bytes[5]));
    sequenceNumber = static_cast<uint16_t>((static_cast<uint16_t>(bytes[6]) << 8) |
                                           static_cast<uint16_t>(bytes[7]));
    payload.assign(bytes.begin() + 8, bytes.end());
}

uint16_t ICMPMessage::calculateChecksum() const {
    ICMPMessage copy = *this;
    copy.checksum = 0;
    return internetChecksum(copy.toBytes());
}

void ICMPMessage::updateChecksum() {
    checksum = 0;
    checksum = calculateChecksum();
}

bool ICMPMessage::validateChecksum() const {
    return internetChecksum(toBytes()) == 0;
}

}  // namespace magi
