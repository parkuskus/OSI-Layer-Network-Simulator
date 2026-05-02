#include "ethernet.hpp"

#include <stdexcept>

namespace {

void writeU16(std::vector<uint8_t>& out, uint16_t value) {
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

void writeU32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(value & 0xFF));
}

uint16_t readU16(const std::vector<uint8_t>& in, size_t& pos) {
    if (pos + 2 > in.size()) throw std::runtime_error("Invalid Ethernet bytes (u16)");
    uint16_t v = (static_cast<uint16_t>(in[pos]) << 8) | static_cast<uint16_t>(in[pos + 1]);
    pos += 2;
    return v;
}

uint32_t readU32(const std::vector<uint8_t>& in, size_t& pos) {
    if (pos + 4 > in.size()) throw std::runtime_error("Invalid Ethernet bytes (u32)");
    uint32_t v = (static_cast<uint32_t>(in[pos]) << 24) |
                 (static_cast<uint32_t>(in[pos + 1]) << 16) |
                 (static_cast<uint32_t>(in[pos + 2]) << 8) |
                 static_cast<uint32_t>(in[pos + 3]);
    pos += 4;
    return v;
}

void writeString(std::vector<uint8_t>& out, const std::string& s) {
    writeU16(out, static_cast<uint16_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

std::string readString(const std::vector<uint8_t>& in, size_t& pos) {
    uint16_t len = readU16(in, pos);
    if (pos + len > in.size()) throw std::runtime_error("Invalid Ethernet bytes (string)");
    std::string out(in.begin() + static_cast<long>(pos), in.begin() + static_cast<long>(pos + len));
    pos += len;
    return out;
}

} // namespace

namespace magi {

std::vector<uint8_t> EthernetFrame::toBytes() const {
    std::vector<uint8_t> out;
    writeString(out, dstMac);
    writeString(out, srcMac);
    writeU16(out, static_cast<uint16_t>(etherType));
    writeU32(out, static_cast<uint32_t>(vlanId));
    writeU32(out, static_cast<uint32_t>(payload.size()));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

void EthernetFrame::fromBytes(const std::vector<uint8_t>& rawBytes) {
    size_t pos = 0;
    dstMac = readString(rawBytes, pos);
    srcMac = readString(rawBytes, pos);
    etherType = static_cast<short>(readU16(rawBytes, pos));
    vlanId = static_cast<int>(readU32(rawBytes, pos));
    uint32_t payloadLen = readU32(rawBytes, pos);
    if (pos + payloadLen > rawBytes.size()) {
        throw std::runtime_error("Invalid Ethernet bytes (payload)");
    }
    payload.assign(rawBytes.begin() + static_cast<long>(pos),
                   rawBytes.begin() + static_cast<long>(pos + payloadLen));
}

std::string EthernetFrame::getType() const {
    return "ethernet";
}

}