#ifndef NODE_HPP
#define NODE_HPP

#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <vector>

namespace magi {

class Interface;
class Packet;

class Node {
protected:
    std::string name;
    std::map<uint32_t, std::shared_ptr<Interface>> interfaces; 
    uint32_t nextPortNumber;

public:
    Node(const std::string& name);
    virtual ~Node() = default;
    
    // Getters
    std::string getName() const { return name; }
    std::map<uint32_t, std::shared_ptr<Interface>> getInterfaces() const { return interfaces; }
    
    std::shared_ptr<Interface> getInterface(uint32_t portNumber) const;
    
    std::shared_ptr<Interface> addInterface();
    
    std::shared_ptr<Interface> addInterface(const std::string& macAddress);
    
    void removeInterface(uint32_t portNumber);
    
    uint32_t getPortNumber(const std::shared_ptr<Interface>& iface) const;
    
    virtual void handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) = 0;
    
    virtual std::string getType() const = 0;
    
    virtual void printInfo() const;
    
    virtual std::string toJson() const = 0;
};

// Host Node
class Host : public Node {
private:
    std::string ipAddress;
    std::string defaultGateway;

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
};

// Switch Node
class Switch : public Node {
private:
    uint32_t numPorts;

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
};

// Router Node
class Router : public Node {
public:
    Router(const std::string& name);
    
    // Override virtual methods
    void handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) override;
    std::string getType() const override { return "router"; }
    std::string toJson() const override;
    void printInfo() const override;
};

} // namespace magi

#endif // NODE_HPP
