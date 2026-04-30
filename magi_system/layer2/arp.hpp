#ifndef ARP_HPP
#define ARP_HPP

#include "core/packet.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <map>

namespace magi {

class ARPMessage : public Packet {
public:
    short opcode;
    std::string senderMac;
    std::string senderIp;
    std::string targetMac;
    std::string targetIp;

    std::vector<uint8_t> toBytes() const override;
    void fromBytes(const std::vector<uint8_t>& rawBytes) override;
    std::string getType() const override;
};

class ARPCache {
public:
    std::map<std::string, std::string> table;
    std::map<std::string, std::vector<std::vector<uint8_t>>> queue;
};

} // namespace magi

#endif // ARP_HPP