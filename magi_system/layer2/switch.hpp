#ifndef SWITCH_HPP
#define SWITCH_HPP

#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include "core/node.hpp"

namespace magi {

class Node;
class Interface;

enum class PortMode {
    ACCESS,
    TRUNK
};

struct VlanConfig {
    PortMode mode = PortMode::ACCESS;
    int accessVlan = 1;
    int nativeVlan = 1;
};

// Switch Node
class Switch : public Node {
private:
    uint32_t numPorts;
    std::map<std::string, int> macTable; // "vlan_id:mac" -> port
    // Map port_number -> VLAN config
    std::map<int, VlanConfig> vlanConfig;

    int getIngressVlan(const VlanConfig& cfg, int frameVlan) const;
    bool canEgressVlan(const VlanConfig& cfg, int vlan) const;
    void sendFrameOnPort(int port, const std::vector<uint8_t>& rawBytes, int vlan) const;

public:
    Switch(const std::string& name, uint32_t numPorts = 24);
    
    // Getters dan Setters
    uint32_t getNumPorts() const { return numPorts; }
    void setNumPorts(uint32_t num) { numPorts = num; }
    
    // Override virtual methods
    void handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) override;
    std::string getType() const override { return "switch"; }
    std::string toJson() const override;
    void printInfo() const override;
    void printMacTable() const;
    void setAccessVlan(int port, int vlan);
    void setTrunkVlan(int port, int nativeVlan = 1);
};

} // namespace magi

#endif // SWITCH_HPP