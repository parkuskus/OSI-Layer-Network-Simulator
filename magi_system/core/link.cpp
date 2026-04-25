#include "link.hpp"
#include "interface.hpp"
#include <iostream>

namespace magi {

Link::Link(std::shared_ptr<Interface> interfaceA,std::shared_ptr<Interface> interfaceB, uint32_t delayMs): interfaceA(interfaceA), interfaceB(interfaceB), delayMs(delayMs) {

}

std::shared_ptr<Link> Link::create(std::shared_ptr<Interface> interfaceA, std::shared_ptr<Interface> interfaceB, uint32_t delayMs) {
    auto link = std::shared_ptr<Link>(new Link(interfaceA, interfaceB, delayMs));
    
    if (interfaceA) interfaceA->setLink(link);
    if (interfaceB) interfaceB->setLink(link);
    
    return link;
}

void Link::transmit(Interface* senderInterface, const std::vector<uint8_t>& rawBytes) {
    std::shared_ptr<Interface> receiver = nullptr;
    
    if (senderInterface == interfaceA.get()) {
        receiver = interfaceB;
    } else if (senderInterface == interfaceB.get()) {
        receiver = interfaceA;
    }
    
    if (!receiver) {
        return; 
    }
    
    if (delayMs > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    
    receiver->receive(rawBytes);
}

bool Link::hasInterface(const std::shared_ptr<Interface>& iface) const {
    return (iface == interfaceA) || (iface == interfaceB);
}

std::shared_ptr<Interface> Link::getOtherEnd(const std::shared_ptr<Interface>& iface) const {
    if (iface == interfaceA) return interfaceB;
    if (iface == interfaceB) return interfaceA;
    return nullptr;
}

void Link::disconnect() {
    if (interfaceA) {
        interfaceA->setLink(nullptr);
    }
    if (interfaceB) {
        interfaceB->setLink(nullptr);
    }
    interfaceA = nullptr;
    interfaceB = nullptr;
}

} 
