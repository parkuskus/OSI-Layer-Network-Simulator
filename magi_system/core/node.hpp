#ifndef NODE_HPP
#define NODE_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "layer3/icmp.hpp"
#include "layer3/ipv4.hpp"

namespace magi {

class EthernetFrame;
class Interface;
class Packet;

struct RouterLogicalInterface {
    uint32_t portNumber;
    int vlanId;
    std::string cidr;
};

struct RouterStaticRoute {
    std::string destinationCidr;
    std::string nextHopIp;
    uint32_t outPortNumber;
    int outVlanId;
};

struct RouterResolvedRoute {
    std::string destinationCidr;
    uint8_t prefixLength;
    std::string nextHopIp;
    uint32_t outPortNumber;
    int outVlanId;
    bool connected;
};

struct RouterPendingPacket {
    IPv4Packet packet;
    std::string nextHopIp;
    uint32_t outPortNumber;
    int outVlanId;
};

class Node {
protected:
    std::string name;
    std::map<uint32_t, std::shared_ptr<Interface>> interfaces;
    uint32_t nextPortNumber;

public:
    Node(const std::string& name);
    virtual ~Node() = default;

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

class Router : public Node {
private:
    uint32_t numPorts;
    uint16_t nextIpIdentification;
    std::map<std::string, RouterLogicalInterface> logicalInterfaces;
    std::vector<RouterStaticRoute> staticRoutes;
    std::map<std::string, std::string> arpCache;
    std::map<std::string, std::vector<RouterPendingPacket>> pendingPackets;

    std::string makeArpKey(uint32_t portNumber, int vlanId, const std::string& ip) const;
    const RouterLogicalInterface* getLogicalInterface(uint32_t portNumber, int vlanId) const;
    const RouterLogicalInterface* findInterfaceByIp(const std::string& ip) const;
    std::vector<RouterResolvedRoute> buildRoutingTable() const;
    bool findRoute(const std::string& destIp, RouterResolvedRoute& route) const;
    bool sendPacketOut(const IPv4Packet& packet,
                       const std::string& nextHopIp,
                       uint32_t outPortNumber,
                       int outVlanId);
    void sendArpRequest(uint32_t outPortNumber, int outVlanId, const std::string& targetIp);
    void flushQueuedPackets(uint32_t outPortNumber, int outVlanId, const std::string& nextHopIp);
    bool sendIcmpTo(const std::string& dstIp, ICMPMessage icmp);
    void sendIcmpError(const IPv4Packet& originalPacket, uint8_t type, uint8_t code);
    void sendIcmpEchoReply(const IPv4Packet& requestPacket, const ICMPMessage& requestMessage);
    void handleArpFrame(Interface* incomingInterface, const EthernetFrame& frame);
    void handleIpv4Frame(Interface* incomingInterface, const EthernetFrame& frame);

public:
    Router(const std::string& name, uint32_t numPorts = 4);

    void configureInterface(uint32_t portNumber, int vlanId, const std::string& cidr);
    bool addRoute(const std::string& destinationCidr,
                  const std::string& nextHopIp,
                  uint32_t outPortNumber,
                  int outVlanId);
    void printRoutingTable() const;
    void printArpCache() const;

    void handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) override;
    std::string getType() const override { return "router"; }
    std::string toJson() const override;
    void printInfo() const override;
};

}

#endif