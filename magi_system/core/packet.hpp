#ifndef PACKET_HPP
#define PACKET_HPP

#include <cstdint>
#include <vector>
#include <string>

namespace magi {


class Packet {
public:
    virtual ~Packet() = default;
    
    virtual std::vector<uint8_t> toBytes() const = 0;
    
    virtual void fromBytes(const std::vector<uint8_t>& rawBytes) = 0;
    
    virtual std::string getType() const = 0;
    
    static std::string bytesToHex(const std::vector<uint8_t>& bytes);
    
    static std::vector<uint8_t> hexToBytes(const std::string& hex);
};

} // namespace magi

#endif // PACKET_HPP
