#include "switch.hpp"
#include "core/interface.hpp"
#include "layer2/ethernet.hpp"
#include <iostream>
#include <sstream>

namespace {

const std::string kBroadcastMac = "ff:ff:ff:ff:ff:ff";

std::string makeMacKey(int vlan, const std::string& mac) {
    return std::to_string(vlan) + ":" + mac;
}

} // namespace

namespace magi {


Switch::Switch(const std::string& name, uint32_t numPorts)
    : Node(name), numPorts(numPorts) {
    // Buat semua port switch
    for (uint32_t i = 0; i < numPorts; ++i) {
        addInterface();
        vlanConfig[static_cast<int>(i + 1)] = VlanConfig();
    }
}

void Switch::handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) {
    int ingressPort = 0;
    for (const auto& pair : interfaces) {
        if (pair.second.get() == incomingInterface) {
            ingressPort = static_cast<int>(pair.first);
            break;
        }
    }
    if (ingressPort == 0) {
        return;
    }

    EthernetFrame frame;
    try {
        frame.fromBytes(rawBytes);
    } catch (...) {
        return;
    }

    VlanConfig ingressCfg = vlanConfig[ingressPort];
    int ingressVlan = getIngressVlan(ingressCfg, frame.vlanId);
    if (ingressVlan <= 0) {
        return;
    }

    macTable[makeMacKey(ingressVlan, frame.srcMac)] = ingressPort;

    auto forwardToPort = [&](int outPort) {
        if (outPort == ingressPort) {
            return;
        }
        auto outIface = getInterface(static_cast<uint32_t>(outPort));
        if (!outIface || !outIface->isConnected()) {
            return;
        }
        sendFrameOnPort(outPort, rawBytes, ingressVlan);
    };

    if (frame.dstMac != kBroadcastMac) {
        const std::string key = makeMacKey(ingressVlan, frame.dstMac);
        auto macIt = macTable.find(key);
        if (macIt != macTable.end()) {
            forwardToPort(macIt->second);
            return;
        }
    }

    // Flood only to ports that can carry the same VLAN.
    for (const auto& pair : interfaces) {
        int outPort = static_cast<int>(pair.first);
        const VlanConfig cfg = vlanConfig[outPort];
        if (canEgressVlan(cfg, ingressVlan)) {
            forwardToPort(outPort);
        }
    }
}

std::string Switch::toJson() const {
    std::stringstream ss;
    ss << "    {\n";
    ss << "      \"name\": \"" << name << "\",\n";
    ss << "      \"num_ports\": " << numPorts << ",\n";
    ss << "      \"vlans\": []\n";
    ss << "    }";
    return ss.str();
}

void Switch::printInfo() const {
    std::cout << "[Switch: " << name << "]" << std::endl;
    std::cout << "  Number of Ports: " << numPorts << std::endl;
    std::cout << "  Interfaces:" << std::endl;
    for (const auto& pair : interfaces) {
        std::cout << "    Port " << pair.first << ": " << pair.second->getMacAddress();
        if (pair.second->isConnected()) {
            std::cout << " (Connected)";
        }
        std::cout << std::endl;
    }

    std::cout << "  VLAN Config:" << std::endl;
    for (const auto& entry : vlanConfig) {
        std::cout << "    Port " << entry.first << ": ";
        if (entry.second.mode == PortMode::ACCESS) {
            std::cout << "access vlan " << entry.second.accessVlan;
        } else {
            std::cout << "trunk native " << entry.second.nativeVlan;
        }
        std::cout << std::endl;
    }
}

void Switch::printMacTable() const {
    std::cout << "[MAC Table: " << name << "]" << std::endl;
    if (macTable.empty()) {
        std::cout << "  (empty)" << std::endl;
        return;
    }

    for (const auto& entry : macTable) {
        std::cout << "  " << entry.first << " -> port " << entry.second << std::endl;
    }
}

int Switch::getIngressVlan(const VlanConfig& cfg, int frameVlan) const {
    if (cfg.mode == PortMode::ACCESS) {
        if (frameVlan == -1 || frameVlan == cfg.accessVlan) {
            return cfg.accessVlan;
        }
        return -1;
    }

    if (frameVlan == -1) {
        return cfg.nativeVlan;
    }
    return frameVlan;
}

bool Switch::canEgressVlan(const VlanConfig& cfg, int vlan) const {
    if (cfg.mode == PortMode::ACCESS) {
        return cfg.accessVlan == vlan;
    }
    return vlan > 0;
}

void Switch::sendFrameOnPort(int port, const std::vector<uint8_t>& rawBytes, int vlan) const {
    auto iface = getInterface(static_cast<uint32_t>(port));
    if (!iface) {
        return;
    }

    EthernetFrame frame;
    try {
        frame.fromBytes(rawBytes);
    } catch (...) {
        return;
    }

    auto cfgIt = vlanConfig.find(port);
    VlanConfig cfg;
    if (cfgIt != vlanConfig.end()) {
        cfg = cfgIt->second;
    }

    if (cfg.mode == PortMode::ACCESS) {
        frame.vlanId = -1;
    } else {
        frame.vlanId = vlan;
    }

    iface->send(frame.toBytes());
}

void Switch::setAccessVlan(int port, int vlan) {
    if (vlan <= 0) {
        return;
    }
    vlanConfig[port].mode = PortMode::ACCESS;
    vlanConfig[port].accessVlan = vlan;
}

void Switch::setTrunkVlan(int port, int nativeVlan) {
    if (nativeVlan <= 0) {
        nativeVlan = 1;
    }
    vlanConfig[port].mode = PortMode::TRUNK;
    vlanConfig[port].nativeVlan = nativeVlan;
}

} 