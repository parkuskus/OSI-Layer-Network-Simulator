#ifndef ETHERNET_HPP
#define ETHERNET_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "core/packet.hpp"

namespace magi {

class EthernetFrame : public Packet {
public:
    std::string dstMac;
    std::string srcMac;
    short etherType;
    int vlanId;
    std::vector<uint8_t> payload;

    std::vector<uint8_t> toBytes() const override;
    void fromBytes(const std::vector<uint8_t>& rawBytes) override;
    std::string getType() const override;
};

} // namespace magi

#endif // ETHERNET_HPP