#ifndef INTERFACE_HPP
#define INTERFACE_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace magi {

class Node;
class Link;

class Interface : public std::enable_shared_from_this<Interface> {
private:
    Node* node;                    
    uint32_t portNumber;         
    std::string macAddress;       
    std::shared_ptr<Link> link;    

public:
    // Constructor
    Interface(Node* node, uint32_t portNumber, const std::string& macAddress);
    
    // Getters
    Node* getNode() const { return node; }
    uint32_t getPortNumber() const { return portNumber; }
    std::string getMacAddress() const { return macAddress; }
    std::shared_ptr<Link> getLink() const { return link; }
    
    // Setters
    void setLink(std::shared_ptr<Link> newLink) { link = newLink; }
    void setMacAddress(const std::string& mac) { macAddress = mac; }
    
    void send(const std::vector<uint8_t>& rawBytes);
    
    void receive(const std::vector<uint8_t>& rawBytes);
    
    bool isConnected() const { return link != nullptr; }
    
    static std::string generateMacAddress();
};

} 

#endif 
