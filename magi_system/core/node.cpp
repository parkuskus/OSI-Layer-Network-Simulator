#include "node.hpp"

#include "interface.hpp"
#include "link.hpp"
#include "layer2/arp.hpp"
#include "layer2/ethernet.hpp"
#include "layer3/ip_utils.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace magi {

namespace {

const short kEtherTypeIpv4 = static_cast<short>(0x0800);
const short kEtherTypeArp = static_cast<short>(0x0806);
const std::string kBroadcastMac = "ff:ff:ff:ff:ff:ff";

}  // namespace

Node::Node(const std::string& name)
    : name(name), nextPortNumber(1) {
}

std::shared_ptr<Interface> Node::getInterface(uint32_t portNumber) const {
    std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.find(portNumber);
    if (it != interfaces.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Interface> Node::addInterface() {
    return addInterface(Interface::generateMacAddress());
}

std::shared_ptr<Interface> Node::addInterface(const std::string& macAddress) {
    const uint32_t portNum = nextPortNumber++;
    std::shared_ptr<Interface> iface = std::make_shared<Interface>(this, portNum, macAddress);
    interfaces[portNum] = iface;
    return iface;
}

void Node::removeInterface(uint32_t portNumber) {
    std::map<uint32_t, std::shared_ptr<Interface>>::iterator it = interfaces.find(portNumber);
    if (it != interfaces.end()) {
        if (it->second->getLink()) {
            it->second->getLink()->disconnect();
        }
        interfaces.erase(it);
    }
}

uint32_t Node::getPortNumber(const std::shared_ptr<Interface>& iface) const {
    for (std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.begin();
         it != interfaces.end();
         ++it) {
        if (it->second == iface) {
            return it->first;
        }
    }
    return 0;
}

void Node::printInfo() const {
    std::cout << "[" << getType() << ": " << name << "]" << std::endl;
    std::cout << "  Interfaces:" << std::endl;
    for (std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.begin();
         it != interfaces.end();
         ++it) {
        std::cout << "    Port " << it->first << ": " << it->second->getMacAddress();
        if (it->second->isConnected()) {
            std::cout << " (Connected)";
        } else {
            std::cout << " (Disconnected)";
        }
        std::cout << std::endl;
    }
}

Router::Router(const std::string& name, uint32_t numPorts)
    : Node(name), numPorts(numPorts), nextIpIdentification(1) {
    for (uint32_t i = 0; i < numPorts; ++i) {
        addInterface();
    }
}

std::string Router::makeArpKey(uint32_t portNumber, int vlanId, const std::string& ip) const {
    std::stringstream ss;
    ss << portNumber << ":" << vlanId << ":" << ip;
    return ss.str();
}

const RouterLogicalInterface* Router::getLogicalInterface(uint32_t portNumber, int vlanId) const {
    const std::string key = iputil::makeLogicalInterfaceKey(portNumber, vlanId);
    std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.find(key);
    if (it == logicalInterfaces.end()) {
        return nullptr;
    }
    return &it->second;
}

const RouterLogicalInterface* Router::findInterfaceByIp(const std::string& ip) const {
    for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
         it != logicalInterfaces.end();
         ++it) {
        if (iputil::stripCidr(it->second.cidr) == ip) {
            return &it->second;
        }
    }
    return nullptr;
}

std::vector<RouterResolvedRoute> Router::buildRoutingTable() const {
    std::vector<RouterResolvedRoute> routes;

    for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
         it != logicalInterfaces.end();
         ++it) {
        const std::string cidr = iputil::canonicalCidr(it->second.cidr);
        if (cidr.empty()) {
            continue;
        }

        RouterResolvedRoute route;
        route.destinationCidr = cidr;
        route.prefixLength = iputil::prefixLength(cidr, 32);
        route.nextHopIp = "";
        route.outPortNumber = it->second.portNumber;
        route.outVlanId = it->second.vlanId;
        route.connected = true;
        routes.push_back(route);
    }

    for (size_t i = 0; i < staticRoutes.size(); ++i) {
        const std::string cidr = iputil::canonicalCidr(staticRoutes[i].destinationCidr);
        if (cidr.empty()) {
            continue;
        }

        RouterResolvedRoute route;
        route.destinationCidr = cidr;
        route.prefixLength = iputil::prefixLength(cidr, 32);
        route.nextHopIp = staticRoutes[i].nextHopIp;
        route.outPortNumber = staticRoutes[i].outPortNumber;
        route.outVlanId = staticRoutes[i].outVlanId;
        route.connected = false;
        routes.push_back(route);
    }

    std::sort(routes.begin(), routes.end(),
              [](const RouterResolvedRoute& left, const RouterResolvedRoute& right) {
                  if (left.prefixLength != right.prefixLength) {
                      return left.prefixLength > right.prefixLength;
                  }
                  if (left.connected != right.connected) {
                      return left.connected;
                  }
                  return left.destinationCidr < right.destinationCidr;
              });

    return routes;
}

bool Router::findRoute(const std::string& destIp, RouterResolvedRoute& route) const {
    const std::vector<RouterResolvedRoute> routes = buildRoutingTable();
    for (size_t i = 0; i < routes.size(); ++i) {
        if (iputil::ipInCidr(destIp, routes[i].destinationCidr)) {
            route = routes[i];
            return true;
        }
    }
    return false;
}

void Router::configureInterface(uint32_t portNumber, int vlanId, const std::string& cidr) {
    if (!getInterface(portNumber)) {
        return;
    }

    RouterLogicalInterface logicalInterface;
    logicalInterface.portNumber = portNumber;
    logicalInterface.vlanId = vlanId;
    logicalInterface.cidr = cidr;
    logicalInterfaces[iputil::makeLogicalInterfaceKey(portNumber, vlanId)] = logicalInterface;
}

bool Router::addRoute(const std::string& destinationCidr,
                      const std::string& nextHopIp,
                      uint32_t outPortNumber,
                      int outVlanId) {
    if (!iputil::isValidCidr(destinationCidr) || !iputil::isValidIp(nextHopIp)) {
        return false;
    }
    if (!getInterface(outPortNumber) || !getLogicalInterface(outPortNumber, outVlanId)) {
        return false;
    }

    RouterStaticRoute route;
    route.destinationCidr = iputil::canonicalCidr(destinationCidr);
    route.nextHopIp = nextHopIp;
    route.outPortNumber = outPortNumber;
    route.outVlanId = outVlanId;
    staticRoutes.push_back(route);
    return true;
}

void Router::printRoutingTable() const {
    const std::vector<RouterResolvedRoute> routes = buildRoutingTable();

    std::cout << "[Routing Table: " << name << "]" << std::endl;
    if (routes.empty()) {
        std::cout << "  (empty)" << std::endl;
        return;
    }

    std::cout << "  Destination        Next Hop         Interface   Type" << std::endl;
    std::cout << "  -----------------  ---------------  ----------  ---------" << std::endl;
    for (size_t i = 0; i < routes.size(); ++i) {
        std::stringstream ifaceSpec;
        ifaceSpec << routes[i].outPortNumber;
        if (routes[i].outVlanId != iputil::kUntaggedVlan) {
            ifaceSpec << "." << routes[i].outVlanId;
        }

        std::cout << "  " << std::setw(17) << std::left << routes[i].destinationCidr
                  << "  " << std::setw(15) << (routes[i].nextHopIp.empty() ? "direct" : routes[i].nextHopIp)
                  << "  " << std::setw(10) << ifaceSpec.str()
                  << "  " << (routes[i].connected ? "connected" : "static") << std::endl;
    }
}

void Router::printArpCache() const {
    std::cout << "[ARP Cache: " << name << "]" << std::endl;
    if (arpCache.empty()) {
        std::cout << "  (empty)" << std::endl;
        return;
    }

    for (std::map<std::string, std::string>::const_iterator it = arpCache.begin();
         it != arpCache.end();
         ++it) {
        std::cout << "  " << it->first << " -> " << it->second << std::endl;
    }
}

void Router::sendArpRequest(uint32_t outPortNumber, int outVlanId, const std::string& targetIp) {
    std::shared_ptr<Interface> iface = getInterface(outPortNumber);
    const RouterLogicalInterface* logicalInterface = getLogicalInterface(outPortNumber, outVlanId);
    if (!iface || !logicalInterface) {
        return;
    }

    ARPMessage request;
    request.opcode = 1;
    request.senderMac = iface->getMacAddress();
    request.senderIp = iputil::stripCidr(logicalInterface->cidr);
    request.targetMac = "00:00:00:00:00:00";
    request.targetIp = targetIp;

    EthernetFrame frame;
    frame.dstMac = kBroadcastMac;
    frame.srcMac = iface->getMacAddress();
    frame.etherType = kEtherTypeArp;
    frame.vlanId = outVlanId;
    frame.payload = request.toBytes();
    iface->send(frame.toBytes());
}

bool Router::sendPacketOut(const IPv4Packet& packet,
                           const std::string& nextHopIp,
                           uint32_t outPortNumber,
                           int outVlanId) {
    std::shared_ptr<Interface> iface = getInterface(outPortNumber);
    if (!iface || !iface->isConnected()) {
        return false;
    }

    const std::string key = makeArpKey(outPortNumber, outVlanId, nextHopIp);
    std::map<std::string, std::string>::const_iterator arpIt = arpCache.find(key);
    if (arpIt != arpCache.end()) {
        EthernetFrame frame;
        frame.dstMac = arpIt->second;
        frame.srcMac = iface->getMacAddress();
        frame.etherType = kEtherTypeIpv4;
        frame.vlanId = outVlanId;
        frame.payload = packet.toBytes();
        iface->send(frame.toBytes());
        return true;
    }

    RouterPendingPacket pending;
    pending.packet = packet;
    pending.nextHopIp = nextHopIp;
    pending.outPortNumber = outPortNumber;
    pending.outVlanId = outVlanId;

    std::vector<RouterPendingPacket>& queue = pendingPackets[key];
    queue.push_back(pending);
    if (queue.size() == 1) {
        sendArpRequest(outPortNumber, outVlanId, nextHopIp);
    }
    return true;
}

void Router::flushQueuedPackets(uint32_t outPortNumber, int outVlanId, const std::string& nextHopIp) {
    const std::string key = makeArpKey(outPortNumber, outVlanId, nextHopIp);
    std::map<std::string, std::vector<RouterPendingPacket>>::iterator queueIt = pendingPackets.find(key);
    std::map<std::string, std::string>::const_iterator arpIt = arpCache.find(key);
    std::shared_ptr<Interface> iface = getInterface(outPortNumber);
    if (queueIt == pendingPackets.end() || arpIt == arpCache.end() || !iface) {
        return;
    }

    for (size_t i = 0; i < queueIt->second.size(); ++i) {
        EthernetFrame frame;
        frame.dstMac = arpIt->second;
        frame.srcMac = iface->getMacAddress();
        frame.etherType = kEtherTypeIpv4;
        frame.vlanId = outVlanId;
        frame.payload = queueIt->second[i].packet.toBytes();
        iface->send(frame.toBytes());
    }

    pendingPackets.erase(queueIt);
}

bool Router::sendIcmpTo(const std::string& dstIp, ICMPMessage icmp) {
    RouterResolvedRoute route;
    if (!findRoute(dstIp, route)) {
        return false;
    }

    const RouterLogicalInterface* logicalInterface = getLogicalInterface(route.outPortNumber, route.outVlanId);
    if (!logicalInterface) {
        return false;
    }

    icmp.updateChecksum();

    IPv4Packet packet;
    packet.srcIp = iputil::stripCidr(logicalInterface->cidr);
    packet.dstIp = dstIp;
    packet.protocol = 1;
    packet.ttl = 64;
    packet.identification = nextIpIdentification++;
    packet.payload = icmp.toBytes();
    packet.updateChecksum();

    const std::string nextHopIp = route.nextHopIp.empty() ? dstIp : route.nextHopIp;
    return sendPacketOut(packet, nextHopIp, route.outPortNumber, route.outVlanId);
}

void Router::sendIcmpError(const IPv4Packet& originalPacket, uint8_t type, uint8_t code) {
    ICMPMessage icmp;
    icmp.type = type;
    icmp.code = code;
    icmp.identifier = 0;
    icmp.sequenceNumber = 0;

    const std::vector<uint8_t> originalBytes = originalPacket.toBytes();
    const size_t copyLength = std::min<size_t>(originalBytes.size(), 28);
    icmp.payload.assign(originalBytes.begin(), originalBytes.begin() + static_cast<long>(copyLength));

    sendIcmpTo(originalPacket.srcIp, icmp);
}

void Router::sendIcmpEchoReply(const IPv4Packet& requestPacket, const ICMPMessage& requestMessage) {
    ICMPMessage reply;
    reply.type = kICMPEchoReply;
    reply.code = 0;
    reply.identifier = requestMessage.identifier;
    reply.sequenceNumber = requestMessage.sequenceNumber;
    reply.payload = requestMessage.payload;

    sendIcmpTo(requestPacket.srcIp, reply);
}

void Router::handleReceive(Interface* incomingInterface, const std::vector<uint8_t>& rawBytes) {
    if (!incomingInterface) {
        return;
    }

    EthernetFrame frame;
    try {
        frame.fromBytes(rawBytes);
    } catch (...) {
        return;
    }

    if (frame.dstMac != kBroadcastMac && frame.dstMac != incomingInterface->getMacAddress()) {
        return;
    }

    if (frame.etherType == kEtherTypeArp) {
        handleArpFrame(incomingInterface, frame);
        return;
    }

    if (frame.etherType == kEtherTypeIpv4) {
        handleIpv4Frame(incomingInterface, frame);
    }
}

void Router::handleArpFrame(Interface* incomingInterface, const EthernetFrame& frame) {
    ARPMessage arp;
    try {
        arp.fromBytes(frame.payload);
    } catch (...) {
        return;
    }

    const uint32_t portNumber = incomingInterface->getPortNumber();
    arpCache[makeArpKey(portNumber, frame.vlanId, arp.senderIp)] = arp.senderMac;

    const RouterLogicalInterface* logicalInterface = getLogicalInterface(portNumber, frame.vlanId);
    if (arp.opcode == 1 && logicalInterface != nullptr &&
        arp.targetIp == iputil::stripCidr(logicalInterface->cidr)) {
        ARPMessage reply;
        reply.opcode = 2;
        reply.senderMac = incomingInterface->getMacAddress();
        reply.senderIp = iputil::stripCidr(logicalInterface->cidr);
        reply.targetMac = arp.senderMac;
        reply.targetIp = arp.senderIp;

        EthernetFrame out;
        out.dstMac = arp.senderMac;
        out.srcMac = incomingInterface->getMacAddress();
        out.etherType = kEtherTypeArp;
        out.vlanId = frame.vlanId;
        out.payload = reply.toBytes();
        incomingInterface->send(out.toBytes());
        return;
    }

    if (arp.opcode == 2) {
        flushQueuedPackets(portNumber, frame.vlanId, arp.senderIp);
    }
}

void Router::handleIpv4Frame(Interface* incomingInterface, const EthernetFrame& frame) {
    IPv4Packet packet;
    try {
        packet.fromBytes(frame.payload);
    } catch (...) {
        return;
    }

    if (!packet.validateChecksum()) {
        return;
    }

    const uint32_t ingressPort = incomingInterface->getPortNumber();
    arpCache[makeArpKey(ingressPort, frame.vlanId, packet.srcIp)] = frame.srcMac;

    const RouterLogicalInterface* localInterface = findInterfaceByIp(packet.dstIp);
    if (localInterface != nullptr) {
        if (packet.protocol != 1) {
            return;
        }

        ICMPMessage icmp;
        try {
            icmp.fromBytes(packet.payload);
        } catch (...) {
            return;
        }

        if (!icmp.validateChecksum()) {
            return;
        }

        if (icmp.type == kICMPEchoRequest) {
            sendIcmpEchoReply(packet, icmp);
        }
        return;
    }

    if (packet.ttl <= 1) {
        sendIcmpError(packet, kICMPTimeExceeded, 0);
        return;
    }

    RouterResolvedRoute route;
    if (!findRoute(packet.dstIp, route)) {
        sendIcmpError(packet, kICMPDestinationUnreachable, 0);
        return;
    }

    IPv4Packet forwarded = packet;
    forwarded.ttl = static_cast<uint8_t>(forwarded.ttl - 1);
    forwarded.updateChecksum();

    const std::string nextHopIp = route.nextHopIp.empty() ? forwarded.dstIp : route.nextHopIp;
    sendPacketOut(forwarded, nextHopIp, route.outPortNumber, route.outVlanId);
}

std::string Router::toJson() const {
    std::stringstream ss;
    ss << "    {\n";
    ss << "      \"name\": \"" << name << "\",\n";
    ss << "      \"num_ports\": " << numPorts << ",\n";
    ss << "      \"interfaces\": [\n";
    bool first = true;
    for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
         it != logicalInterfaces.end();
         ++it) {
        if (!first) {
            ss << ",\n";
        }
        first = false;
        ss << "        {\"endpoint\": \"" << it->second.portNumber;
        if (it->second.vlanId != iputil::kUntaggedVlan) {
            ss << "." << it->second.vlanId;
        }
        ss << "\", \"ip_address\": \"" << it->second.cidr << "\"}";
    }
    ss << "\n      ],\n";
    ss << "      \"routing_table\": [\n";
    first = true;
    for (size_t i = 0; i < staticRoutes.size(); ++i) {
        if (!first) {
            ss << ",\n";
        }
        first = false;
        ss << "        {\"destination\": \"" << staticRoutes[i].destinationCidr
           << "\", \"next_hop\": \"" << staticRoutes[i].nextHopIp
           << "\", \"out_interface\": \"" << staticRoutes[i].outPortNumber;
        if (staticRoutes[i].outVlanId != iputil::kUntaggedVlan) {
            ss << "." << staticRoutes[i].outVlanId;
        }
        ss << "\"}";
    }
    ss << "\n      ]\n";
    ss << "    }";
    return ss.str();
}

void Router::printInfo() const {
    std::cout << "[Router: " << name << "]" << std::endl;
    std::cout << "  Physical Ports:" << std::endl;
    for (std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.begin();
         it != interfaces.end();
         ++it) {
        std::cout << "    Port " << it->first << ": " << it->second->getMacAddress();
        if (it->second->isConnected()) {
            std::cout << " (Connected)";
        } else {
            std::cout << " (Disconnected)";
        }
        std::cout << std::endl;
    }

    std::cout << "  Logical Interfaces:" << std::endl;
    if (logicalInterfaces.empty()) {
        std::cout << "    (none)" << std::endl;
    } else {
        for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
             it != logicalInterfaces.end();
             ++it) {
            std::cout << "    Port " << it->second.portNumber;
            if (it->second.vlanId != iputil::kUntaggedVlan) {
                std::cout << "." << it->second.vlanId;
            }
            std::cout << " -> " << it->second.cidr << std::endl;
        }
    }

    std::cout << std::endl;
    printRoutingTable();
}

}