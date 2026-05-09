#ifndef MAGI_LAYER3_IPV4_HPP
#define MAGI_LAYER3_IPV4_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace magi {

struct IPv4Packet {
    uint8_t version;
    uint8_t ihl;
    uint8_t diffServ;
    uint16_t totalLength;
    uint16_t identification;
    uint8_t flags;
    uint16_t fragmentOffset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t checksum;
    std::string srcIp;
    std::string dstIp;
    std::vector<uint8_t> payload;

    IPv4Packet();

    std::vector<uint8_t> toBytes() const;
    void fromBytes(const std::vector<uint8_t>& bytes);

    uint16_t calculateChecksum() const;
    void updateChecksum();
    bool validateChecksum() const;
};

}

#endif