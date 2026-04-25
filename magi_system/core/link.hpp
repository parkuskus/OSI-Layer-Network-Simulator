#ifndef LINK_HPP
#define LINK_HPP

#include <cstdint>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

namespace magi {

// Forward declaration
class Interface;

// Link merepresentasikan kabel virtual yang menghubungkan dua Interface
class Link : public std::enable_shared_from_this<Link> {
private:
    std::shared_ptr<Interface> interfaceA;
    std::shared_ptr<Interface> interfaceB;
    uint32_t delayMs;  // Propagation delay dalam milidetik
    
    // Private constructor - use create() factory method
    Link(std::shared_ptr<Interface> interfaceA, 
         std::shared_ptr<Interface> interfaceB, 
         uint32_t delayMs);

public:
    // Factory method - creates link and attaches to interfaces
    static std::shared_ptr<Link> create(std::shared_ptr<Interface> interfaceA, 
                                        std::shared_ptr<Interface> interfaceB, 
                                        uint32_t delayMs = 0);
    
    // Getters
    std::shared_ptr<Interface> getInterfaceA() const { return interfaceA; }
    std::shared_ptr<Interface> getInterfaceB() const { return interfaceB; }
    uint32_t getDelay() const { return delayMs; }
    
    // Setters
    void setDelay(uint32_t delay) { delayMs = delay; }
    
    // Melempar byte secara langsung ke ujung antarmuka kabel satunya
    // dengan simulasi propagation delay (blocking secara sekuensial)
    void transmit(Interface* senderInterface, const std::vector<uint8_t>& rawBytes);
    
    // Cek apakah link terhubung ke interface tertentu
    bool hasInterface(const std::shared_ptr<Interface>& iface) const;
    
    // Mendapatkan interface di ujung lain
    std::shared_ptr<Interface> getOtherEnd(const std::shared_ptr<Interface>& iface) const;
    
    // Disconnect link dari kedua interface
    void disconnect();
};

} // namespace magi

#endif // LINK_HPP
