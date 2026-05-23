#ifndef MAGI_LAYER3_NAT_HPP
#define MAGI_LAYER3_NAT_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace magi {

enum class NATType {
    STATIC,   // One-to-one mapping  
    DYNAMIC   // Many-to-one mapping (PAT)
};

struct NATMapping {
    std::string internalIp;
    uint16_t internalPort;
    std::string externalIp;
    uint16_t externalPort;
    uint8_t protocol;  // IPPROTO_TCP (6) or IPPROTO_UDP (17)
    NATType natType;
    bool isActive;

    NATMapping()
        : internalIp(""), internalPort(0), externalIp(""), externalPort(0),
          protocol(0), natType(NATType::DYNAMIC), isActive(true) {}
    
    NATMapping(const std::string& intIp, uint16_t intPort,
               const std::string& extIp, uint16_t extPort,
               uint8_t proto, NATType type = NATType::DYNAMIC)
        : internalIp(intIp), internalPort(intPort),
          externalIp(extIp), externalPort(extPort),
          protocol(proto), natType(type), isActive(true) {}
};

class NATTable {
private:
    std::vector<NATMapping> staticMappings;
    std::map<std::string, NATMapping> dynamicMappings;  // Key: "internal_ip:internal_port:protocol"
    std::map<std::string, NATMapping> reverseMappings;  // Key: "external_ip:external_port:protocol"
    uint16_t nextDynamicPort;

    std::string makeInternalKey(const std::string& ip, uint16_t port, uint8_t proto) const;
    std::string makeExternalKey(const std::string& ip, uint16_t port, uint8_t proto) const;

public:
    NATTable() : nextDynamicPort(49152) {}  // Start from ephemeral port range

    // Static NAT: one-to-one mapping
    int addStaticMapping(const std::string& internalIp, uint16_t internalPort,
                        const std::string& externalIp, uint16_t externalPort,
                        uint8_t protocol);

    // Dynamic NAT/PAT: allocate external port based on internal IP:port
    int addDynamicMapping(const std::string& internalIp, uint16_t internalPort,
                         const std::string& externalIp,
                         uint8_t protocol);

    // Lookup: internal to external translation
    bool lookupInternal(const std::string& internalIp, uint16_t internalPort,
                       uint8_t protocol, std::string& outExternalIp, uint16_t& outExternalPort) const;

    // Lookup: external to internal translation (for return traffic)
    bool lookupExternal(const std::string& externalIp, uint16_t externalPort,
                       uint8_t protocol, std::string& outInternalIp, uint16_t& outInternalPort) const;

    bool removeMapping(const std::string& internalIp, uint16_t internalPort, uint8_t protocol);
    void clearMappings();
    void printMappings() const;

    size_t getStaticMappingCount() const { return staticMappings.size(); }
    size_t getDynamicMappingCount() const { return dynamicMappings.size(); }
};

}  // namespace magi

#endif
