#include "node.hpp"
#include "interface.hpp"
#include "link.hpp"
#include <iostream>
#include <sstream>

namespace magi {


Node::Node(const std::string& name) 
    : name(name), nextPortNumber(1) {
}

std::shared_ptr<Interface> Node::getInterface(uint32_t portNumber) const {
    auto it = interfaces.find(portNumber);
    if (it != interfaces.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Interface> Node::addInterface() {
    return addInterface(Interface::generateMacAddress());
}

std::shared_ptr<Interface> Node::addInterface(const std::string& macAddress) {
    uint32_t portNum = nextPortNumber++;
    auto iface = std::make_shared<Interface>(this, portNum, macAddress);
    interfaces[portNum] = iface;
    return iface;
}

void Node::removeInterface(uint32_t portNumber) {
    auto it = interfaces.find(portNumber);
    if (it != interfaces.end()) {
        if (it->second->getLink()) {
            it->second->getLink()->disconnect();
        }
        interfaces.erase(it);
    }
}

uint32_t Node::getPortNumber(const std::shared_ptr<Interface>& iface) const {
    for (const auto& pair : interfaces) {
        if (pair.second == iface) {
            return pair.first;
        }
    }
    return 0;
}

void Node::printInfo() const {
    std::cout << "[" << getType() << ": " << name << "]" << std::endl;
    std::cout << "  Interfaces:" << std::endl;
    for (const auto& pair : interfaces) {
        std::cout << "    Port " << pair.first << ": " << pair.second->getMacAddress();
        if (pair.second->isConnected()) {
            std::cout << " (Connected)";
        } else {
            std::cout << " (Disconnected)";
        }
        std::cout << std::endl;
    }
}

Router::Router(const std::string& name)
    : Node(name) {
}

void Router::handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) {
    uint32_t portNum = 0;
    for (const auto& pair : interfaces) {
        if (pair.second.get() == incomingInterface) {
            portNum = pair.first;
            break;
        }
    }
    std::cout << "[" << name << "] Menerima " << rawBytes.size() << " bytes di port " << portNum << std::endl;
}

std::string Router::toJson() const {
    std::stringstream ss;
    ss << "    {\n";
    ss << "      \"name\": \"" << name << "\",\n";
    ss << "      \"interfaces\": [\n";
    bool first = true;
    for (const auto& pair : interfaces) {
        if (!first) ss << ",\n";
        first = false;
        ss << "        {\"port\": " << pair.first << ", \"ip_address\": \"\"}";
    }
    ss << "\n      ],\n";
    ss << "      \"routing_table\": []\n";
    ss << "    }";
    return ss.str();
}

void Router::printInfo() const {
    std::cout << "[Router: " << name << "]" << std::endl;
    std::cout << "  Interfaces:" << std::endl;
    for (const auto& pair : interfaces) {
        std::cout << "    Port " << pair.first << ": " << pair.second->getMacAddress();
        if (pair.second->isConnected()) {
            std::cout << " (Connected)";
        }
        std::cout << std::endl;
    }
}

} 
