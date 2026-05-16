#ifndef MAGI_LAYER4_UDP_HPP
#define MAGI_LAYER4_UDP_HPP

#include <cstdint>
#include <string>
#include <vector>
#include "core/packet.hpp"

namespace magi {

struct UDPSegment : public Packet {
    uint16_t sourcePort;
    uint16_t destinationPort;
    uint16_t length;
    uint16_t checksum;
    std::vector<uint8_t> payload;

    UDPSegment();
    
    std::vector<uint8_t> toBytes() const override;
    void fromBytes(const std::vector<uint8_t>& bytes) override;
    std::string getType() const override { return "UDP"; }

    uint16_t calculateChecksum(const std::string& srcIp, const std::string& dstIp) const;
    void updateChecksum(const std::string& srcIp, const std::string& dstIp);
    bool validateChecksum(const std::string& srcIp, const std::string& dstIp) const;
    
    size_t getPayloadSize() const { return payload.size(); }
};

struct UDPPseudoHeader {
    std::string srcIp;
    std::string dstIp;
    uint8_t reserved = 0;
    uint8_t protocol = 17;
    uint16_t udpLength;
    
    std::vector<uint8_t> toBytes() const;
};

}

#endif // MAGI_LAYER4_UDP_HPP
