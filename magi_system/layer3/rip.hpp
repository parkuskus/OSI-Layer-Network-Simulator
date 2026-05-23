#ifndef MAGI_LAYER3_RIP_HPP
#define MAGI_LAYER3_RIP_HPP

#include <cstdint>
#include <string>
#include <vector>

namespace magi {

constexpr uint16_t kRipPort = 520;
constexpr uint8_t kRipVersion = 2;
constexpr uint8_t kRipCommandRequest = 1;
constexpr uint8_t kRipCommandResponse = 2;
constexpr uint8_t kRipInfinity = 16;
constexpr uint8_t kRipDefaultMetric = 1;
constexpr size_t kRipEntrySize = 20;
constexpr size_t kRipHeaderSize = 4;
constexpr uint16_t kRipAddressFamilyIp = 2;

struct RipEntry {
    uint16_t addressFamilyId = kRipAddressFamilyIp;
    uint16_t routeTag = 0;
    uint32_t ipAddress = 0;
    uint32_t subnetMask = 0;
    uint32_t nextHop = 0;
    uint32_t metric = kRipInfinity;

    std::vector<uint8_t> toBytes() const;
    bool fromBytes(const std::vector<uint8_t>& data, size_t offset);
};

struct RipPacket {
    uint8_t command = kRipCommandResponse;
    uint8_t version = kRipVersion;
    uint16_t mustBeZero = 0;
    std::vector<RipEntry> entries;

    std::vector<uint8_t> toBytes() const;
    bool fromBytes(const std::vector<uint8_t>& data);

    size_t getEntryCount() const { return entries.size(); }
};

} // namespace magi

#endif
