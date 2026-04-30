#ifndef LINK_HPP
#define LINK_HPP

#include <cstdint>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

namespace magi {

class Interface;

class Link : public std::enable_shared_from_this<Link> {
private:
    std::weak_ptr<Interface> interfaceA;
    std::weak_ptr<Interface> interfaceB;
    uint32_t delayMs;  
    
    Link(std::shared_ptr<Interface> interfaceA, 
         std::shared_ptr<Interface> interfaceB, 
         uint32_t delayMs);

public:
    static std::shared_ptr<Link> create(std::shared_ptr<Interface> interfaceA, 
                                        std::shared_ptr<Interface> interfaceB, 
                                        uint32_t delayMs = 0);
    
    // Getters
    std::shared_ptr<Interface> getInterfaceA() const { return interfaceA.lock(); }
    std::shared_ptr<Interface> getInterfaceB() const { return interfaceB.lock(); }
    uint32_t getDelay() const { return delayMs; }
    
    // Setters
    void setDelay(uint32_t delay) { delayMs = delay; }
    
    void transmit(Interface* senderInterface, const std::vector<uint8_t>& rawBytes);
    
    bool hasInterface(const std::shared_ptr<Interface>& iface) const;
    
    std::shared_ptr<Interface> getOtherEnd(const std::shared_ptr<Interface>& iface) const;
    
    void disconnect();
};

} // namespace magi

#endif // LINK_HPP
