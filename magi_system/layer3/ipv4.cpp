#include "ipv4.hpp"
#include "ip_utils.hpp"

#include <algorithm>
#include <stdexcept>

namespace {

uint16_t readU16(const std::vector<uint8_t>& bytes, size_t offset) {
    return static_cast<uint16_t>((static_cast<uint16_t>(bytes[offset]) << 8) | static_cast<uint16_t>(bytes[offset + 1]));
}

uint16_t internetChecksum(const std::vector<uint8_t>& bytes) {
    uint32_t sum = 0;
    size_t index = 0;

    while (index + 1 < bytes.size()) {
        sum += static_cast<uint16_t>((static_cast<uint16_t>(bytes[index]) << 8) | static_cast<uint16_t>(bytes[index + 1]));
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

}

namespace magi {

IPv4Packet::IPv4Packet()
    : version(4),
      ihl(5),
      diffServ(0),
      totalLength(20),
      identification(0),
      flags(0),
      fragmentOffset(0),
      ttl(64),
      protocol(0),
      checksum(0),
      srcIp("0.0.0.0"),
      dstIp("0.0.0.0") {
}

std::vector<uint8_t> IPv4Packet::toBytes() const {
    uint32_t srcValue = 0;
    uint32_t dstValue = 0;
    if (!iputil::parseIp(srcIp, srcValue) || !iputil::parseIp(dstIp, dstValue)) {
        throw std::runtime_error("Invalid IPv4 address");
    }

    std::vector<uint8_t> bytes;
    bytes.reserve(static_cast<size_t>(ihl) * 4 + payload.size());
    bytes.push_back(static_cast<uint8_t>((version << 4) | (ihl & 0x0F)));
    bytes.push_back(diffServ);
    bytes.push_back(static_cast<uint8_t>((totalLength >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(totalLength & 0xFF));
    bytes.push_back(static_cast<uint8_t>((identification >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(identification & 0xFF));

    const uint16_t flagsAndOffset = static_cast<uint16_t>((static_cast<uint16_t>(flags & 0x07) << 13) |
                                                          (fragmentOffset & 0x1FFF));
    bytes.push_back(static_cast<uint8_t>((flagsAndOffset >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(flagsAndOffset & 0xFF));
    bytes.push_back(ttl);
    bytes.push_back(protocol);
    bytes.push_back(static_cast<uint8_t>((checksum >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(checksum & 0xFF));

    bytes.push_back(static_cast<uint8_t>((srcValue >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((srcValue >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((srcValue >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(srcValue & 0xFF));

    bytes.push_back(static_cast<uint8_t>((dstValue >> 24) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((dstValue >> 16) & 0xFF));
    bytes.push_back(static_cast<uint8_t>((dstValue >> 8) & 0xFF));
    bytes.push_back(static_cast<uint8_t>(dstValue & 0xFF));

    bytes.insert(bytes.end(), payload.begin(), payload.end());
    return bytes;
}

void IPv4Packet::fromBytes(const std::vector<uint8_t>& bytes) {
    if (bytes.size() < 20) {
        throw std::runtime_error("IPv4 packet too short");
    }

    version = static_cast<uint8_t>((bytes[0] >> 4) & 0x0F);
    ihl = static_cast<uint8_t>(bytes[0] & 0x0F);
    if (version != 4 || ihl < 5) {
        throw std::runtime_error("Unsupported IPv4 header");
    }

    const size_t headerLength = static_cast<size_t>(ihl) * 4;
    if (bytes.size() < headerLength) {
        throw std::runtime_error("Truncated IPv4 header");
    }

    diffServ = bytes[1];
    totalLength = readU16(bytes, 2);
    identification = readU16(bytes, 4);

    const uint16_t flagsAndOffset = readU16(bytes, 6);
    flags = static_cast<uint8_t>((flagsAndOffset >> 13) & 0x07);
    fragmentOffset = static_cast<uint16_t>(flagsAndOffset & 0x1FFF);
    ttl = bytes[8];
    protocol = bytes[9];
    checksum = readU16(bytes, 10);

    const uint32_t srcValue = (static_cast<uint32_t>(bytes[12]) << 24) |
                              (static_cast<uint32_t>(bytes[13]) << 16) |
                              (static_cast<uint32_t>(bytes[14]) << 8) |
                              static_cast<uint32_t>(bytes[15]);
    const uint32_t dstValue = (static_cast<uint32_t>(bytes[16]) << 24) |
                              (static_cast<uint32_t>(bytes[17]) << 16) |
                              (static_cast<uint32_t>(bytes[18]) << 8) |
                              static_cast<uint32_t>(bytes[19]);
    srcIp = iputil::toIp(srcValue);
    dstIp = iputil::toIp(dstValue);

    const size_t effectiveLength = std::min(bytes.size(), static_cast<size_t>(totalLength));
    if (effectiveLength < headerLength) {
        throw std::runtime_error("Invalid IPv4 total length");
    }

    payload.assign(bytes.begin() + static_cast<long>(headerLength),
                   bytes.begin() + static_cast<long>(effectiveLength));
}

uint16_t IPv4Packet::calculateChecksum() const {
    IPv4Packet copy = *this;
    copy.totalLength = static_cast<uint16_t>(copy.ihl * 4 + copy.payload.size());
    copy.checksum = 0;
    std::vector<uint8_t> bytes = copy.toBytes();
    bytes.resize(static_cast<size_t>(copy.ihl) * 4);
    return internetChecksum(bytes);
}

void IPv4Packet::updateChecksum() {
    totalLength = static_cast<uint16_t>(ihl * 4 + payload.size());
    checksum = 0;
    checksum = calculateChecksum();
}

bool IPv4Packet::validateChecksum() const {
    IPv4Packet copy = *this;
    copy.totalLength = static_cast<uint16_t>(ihl * 4 + payload.size());
    std::vector<uint8_t> bytes;
    try {
        bytes = copy.toBytes();
    } catch (...) {
        return false;
    }
    bytes.resize(static_cast<size_t>(copy.ihl) * 4);
    return internetChecksum(bytes) == 0;
}

}