#ifndef MAGI_LAYER4_TCP_HPP
#define MAGI_LAYER4_TCP_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "core/packet.hpp"

namespace magi {

#define TCP_FLAG_FIN  0x01
#define TCP_FLAG_SYN  0x02
#define TCP_FLAG_RST  0x04
#define TCP_FLAG_PSH  0x08
#define TCP_FLAG_ACK  0x10
#define TCP_FLAG_URG  0x20

struct TCPSegment : public Packet {
    uint16_t sourcePort;
    uint16_t destinationPort;
    uint32_t seqNum;
    uint32_t ackNum;
    uint8_t dataOffset;
    uint8_t flags;
    uint16_t windowSize;
    uint16_t checksum;
    uint16_t urgentPointer;
    std::vector<uint8_t> payload;

    TCPSegment();
    
    std::vector<uint8_t> toBytes() const override;
    void fromBytes(const std::vector<uint8_t>& bytes) override;
    std::string getType() const override { return "TCP"; }

    uint16_t calculateChecksum(const std::string& srcIp, const std::string& dstIp) const;
    void updateChecksum(const std::string& srcIp, const std::string& dstIp);
    bool validateChecksum(const std::string& srcIp, const std::string& dstIp) const;

    bool hasSYN() const { return (flags & TCP_FLAG_SYN) != 0; }
    bool hasACK() const { return (flags & TCP_FLAG_ACK) != 0; }
    bool hasFIN() const { return (flags & TCP_FLAG_FIN) != 0; }
    bool hasPSH() const { return (flags & TCP_FLAG_PSH) != 0; }
    bool hasRST() const { return (flags & TCP_FLAG_RST) != 0; }
    
    size_t getPayloadSize() const { return payload.size(); }
};

struct TCPPseudoHeader {
    std::string srcIp;
    std::string dstIp;
    uint8_t reserved = 0;
    uint8_t protocol = 6;
    uint16_t tcpLength;
    
    std::vector<uint8_t> toBytes() const;
};

}

#endif // MAGI_LAYER4_TCP_HPP
