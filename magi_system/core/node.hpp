#ifndef NODE_HPP
#define NODE_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "layer3/icmp.hpp"
#include "layer3/ipv4.hpp"
#include "layer3/acl.hpp"
#include "layer3/nat.hpp"
#include "layer3/rip.hpp"

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

struct RouterRipRoute {
    std::string destinationCidr;
    std::string nextHopIp;
    uint32_t outPortNumber;
    int outVlanId;
    uint8_t metric;
    int ageTimer;
};

struct RouterResolvedRoute {
    std::string destinationCidr;
    uint8_t prefixLength;
    std::string nextHopIp;
    uint32_t outPortNumber;
    int outVlanId;
    bool connected;
    std::string routeSource;
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
    
    // ACL and NAT
    ACLList aclIngress;  // Ingress ACL (applied to incoming packets)
    ACLList aclEgress;   // Egress ACL (applied to outgoing packets)
    NATTable natTable;   // NAT address translation table
    std::set<uint32_t> natInsideInterfaces;  // Ports marked as "inside" for NAT
    std::set<uint32_t> natOutsideInterfaces; // Ports marked as "outside" for NAT

    // RIP
    std::vector<RouterRipRoute> ripRoutes;
    bool ripEnabled;
    int ripUpdateInterval;
    int ripCommandCounter;
    int ripInvalidTimeout;
    int ripFlushTimeout;

    void processRipUpdate(const std::string& sourceIp, const std::vector<uint8_t>& ripPayload, uint32_t ingressPort, int vlanId);
    void sendRipUpdates();
    void sendRipUpdateOnInterface(uint32_t portNumber, int vlanId);
    void buildRipEntriesForInterface(uint32_t portNumber, int vlanId, std::vector<RipEntry>& entries) const;

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
    
    // Helper methods for NAT and ACL
    bool extractPortsFromPayload(const std::vector<uint8_t>& payload, uint8_t protocol, uint16_t& srcPort, uint16_t& dstPort) const;
    bool applyIngressACL(const IPv4Packet& packet, uint8_t protocol, uint16_t srcPort, uint16_t dstPort) const;
    bool applyEgressACL(const IPv4Packet& packet, uint8_t protocol, uint16_t srcPort, uint16_t dstPort) const;
    IPv4Packet applyNATTranslation(const IPv4Packet& packet, bool isOutgoing, uint32_t ingressPort);

public:
    Router(const std::string& name, uint32_t numPorts = 4);

    void configureInterface(uint32_t portNumber, int vlanId, const std::string& cidr);
    bool addRoute(const std::string& destinationCidr,
                  const std::string& nextHopIp,
                  uint32_t outPortNumber,
                  int outVlanId);
    void printRoutingTable() const;
    void printArpCache() const;

    // RIP methods
    void enableRip();
    void disableRip();
    bool isRipEnabled() const { return ripEnabled; }
    void triggerRipUpdate();
    void ageRipRoutes();
    void printRipRoutes() const;

    // ACL methods
    int addIngressACLRule(const ACLRule& rule);
    int addEgressACLRule(const ACLRule& rule);
    bool removeIngressACLRule(int ruleId);
    bool removeEgressACLRule(int ruleId);
    void clearIngressACL();
    void clearEgressACL();
    void printIngressACL() const;
    void printEgressACL() const;

    // NAT methods
    int addStaticNAT(const std::string& internalIp, uint16_t internalPort,
                     const std::string& externalIp, uint16_t externalPort,
                     uint8_t protocol);
    int addDynamicNAT(const std::string& internalIp, uint16_t internalPort,
                      const std::string& externalIp,
                      uint8_t protocol);
    bool removeNAT(const std::string& internalIp, uint16_t internalPort, uint8_t protocol);
    void clearNAT();
    void printNAT() const;
    void setNATInside(uint32_t portNumber);
    void setNATOutside(uint32_t portNumber);

    void handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) override;
    std::string getType() const override { return "router"; }
    std::string toJson() const override;
    void printInfo() const override;
};

}

#endif