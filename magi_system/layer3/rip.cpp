#include "rip.hpp"
#include <cstring>
#include <stdexcept>

namespace magi {

std::vector<uint8_t> RipEntry::toBytes() const {
    std::vector<uint8_t> data(kRipEntrySize, 0);
    data[0] = static_cast<uint8_t>((addressFamilyId >> 8) & 0xFF);
    data[1] = static_cast<uint8_t>(addressFamilyId & 0xFF);
    data[2] = static_cast<uint8_t>((routeTag >> 8) & 0xFF);
    data[3] = static_cast<uint8_t>(routeTag & 0xFF);
    data[4] = static_cast<uint8_t>((ipAddress >> 24) & 0xFF);
    data[5] = static_cast<uint8_t>((ipAddress >> 16) & 0xFF);
    data[6] = static_cast<uint8_t>((ipAddress >> 8) & 0xFF);
    data[7] = static_cast<uint8_t>(ipAddress & 0xFF);
    data[8] = static_cast<uint8_t>((subnetMask >> 24) & 0xFF);
    data[9] = static_cast<uint8_t>((subnetMask >> 16) & 0xFF);
    data[10] = static_cast<uint8_t>((subnetMask >> 8) & 0xFF);
    data[11] = static_cast<uint8_t>(subnetMask & 0xFF);
    data[12] = static_cast<uint8_t>((nextHop >> 24) & 0xFF);
    data[13] = static_cast<uint8_t>((nextHop >> 16) & 0xFF);
    data[14] = static_cast<uint8_t>((nextHop >> 8) & 0xFF);
    data[15] = static_cast<uint8_t>(nextHop & 0xFF);
    data[16] = static_cast<uint8_t>((metric >> 24) & 0xFF);
    data[17] = static_cast<uint8_t>((metric >> 16) & 0xFF);
    data[18] = static_cast<uint8_t>((metric >> 8) & 0xFF);
    data[19] = static_cast<uint8_t>(metric & 0xFF);
    return data;
}

bool RipEntry::fromBytes(const std::vector<uint8_t>& data, size_t offset) {
    if (offset + kRipEntrySize > data.size()) return false;
    addressFamilyId = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
    routeTag = (static_cast<uint16_t>(data[offset + 2]) << 8) | data[offset + 3];
    ipAddress = (static_cast<uint32_t>(data[offset + 4]) << 24) |
                (static_cast<uint32_t>(data[offset + 5]) << 16) |
                (static_cast<uint32_t>(data[offset + 6]) << 8) |
                data[offset + 7];
    subnetMask = (static_cast<uint32_t>(data[offset + 8]) << 24) |
                 (static_cast<uint32_t>(data[offset + 9]) << 16) |
                 (static_cast<uint32_t>(data[offset + 10]) << 8) |
                 data[offset + 11];
    nextHop = (static_cast<uint32_t>(data[offset + 12]) << 24) |
              (static_cast<uint32_t>(data[offset + 13]) << 16) |
              (static_cast<uint32_t>(data[offset + 14]) << 8) |
              data[offset + 15];
    metric = (static_cast<uint32_t>(data[offset + 16]) << 24) |
             (static_cast<uint32_t>(data[offset + 17]) << 16) |
             (static_cast<uint32_t>(data[offset + 18]) << 8) |
             data[offset + 19];
    return true;
}

std::vector<uint8_t> RipPacket::toBytes() const {
    std::vector<uint8_t> data;
    data.push_back(command);
    data.push_back(version);
    data.push_back(static_cast<uint8_t>((mustBeZero >> 8) & 0xFF));
    data.push_back(static_cast<uint8_t>(mustBeZero & 0xFF));
    for (size_t i = 0; i < entries.size(); ++i) {
        std::vector<uint8_t> entryData = entries[i].toBytes();
        data.insert(data.end(), entryData.begin(), entryData.end());
    }
    return data;
}

bool RipPacket::fromBytes(const std::vector<uint8_t>& data) {
    if (data.size() < kRipHeaderSize) return false;
    command = data[0];
    version = data[1];
    mustBeZero = (static_cast<uint16_t>(data[2]) << 8) | data[3];
    entries.clear();
    size_t offset = kRipHeaderSize;
    while (offset + kRipEntrySize <= data.size()) {
        RipEntry entry;
        if (!entry.fromBytes(data, offset)) return false;
        entries.push_back(entry);
        offset += kRipEntrySize;
    }
    return true;
}

} // namespace magi
