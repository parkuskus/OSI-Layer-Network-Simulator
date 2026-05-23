#include "nat.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace magi {

std::string NATTable::makeInternalKey(const std::string& ip, uint16_t port, uint8_t proto) const {
    std::stringstream ss;
    ss << ip << ":" << port << ":" << static_cast<int>(proto);
    return ss.str();
}

std::string NATTable::makeExternalKey(const std::string& ip, uint16_t port, uint8_t proto) const {
    std::stringstream ss;
    ss << ip << ":" << port << ":" << static_cast<int>(proto);
    return ss.str();
}

int NATTable::addStaticMapping(const std::string& internalIp, uint16_t internalPort,
                              const std::string& externalIp, uint16_t externalPort,
                              uint8_t protocol) {
    NATMapping mapping(internalIp, internalPort, externalIp, externalPort, protocol, NATType::STATIC);
    staticMappings.push_back(mapping);
    
    // Also add to reverse mapping for return traffic
    const std::string externalKey = makeExternalKey(externalIp, externalPort, protocol);
    reverseMappings[externalKey] = mapping;
    
    return static_cast<int>(staticMappings.size() - 1);
}

int NATTable::addDynamicMapping(const std::string& internalIp, uint16_t internalPort,
                               const std::string& externalIp,
                               uint8_t protocol) {
    const std::string internalKey = makeInternalKey(internalIp, internalPort, protocol);
    
    // Check if mapping already exists
    if (dynamicMappings.find(internalKey) != dynamicMappings.end()) {
        return 1;  // Already exists
    }

    // Allocate external port from ephemeral range
    uint16_t externalPort = nextDynamicPort++;
    if (externalPort == 65535) {
        nextDynamicPort = 49152;  // Wrap around
    }

    NATMapping mapping(internalIp, internalPort, externalIp, externalPort, protocol, NATType::DYNAMIC);
    dynamicMappings[internalKey] = mapping;
    
    // Also add to reverse mapping for return traffic
    const std::string externalKey = makeExternalKey(externalIp, externalPort, protocol);
    reverseMappings[externalKey] = mapping;
    
    return 0;
}

bool NATTable::lookupInternal(const std::string& internalIp, uint16_t internalPort,
                             uint8_t protocol, std::string& outExternalIp, uint16_t& outExternalPort) const {
    const std::string internalKey = makeInternalKey(internalIp, internalPort, protocol);
    
    // Check dynamic mappings first
    std::map<std::string, NATMapping>::const_iterator it = dynamicMappings.find(internalKey);
    if (it != dynamicMappings.end()) {
        outExternalIp = it->second.externalIp;
        outExternalPort = it->second.externalPort;
        return true;
    }

    // Check static mappings
    for (size_t i = 0; i < staticMappings.size(); ++i) {
        if (staticMappings[i].internalIp == internalIp &&
            staticMappings[i].internalPort == internalPort &&
            staticMappings[i].protocol == protocol &&
            staticMappings[i].isActive) {
            outExternalIp = staticMappings[i].externalIp;
            outExternalPort = staticMappings[i].externalPort;
            return true;
        }
    }

    return false;
}

bool NATTable::lookupExternal(const std::string& externalIp, uint16_t externalPort,
                             uint8_t protocol, std::string& outInternalIp, uint16_t& outInternalPort) const {
    const std::string externalKey = makeExternalKey(externalIp, externalPort, protocol);
    
    std::map<std::string, NATMapping>::const_iterator it = reverseMappings.find(externalKey);
    if (it != reverseMappings.end()) {
        outInternalIp = it->second.internalIp;
        outInternalPort = it->second.internalPort;
        return true;
    }

    return false;
}

bool NATTable::removeMapping(const std::string& internalIp, uint16_t internalPort, uint8_t protocol) {
    const std::string internalKey = makeInternalKey(internalIp, internalPort, protocol);
    
    // Remove from dynamic mappings
    std::map<std::string, NATMapping>::iterator it = dynamicMappings.find(internalKey);
    if (it != dynamicMappings.end()) {
        const std::string externalKey = makeExternalKey(it->second.externalIp, it->second.externalPort, protocol);
        reverseMappings.erase(externalKey);
        dynamicMappings.erase(it);
        return true;
    }

    // Remove from static mappings
    for (size_t i = 0; i < staticMappings.size(); ++i) {
        if (staticMappings[i].internalIp == internalIp &&
            staticMappings[i].internalPort == internalPort &&
            staticMappings[i].protocol == protocol) {
            const std::string externalKey = makeExternalKey(staticMappings[i].externalIp, 
                                                           staticMappings[i].externalPort, protocol);
            reverseMappings.erase(externalKey);
            staticMappings.erase(staticMappings.begin() + static_cast<long>(i));
            return true;
        }
    }

    return false;
}

void NATTable::clearMappings() {
    staticMappings.clear();
    dynamicMappings.clear();
    reverseMappings.clear();
}

void NATTable::printMappings() const {
    std::cout << "[NAT Mappings]" << std::endl;
    
    if (staticMappings.empty() && dynamicMappings.empty()) {
        std::cout << "  (empty)" << std::endl;
        return;
    }

    std::cout << "  Type     Proto  Internal IP:Port          External IP:Port          Status" << std::endl;
    std::cout << "  -------  -----  -----------------------  -----------------------  --------" << std::endl;

    for (size_t i = 0; i < staticMappings.size(); ++i) {
        const NATMapping& mapping = staticMappings[i];
        std::string protoStr = (mapping.protocol == 6) ? "TCP" : "UDP";
        std::string statusStr = mapping.isActive ? "active" : "inactive";

        std::cout << "  Static   " << protoStr << "    "
                  << mapping.internalIp << ":" << mapping.internalPort << "          "
                  << mapping.externalIp << ":" << mapping.externalPort << "          "
                  << statusStr << std::endl;
    }

    for (std::map<std::string, NATMapping>::const_iterator it = dynamicMappings.begin();
         it != dynamicMappings.end(); ++it) {
        const NATMapping& mapping = it->second;
        std::string protoStr = (mapping.protocol == 6) ? "TCP" : "UDP";

        std::cout << "  Dynamic  " << protoStr << "    "
                  << mapping.internalIp << ":" << mapping.internalPort << "          "
                  << mapping.externalIp << ":" << mapping.externalPort << "          "
                  << "active" << std::endl;
    }
}

}  // namespace magi
