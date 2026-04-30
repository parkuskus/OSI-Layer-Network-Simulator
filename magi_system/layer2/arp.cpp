#include "arp.hpp"

#include <stdexcept>

namespace {

void writeU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint16_t readU16(const std::vector<uint8_t>& in, size_t& pos) {
    if (pos + 2 > in.size()) throw std::runtime_error("Invalid ARP bytes (u16)");
    uint16_t v = (static_cast<uint16_t>(in[pos]) << 8) | static_cast<uint16_t>(in[pos + 1]);
    pos += 2;
    return v;
}

void writeString(std::vector<uint8_t>& out, const std::string& s) {
    writeU16(out, static_cast<uint16_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

std::string readString(const std::vector<uint8_t>& in, size_t& pos) {
    uint16_t len = readU16(in, pos);
    if (pos + len > in.size()) throw std::runtime_error("Invalid ARP bytes (string)");
    std::string out(in.begin() + static_cast<long>(pos), in.begin() + static_cast<long>(pos + len));
    pos += len;
    return out;
}

} // namespace

namespace magi {

std::vector<uint8_t> ARPMessage::toBytes() const {
    std::vector<uint8_t> out;
    writeU16(out, static_cast<uint16_t>(opcode));
    writeString(out, senderMac);
    writeString(out, senderIp);
    writeString(out, targetMac);
    writeString(out, targetIp);
    return out;
}

void ARPMessage::fromBytes(const std::vector<uint8_t>& rawBytes) {
    size_t pos = 0;
    opcode = static_cast<short>(readU16(rawBytes, pos));
    senderMac = readString(rawBytes, pos);
    senderIp = readString(rawBytes, pos);
    targetMac = readString(rawBytes, pos);
    targetIp = readString(rawBytes, pos);
}

std::string ARPMessage::getType() const {
    return "arp";
}

}