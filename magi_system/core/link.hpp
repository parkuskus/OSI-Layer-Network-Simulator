#ifndef LINK_HPP
#define LINK_HPP

#include <cstdint>
#include <memory>
#include <vector>
#include <chrono>
#include <thread>

namespace magi
{

    class Interface;

    class Link : public std::enable_shared_from_this<Link>
    {
    private:
        std::weak_ptr<Interface> interfaceA;
        std::weak_ptr<Interface> interfaceB;
        uint32_t delayMs;
        uint32_t mtu;

        Link(std::shared_ptr<Interface> interfaceA,
             std::shared_ptr<Interface> interfaceB,
             uint32_t delayMs,
             uint32_t mtu);

    public:
        static std::shared_ptr<Link> create(std::shared_ptr<Interface> interfaceA,
                                            std::shared_ptr<Interface> interfaceB,
                                            uint32_t delayMs = 0,
                                            uint32_t mtu = 1500);

        // Getters
        std::shared_ptr<Interface> getInterfaceA() const { return interfaceA.lock(); }
        std::shared_ptr<Interface> getInterfaceB() const { return interfaceB.lock(); }
        uint32_t getDelay() const { return delayMs; }
        uint32_t getMtu() const { return mtu; }

        // Setters
        void setDelay(uint32_t delay) { delayMs = delay; }
        void setMtu(uint32_t newMtu) { mtu = newMtu; }

        void transmit(Interface *senderInterface, const std::vector<uint8_t> &rawBytes);

        bool hasInterface(const std::shared_ptr<Interface> &iface) const;

        std::shared_ptr<Interface> getOtherEnd(const std::shared_ptr<Interface> &iface) const;

        void disconnect();
    };

} // namespace magi

#endif // LINK_HPP
