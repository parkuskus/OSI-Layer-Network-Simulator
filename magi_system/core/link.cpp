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
    auto a = interfaceA.lock();
    auto b = interfaceB.lock();

    if (a && senderInterface == a.get()) {
        receiver = b;
    } else if (b && senderInterface == b.get()) {
        receiver = a;
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
    auto a = interfaceA.lock();
    auto b = interfaceB.lock();
    return (a && a == iface) || (b && b == iface);
}

std::shared_ptr<Interface> Link::getOtherEnd(const std::shared_ptr<Interface>& iface) const {
    auto a = interfaceA.lock();
    auto b = interfaceB.lock();
    if (a && iface == a) return b;
    if (b && iface == b) return a;
    return nullptr;
}

void Link::disconnect() {
    if (auto a = interfaceA.lock()) {
        a->setLink(nullptr);
    }
    if (auto b = interfaceB.lock()) {
        b->setLink(nullptr);
    }
    interfaceA.reset();
    interfaceB.reset();
}

} 
