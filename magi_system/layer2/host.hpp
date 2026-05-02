#ifndef HOST_HPP
#define HOST_HPP

#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <vector>
#include "core/node.hpp"
#include "layer2/arp.hpp"

namespace magi {

class Interface;
class Packet;

// Host Node
class Host : public Node {
private:
    std::string ipAddress;
    std::string defaultGateway;
    ARPCache arpCache;

public:
    Host(const std::string& name, const std::string& ipAddress = "", const std::string& defaultGateway = "");
    
    // Getters dan Setters
    std::string getIpAddress() const { return ipAddress; }
    std::string getDefaultGateway() const { return defaultGateway; }
    void setIpAddress(const std::string& ip) { ipAddress = ip; }
    void setDefaultGateway(const std::string& gateway) { defaultGateway = gateway; }
    
    // Override virtual methods
    void handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) override;
    std::string getType() const override { return "host"; }
    std::string toJson() const override;
    void printInfo() const override;
    void printArpCache() const;

    void sendLayer3Packet(std::string targetIp, std::vector<uint8_t> l3Bytes);
};

} // namespace magi

#endif // HOST_HPP