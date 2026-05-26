#include "node.hpp"

#include "interface.hpp"
#include "link.hpp"
#include "event_log.hpp"
#include "layer2/arp.hpp"
#include "layer2/ethernet.hpp"
#include "layer3/ip_utils.hpp"
#include "layer3/rip.hpp"
#include "layer4/udp.hpp"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace magi
{

    namespace
    {

        const short kEtherTypeIpv4 = static_cast<short>(0x0800);
        const short kEtherTypeArp = static_cast<short>(0x0806);
        const std::string kBroadcastMac = "ff:ff:ff:ff:ff:ff";
        const std::string kRipMulticastAddr = "224.0.0.9";
        const std::string kRipBroadcastAddr = "255.255.255.255";
        const uint32_t kDefaultLinkMtu = 1500;

        uint32_t getLinkMtu(const std::shared_ptr<Interface> &iface)
        {
            if (!iface || !iface->getLink())
            {
                return kDefaultLinkMtu;
            }
            return iface->getLink()->getMtu();
        }

        std::vector<IPv4Packet> fragmentIpv4Packet(const IPv4Packet &packet, uint32_t mtu)
        {
            const size_t headerSize = static_cast<size_t>(packet.ihl) * 4;
            if (mtu <= headerSize)
            {
                return std::vector<IPv4Packet>{packet};
            }

            const size_t maxPayloadPerFragment = ((mtu - headerSize) / 8) * 8;
            if (maxPayloadPerFragment == 0 || packet.payload.size() <= maxPayloadPerFragment)
            {
                return std::vector<IPv4Packet>{packet};
            }

            std::vector<IPv4Packet> fragments;
            size_t offset = 0;
            while (offset < packet.payload.size())
            {
                const size_t remaining = packet.payload.size() - offset;
                const size_t chunkSize = (remaining > maxPayloadPerFragment) ? maxPayloadPerFragment : remaining;

                IPv4Packet fragment = packet;
                fragment.flags = static_cast<uint8_t>(packet.flags & static_cast<uint8_t>(~0x1));
                if (remaining > chunkSize)
                {
                    fragment.flags = static_cast<uint8_t>(fragment.flags | 0x1);
                }
                fragment.fragmentOffset = static_cast<uint16_t>(offset / 8);
                fragment.payload.assign(packet.payload.begin() + static_cast<long>(offset),
                                        packet.payload.begin() + static_cast<long>(offset + chunkSize));
                fragment.updateChecksum();
                fragments.push_back(fragment);

                offset += chunkSize;
            }

            return fragments;
        }

    } 

    Node::Node(const std::string &name)
        : name(name), nextPortNumber(1)
    {
    }

    std::shared_ptr<Interface> Node::getInterface(uint32_t portNumber) const
    {
        std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.find(portNumber);
        if (it != interfaces.end())
        {
            return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<Interface> Node::addInterface()
    {
        return addInterface(Interface::generateMacAddress());
    }

    std::shared_ptr<Interface> Node::addInterface(const std::string &macAddress)
    {
        const uint32_t portNum = nextPortNumber++;
        std::shared_ptr<Interface> iface = std::make_shared<Interface>(this, portNum, macAddress);
        interfaces[portNum] = iface;
        return iface;
    }

    void Node::removeInterface(uint32_t portNumber)
    {
        std::map<uint32_t, std::shared_ptr<Interface>>::iterator it = interfaces.find(portNumber);
        if (it != interfaces.end())
        {
            if (it->second->getLink())
            {
                it->second->getLink()->disconnect();
            }
            interfaces.erase(it);
        }
    }

    uint32_t Node::getPortNumber(const std::shared_ptr<Interface> &iface) const
    {
        for (std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.begin();
             it != interfaces.end();
             ++it)
        {
            if (it->second == iface)
            {
                return it->first;
            }
        }
        return 0;
    }

    void Node::printInfo() const
    {
        std::cout << "[" << getType() << ": " << name << "]" << std::endl;
        std::cout << "  Interfaces:" << std::endl;
        for (std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.begin();
             it != interfaces.end();
             ++it)
        {
            std::cout << "    Port " << it->first << ": " << it->second->getMacAddress();
            if (it->second->isConnected())
            {
                std::cout << " (Connected)";
            }
            else
            {
                std::cout << " (Disconnected)";
            }
            std::cout << std::endl;
        }
    }

    Router::Router(const std::string &name, uint32_t numPorts)
        : Node(name), numPorts(numPorts), nextIpIdentification(1),
          ripEnabled(false), ripCommandCounter(0),
          ripInvalidTimeout(6), ripFlushTimeout(9)
    {
        for (uint32_t i = 0; i < numPorts; ++i)
        {
            addInterface();
        }
    }

    std::string Router::makeArpKey(uint32_t portNumber, int vlanId, const std::string &ip) const
    {
        std::stringstream ss;
        ss << portNumber << ":" << vlanId << ":" << ip;
        return ss.str();
    }

    const RouterLogicalInterface *Router::getLogicalInterface(uint32_t portNumber, int vlanId) const
    {
        const std::string key = iputil::makeLogicalInterfaceKey(portNumber, vlanId);
        std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.find(key);
        if (it == logicalInterfaces.end())
        {
            return nullptr;
        }
        return &it->second;
    }

    const RouterLogicalInterface *Router::findInterfaceByIp(const std::string &ip) const
    {
        for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
             it != logicalInterfaces.end();
             ++it)
        {
            if (iputil::stripCidr(it->second.cidr) == ip)
            {
                return &it->second;
            }
        }
        return nullptr;
    }

    std::vector<RouterResolvedRoute> Router::buildRoutingTable() const
    {
        std::vector<RouterResolvedRoute> routes;

        for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
             it != logicalInterfaces.end();
             ++it)
        {
            const std::string cidr = iputil::canonicalCidr(it->second.cidr);
            if (cidr.empty())
            {
                continue;
            }

            RouterResolvedRoute route;
            route.destinationCidr = cidr;
            route.prefixLength = iputil::prefixLength(cidr, 32);
            route.nextHopIp = "";
            route.outPortNumber = it->second.portNumber;
            route.outVlanId = it->second.vlanId;
            route.connected = true;
            route.routeSource = "connected";
            routes.push_back(route);
        }

        for (size_t i = 0; i < staticRoutes.size(); ++i)
        {
            const std::string cidr = iputil::canonicalCidr(staticRoutes[i].destinationCidr);
            if (cidr.empty())
            {
                continue;
            }

            RouterResolvedRoute route;
            route.destinationCidr = cidr;
            route.prefixLength = iputil::prefixLength(cidr, 32);
            route.nextHopIp = staticRoutes[i].nextHopIp;
            route.outPortNumber = staticRoutes[i].outPortNumber;
            route.outVlanId = staticRoutes[i].outVlanId;
            route.connected = false;
            route.routeSource = "static";
            routes.push_back(route);
        }

        for (size_t i = 0; i < ripRoutes.size(); ++i)
        {
            const std::string cidr = iputil::canonicalCidr(ripRoutes[i].destinationCidr);
            if (cidr.empty() || ripRoutes[i].metric >= kRipInfinity)
            {
                continue;
            }

            bool isDuplicateConnected = false;
            for (size_t j = 0; j < routes.size(); ++j)
            {
                if (routes[j].destinationCidr == cidr && routes[j].routeSource == "connected")
                {
                    isDuplicateConnected = true;
                    break;
                }
            }
            if (isDuplicateConnected)
                continue;

            RouterResolvedRoute route;
            route.destinationCidr = cidr;
            route.prefixLength = iputil::prefixLength(cidr, 32);
            route.nextHopIp = ripRoutes[i].nextHopIp;
            route.outPortNumber = ripRoutes[i].outPortNumber;
            route.outVlanId = ripRoutes[i].outVlanId;
            route.connected = false;
            route.routeSource = "rip";
            routes.push_back(route);
        }

        std::sort(routes.begin(), routes.end(),
                  [](const RouterResolvedRoute &left, const RouterResolvedRoute &right)
                  {
                      if (left.prefixLength != right.prefixLength)
                      {
                          return left.prefixLength > right.prefixLength;
                      }
                      static const std::string prioMap[] = {"connected", "static", "rip"};
                      int leftPrio = 3, rightPrio = 3;
                      for (int p = 0; p < 3; ++p)
                      {
                          if (left.routeSource == prioMap[p])
                              leftPrio = p;
                          if (right.routeSource == prioMap[p])
                              rightPrio = p;
                      }
                      if (leftPrio != rightPrio)
                      {
                          return leftPrio < rightPrio;
                      }
                      return left.destinationCidr < right.destinationCidr;
                  });

        return routes;
    }

    bool Router::findRoute(const std::string &destIp, RouterResolvedRoute &route) const
    {
        const std::vector<RouterResolvedRoute> routes = buildRoutingTable();
        for (size_t i = 0; i < routes.size(); ++i)
        {
            if (iputil::ipInCidr(destIp, routes[i].destinationCidr))
            {
                // Skip routes using physically disconnected interfaces
                auto physicalIface = getInterface(routes[i].outPortNumber);
                if (!physicalIface || !physicalIface->isConnected())
                {
                    continue;
                }
                route = routes[i];
                return true;
            }
        }
        return false;
    }

    void Router::configureInterface(uint32_t portNumber, int vlanId, const std::string &cidr)
    {
        if (!getInterface(portNumber))
        {
            return;
        }

        RouterLogicalInterface logicalInterface;
        logicalInterface.portNumber = portNumber;
        logicalInterface.vlanId = vlanId;
        logicalInterface.cidr = cidr;
        logicalInterfaces[iputil::makeLogicalInterfaceKey(portNumber, vlanId)] = logicalInterface;
    }

    bool Router::addRoute(const std::string &destinationCidr,
                          const std::string &nextHopIp,
                          uint32_t outPortNumber,
                          int outVlanId)
    {
        if (!iputil::isValidCidr(destinationCidr) || !iputil::isValidIp(nextHopIp))
        {
            return false;
        }
        if (!getInterface(outPortNumber) || !getLogicalInterface(outPortNumber, outVlanId))
        {
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

    void Router::printRoutingTable() const
    {
        const std::vector<RouterResolvedRoute> routes = buildRoutingTable();

        std::cout << "[Routing Table: " << name << "]" << std::endl;
        if (routes.empty())
        {
            std::cout << "  (empty)" << std::endl;
            return;
        }

        std::cout << "  Destination        Next Hop         Interface   Type" << std::endl;
        std::cout << "  -----------------  ---------------  ----------  ---------" << std::endl;
        for (size_t i = 0; i < routes.size(); ++i)
        {
            std::stringstream ifaceSpec;
            ifaceSpec << routes[i].outPortNumber;
            if (routes[i].outVlanId != iputil::kUntaggedVlan)
            {
                ifaceSpec << "." << routes[i].outVlanId;
            }

            std::cout << "  " << std::setw(17) << std::left << routes[i].destinationCidr
                      << "  " << std::setw(15) << (routes[i].nextHopIp.empty() ? "direct" : routes[i].nextHopIp)
                      << "  " << std::setw(10) << ifaceSpec.str()
                      << "  " << routes[i].routeSource << std::endl;
        }
    }

    void Router::enableRip()
    {
        ripEnabled = true;
        ripCommandCounter = 0;

        std::cout << "[RIP] RIP diaktifkan pada router '" << name << "'" << std::endl;
    }

    void Router::disableRip()
    {
        ripEnabled = false;
        ripRoutes.clear();
        std::cout << "[RIP] RIP dinonaktifkan pada router '" << name << "'" << std::endl;
    }

    void Router::ageRipRoutes()
    {
        if (!ripEnabled)
            return;

        for (size_t i = 0; i < ripRoutes.size();)
        {
            ripRoutes[i].ageTimer++;

            bool isConnected = false;
            for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
                 it != logicalInterfaces.end(); ++it)
            {
                if (iputil::canonicalCidr(it->second.cidr) == ripRoutes[i].destinationCidr)
                {
                    isConnected = true;
                    break;
                }
            }

            if (isConnected)
            {
                ripRoutes[i].ageTimer = 0;
                ++i;
                continue;
            }

            if (ripRoutes[i].ageTimer >= ripFlushTimeout)
            {
                ripRoutes.erase(ripRoutes.begin() + static_cast<long>(i));
                continue;
            }

            if (ripRoutes[i].ageTimer >= ripInvalidTimeout)
            {
                ripRoutes[i].metric = kRipInfinity;
            }

            ++i;
        }
    }

    void Router::buildRipEntriesForInterface(uint32_t portNumber, int vlanId, std::vector<RipEntry> &entries) const
    {
        if (!ripEnabled)
            return;

        std::set<std::string> addedNetworks;

        for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
             it != logicalInterfaces.end(); ++it)
        {
            if (it->second.portNumber == portNumber && it->second.vlanId == vlanId)
                continue;

            iputil::ParsedCidr parsed;
            if (!iputil::parseCidr(it->second.cidr, parsed))
                continue;

            RipEntry entry;
            entry.addressFamilyId = kRipAddressFamilyIp;
            entry.ipAddress = parsed.networkValue;
            entry.subnetMask = parsed.mask;
            entry.metric = 1;
            std::string cidr = iputil::toIp(parsed.networkValue) + "/" + std::to_string(parsed.prefixLength);
            if (addedNetworks.find(cidr) == addedNetworks.end())
            {
                addedNetworks.insert(cidr);
                entries.push_back(entry);
            }
        }

        for (size_t i = 0; i < ripRoutes.size(); ++i)
        {
            if (ripRoutes[i].metric >= kRipInfinity)
                continue;
            if (ripRoutes[i].outPortNumber == portNumber && ripRoutes[i].outVlanId == vlanId)
                continue;

            iputil::ParsedCidr parsed;
            if (!iputil::parseCidr(ripRoutes[i].destinationCidr, parsed))
                continue;

            std::string cidr = iputil::toIp(parsed.networkValue) + "/" + std::to_string(parsed.prefixLength);
            if (addedNetworks.find(cidr) != addedNetworks.end())
                continue;

            RipEntry entry;
            entry.addressFamilyId = kRipAddressFamilyIp;
            entry.ipAddress = parsed.networkValue;
            entry.subnetMask = parsed.mask;
            entry.metric = ripRoutes[i].metric;
            addedNetworks.insert(cidr);
            entries.push_back(entry);
        }
    }

    void Router::sendRipUpdateOnInterface(uint32_t portNumber, int vlanId)
    {
        std::shared_ptr<Interface> iface = getInterface(portNumber);
        const RouterLogicalInterface *li = getLogicalInterface(portNumber, vlanId);
        if (!iface || !li || !iface->isConnected())
            return;
        logEvent("route", name, "", "RIP", "Sending RIP update on port " + std::to_string(portNumber));

        std::vector<RipEntry> entries;
        buildRipEntriesForInterface(portNumber, vlanId, entries);
        if (entries.empty())
            return;

        RipPacket rip;
        rip.command = kRipCommandResponse;
        rip.entries = entries;

        std::vector<uint8_t> ripData = rip.toBytes();

        UDPSegment udp;
        udp.sourcePort = kRipPort;
        udp.destinationPort = kRipPort;
        udp.payload = ripData;
        udp.updateChecksum(iputil::stripCidr(li->cidr), kRipMulticastAddr);

        IPv4Packet ip;
        ip.srcIp = iputil::stripCidr(li->cidr);
        ip.dstIp = kRipMulticastAddr;
        ip.protocol = 17;
        ip.ttl = 1;
        ip.identification = nextIpIdentification++;
        ip.payload = udp.toBytes();
        ip.updateChecksum();

        EthernetFrame frame;
        frame.dstMac = kBroadcastMac;
        frame.srcMac = iface->getMacAddress();
        frame.etherType = kEtherTypeIpv4;
        frame.vlanId = vlanId;
        frame.payload = ip.toBytes();
        iface->send(frame.toBytes());
    }

    void Router::sendRipUpdates()
    {
        if (!ripEnabled)
            return;

        std::set<std::string> sentInterfaces;
        for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
             it != logicalInterfaces.end(); ++it)
        {
            std::string key = iputil::makeLogicalInterfaceKey(it->second.portNumber, it->second.vlanId);
            if (sentInterfaces.find(key) != sentInterfaces.end())
                continue;
            sentInterfaces.insert(key);
            sendRipUpdateOnInterface(it->second.portNumber, it->second.vlanId);
        }
    }

    void Router::processRipUpdate(const std::string &sourceIp, const std::vector<uint8_t> &ripPayload, uint32_t ingressPort, int vlanId)
    {
        if (!ripEnabled)
            return;

        RipPacket rip;
        if (!rip.fromBytes(ripPayload) || rip.command != kRipCommandResponse)
            return;

        std::string sourceRouterIp = sourceIp;

        for (size_t i = 0; i < rip.entries.size(); ++i)
        {
            const RipEntry &entry = rip.entries[i];
            if (entry.addressFamilyId != kRipAddressFamilyIp || entry.metric >= kRipInfinity)
                continue;

            uint32_t prefixLen = 0;
            uint32_t mask = entry.subnetMask;
            for (int b = 31; b >= 0; --b)
            {
                if (mask & (1u << b))
                    prefixLen++;
                else
                    break;
            }

            uint32_t networkAddr = entry.ipAddress & entry.subnetMask;
            std::string destAddr = iputil::toIp(networkAddr);
            std::string destCidr = destAddr + "/" + std::to_string(prefixLen);
            uint32_t newMetric = entry.metric + 1;
            if (newMetric > kRipInfinity)
                newMetric = kRipInfinity;

            bool found = false;
            for (size_t j = 0; j < ripRoutes.size(); ++j)
            {
                if (ripRoutes[j].destinationCidr == destCidr)
                {
                    found = true;
                    if (ripRoutes[j].nextHopIp == sourceRouterIp)
                    {
                        ripRoutes[j].metric = static_cast<uint8_t>(newMetric);
                        ripRoutes[j].ageTimer = 0;
                    }
                    else if (newMetric < ripRoutes[j].metric)
                    {
                        logEvent("route", name, sourceRouterIp, "RIP",
                                 "Better route to " + destCidr + " via " + sourceRouterIp + " metric=" + std::to_string(newMetric));
                        ripRoutes[j].nextHopIp = sourceRouterIp;
                        ripRoutes[j].metric = static_cast<uint8_t>(newMetric);
                        ripRoutes[j].outPortNumber = ingressPort;
                        ripRoutes[j].outVlanId = vlanId;
                        ripRoutes[j].ageTimer = 0;
                    }
                    break;
                }
            }

            if (!found && newMetric < kRipInfinity)
            {
                bool isConnected = false;
                for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
                     it != logicalInterfaces.end(); ++it)
                {
                    if (iputil::canonicalCidr(it->second.cidr) == destCidr)
                    {
                        isConnected = true;
                        break;
                    }
                }
                if (!isConnected)
                {
                    logEvent("route", name, sourceRouterIp, "RIP",
                             "New route " + destCidr + " via " + sourceRouterIp + " metric=" + std::to_string(newMetric));
                    RouterRipRoute newRoute;
                    newRoute.destinationCidr = destCidr;
                    newRoute.nextHopIp = sourceRouterIp;
                    newRoute.outPortNumber = ingressPort;
                    newRoute.outVlanId = vlanId;
                    newRoute.metric = static_cast<uint8_t>(newMetric);
                    newRoute.ageTimer = 0;
                    ripRoutes.push_back(newRoute);
                }
            }
        }
    }

    void Router::triggerRipUpdate()
    {
        if (!ripEnabled)
            return;

        ageRipRoutes();
        sendRipUpdates();
        ripCommandCounter = 0;
    }

    void Router::printRipRoutes() const
    {
        std::cout << "[RIP Routes: " << name << "]" << std::endl;
        if (!ripEnabled)
        {
            std::cout << "  RIP tidak aktif." << std::endl;
            return;
        }

        if (ripRoutes.empty())
        {
            std::cout << "  (empty)" << std::endl;
            return;
        }

        std::cout << "  Destination        Next Hop         Interface   Metric  Age" << std::endl;
        std::cout << "  -----------------  ---------------  ----------  ------  ---" << std::endl;
        for (size_t i = 0; i < ripRoutes.size(); ++i)
        {
            std::stringstream ifaceSpec;
            ifaceSpec << ripRoutes[i].outPortNumber;
            if (ripRoutes[i].outVlanId != iputil::kUntaggedVlan)
            {
                ifaceSpec << "." << ripRoutes[i].outVlanId;
            }

            std::cout << "  " << std::setw(17) << std::left << ripRoutes[i].destinationCidr
                      << "  " << std::setw(15) << (ripRoutes[i].nextHopIp.empty() ? "direct" : ripRoutes[i].nextHopIp)
                      << "  " << std::setw(10) << ifaceSpec.str()
                      << "  " << std::setw(5) << static_cast<int>(ripRoutes[i].metric)
                      << "  " << ripRoutes[i].ageTimer << std::endl;
        }
    }

    void Router::printArpCache() const
    {
        std::cout << "[ARP Cache: " << name << "]" << std::endl;
        if (arpCache.empty())
        {
            std::cout << "  (empty)" << std::endl;
            return;
        }

        for (std::map<std::string, std::string>::const_iterator it = arpCache.begin();
             it != arpCache.end();
             ++it)
        {
            std::cout << "  " << it->first << " -> " << it->second << std::endl;
        }
    }

    int Router::addIngressACLRule(const ACLRule &rule)
    {
        return aclIngress.addRule(rule);
    }

    int Router::addEgressACLRule(const ACLRule &rule)
    {
        return aclEgress.addRule(rule);
    }

    bool Router::removeIngressACLRule(int ruleId)
    {
        return aclIngress.removeRule(ruleId);
    }

    bool Router::removeEgressACLRule(int ruleId)
    {
        return aclEgress.removeRule(ruleId);
    }

    void Router::clearIngressACL()
    {
        aclIngress.clearRules();
    }

    void Router::clearEgressACL()
    {
        aclEgress.clearRules();
    }

    void Router::printIngressACL() const
    {
        aclIngress.printRules();
    }

    void Router::printEgressACL() const
    {
        aclEgress.printRules();
    }

    // NAT Methods
    int Router::addStaticNAT(const std::string &internalIp, uint16_t internalPort,
                             const std::string &externalIp, uint16_t externalPort,
                             uint8_t protocol)
    {
        return natTable.addStaticMapping(internalIp, internalPort, externalIp, externalPort, protocol);
    }

    int Router::addDynamicNAT(const std::string &internalIp, uint16_t internalPort,
                              const std::string &externalIp,
                              uint8_t protocol)
    {
        return natTable.addDynamicMapping(internalIp, internalPort, externalIp, protocol);
    }

    bool Router::removeNAT(const std::string &internalIp, uint16_t internalPort, uint8_t protocol)
    {
        return natTable.removeMapping(internalIp, internalPort, protocol);
    }

    void Router::clearNAT()
    {
        natTable.clearMappings();
    }

    void Router::printNAT() const
    {
        natTable.printMappings();
    }

    void Router::setNATInside(uint32_t portNumber)
    {
        natInsideInterfaces.insert(portNumber);
        natOutsideInterfaces.erase(portNumber);
    }

    void Router::setNATOutside(uint32_t portNumber)
    {
        natOutsideInterfaces.insert(portNumber);
        natInsideInterfaces.erase(portNumber);
    }

    bool Router::extractPortsFromPayload(const std::vector<uint8_t> &payload, uint8_t protocol, uint16_t &srcPort, uint16_t &dstPort) const
    {
        if (protocol != 6 && protocol != 17)
        {
            srcPort = 0;
            dstPort = 0;
            return false;
        }


        if (payload.size() < 4)
        {
            srcPort = 0;
            dstPort = 0;
            return false;
        }

        srcPort = static_cast<uint16_t>((static_cast<uint16_t>(payload[0]) << 8) | payload[1]);
        dstPort = static_cast<uint16_t>((static_cast<uint16_t>(payload[2]) << 8) | payload[3]);
        return true;
    }

    bool Router::applyIngressACL(const IPv4Packet &packet, uint8_t protocol, uint16_t srcPort, uint16_t dstPort) const
    {
        ACLProtocol aclProto = ACLProtocol::ANY;
        if (protocol == 6)
            aclProto = ACLProtocol::TCP;
        else if (protocol == 17)
            aclProto = ACLProtocol::UDP;
        else if (protocol == 1)
            aclProto = ACLProtocol::ICMP;

        return aclIngress.checkPacket(packet.srcIp, packet.dstIp, aclProto, srcPort, dstPort);
    }

    bool Router::applyEgressACL(const IPv4Packet &packet, uint8_t protocol, uint16_t srcPort, uint16_t dstPort) const
    {
        ACLProtocol aclProto = ACLProtocol::ANY;
        if (protocol == 6)
            aclProto = ACLProtocol::TCP;
        else if (protocol == 17)
            aclProto = ACLProtocol::UDP;
        else if (protocol == 1)
            aclProto = ACLProtocol::ICMP;

        return aclEgress.checkPacket(packet.srcIp, packet.dstIp, aclProto, srcPort, dstPort);
    }

    IPv4Packet Router::applyNATTranslation(const IPv4Packet &packet, bool isOutgoing, uint32_t ingressPort)
    {
        IPv4Packet result = packet;

        // Only apply NAT for TCP and UDP
        if (packet.protocol != 6 && packet.protocol != 17)
        {
            return result;
        }

        // Extract ports from payload
        uint16_t srcPort, dstPort;
        if (!extractPortsFromPayload(packet.payload, packet.protocol, srcPort, dstPort))
        {
            return result;
        }

        if (isOutgoing)
        {
            if (natInsideInterfaces.find(ingressPort) != natInsideInterfaces.end())
            {
                std::string externalIp;
                uint16_t externalPort;
                if (natTable.lookupInternal(packet.srcIp, srcPort, packet.protocol, externalIp, externalPort))
                {
                    result.srcIp = externalIp;
                    result.payload[0] = static_cast<uint8_t>((externalPort >> 8) & 0xFF);
                    result.payload[1] = static_cast<uint8_t>(externalPort & 0xFF);
                }
            }
        }
        else
        {
            if (natOutsideInterfaces.find(ingressPort) != natOutsideInterfaces.end())
            {
                std::string internalIp;
                uint16_t internalPort;
                if (natTable.lookupExternal(packet.dstIp, dstPort, packet.protocol, internalIp, internalPort))
                {
                    result.dstIp = internalIp;
                    result.payload[2] = static_cast<uint8_t>((internalPort >> 8) & 0xFF);
                    result.payload[3] = static_cast<uint8_t>(internalPort & 0xFF);
                }
            }
        }

        return result;
    }

    void Router::sendArpRequest(uint32_t outPortNumber, int outVlanId, const std::string &targetIp)
    {
        std::shared_ptr<Interface> iface = getInterface(outPortNumber);
        const RouterLogicalInterface *logicalInterface = getLogicalInterface(outPortNumber, outVlanId);
        if (!iface || !logicalInterface)
        {
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

    bool Router::sendPacketOut(const IPv4Packet &packet,
                               const std::string &nextHopIp,
                               uint32_t outPortNumber,
                               int outVlanId)
    {
        std::shared_ptr<Interface> iface = getInterface(outPortNumber);
        if (!iface || !iface->isConnected())
        {
            return false;
        }

        const std::string key = makeArpKey(outPortNumber, outVlanId, nextHopIp);
        std::map<std::string, std::string>::const_iterator arpIt = arpCache.find(key);
        if (arpIt != arpCache.end())
        {
            const uint32_t linkMtu = getLinkMtu(iface);
            const std::vector<IPv4Packet> fragments = fragmentIpv4Packet(packet, linkMtu);
            if (fragments.size() > 1)
            {
                std::cout << "[Router] " << name << " memecah IPv4 packet menjadi " << fragments.size()
                          << " fragment (MTU " << linkMtu << ")" << std::endl;
            }

            for (size_t i = 0; i < fragments.size(); ++i)
            {
                EthernetFrame frame;
                frame.dstMac = arpIt->second;
                frame.srcMac = iface->getMacAddress();
                frame.etherType = kEtherTypeIpv4;
                frame.vlanId = outVlanId;
                frame.payload = fragments[i].toBytes();
                iface->send(frame.toBytes());
            }
            return true;
        }

        RouterPendingPacket pending;
        pending.packet = packet;
        pending.nextHopIp = nextHopIp;
        pending.outPortNumber = outPortNumber;
        pending.outVlanId = outVlanId;

        std::vector<RouterPendingPacket> &queue = pendingPackets[key];
        queue.push_back(pending);
        if (queue.size() == 1)
        {
            sendArpRequest(outPortNumber, outVlanId, nextHopIp);
        }
        return true;
    }

    void Router::flushQueuedPackets(uint32_t outPortNumber, int outVlanId, const std::string &nextHopIp)
    {
        const std::string key = makeArpKey(outPortNumber, outVlanId, nextHopIp);
        std::map<std::string, std::vector<RouterPendingPacket>>::iterator queueIt = pendingPackets.find(key);
        std::map<std::string, std::string>::const_iterator arpIt = arpCache.find(key);
        std::shared_ptr<Interface> iface = getInterface(outPortNumber);
        if (queueIt == pendingPackets.end() || arpIt == arpCache.end() || !iface)
        {
            return;
        }

        const uint32_t linkMtu = getLinkMtu(iface);
        for (size_t i = 0; i < queueIt->second.size(); ++i)
        {
            const std::vector<IPv4Packet> fragments = fragmentIpv4Packet(queueIt->second[i].packet, linkMtu);
            if (fragments.size() > 1)
            {
                std::cout << "[Router] " << name << " memecah IPv4 packet menjadi " << fragments.size()
                          << " fragment (MTU " << linkMtu << ")" << std::endl;
            }

            for (size_t j = 0; j < fragments.size(); ++j)
            {
                EthernetFrame frame;
                frame.dstMac = arpIt->second;
                frame.srcMac = iface->getMacAddress();
                frame.etherType = kEtherTypeIpv4;
                frame.vlanId = outVlanId;
                frame.payload = fragments[j].toBytes();
                iface->send(frame.toBytes());
            }
        }

        pendingPackets.erase(queueIt);
    }

    bool Router::sendIcmpTo(const std::string &dstIp, ICMPMessage icmp)
    {
        RouterResolvedRoute route;
        if (!findRoute(dstIp, route))
        {
            return false;
        }

        const RouterLogicalInterface *logicalInterface = getLogicalInterface(route.outPortNumber, route.outVlanId);
        if (!logicalInterface)
        {
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

    void Router::sendIcmpError(const IPv4Packet &originalPacket, uint8_t type, uint8_t code)
    {
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

    void Router::sendIcmpEchoReply(const IPv4Packet &requestPacket, const ICMPMessage &requestMessage)
    {
        ICMPMessage reply;
        reply.type = kICMPEchoReply;
        reply.code = 0;
        reply.identifier = requestMessage.identifier;
        reply.sequenceNumber = requestMessage.sequenceNumber;
        reply.payload = requestMessage.payload;

        sendIcmpTo(requestPacket.srcIp, reply);
    }

    void Router::handleReceive(Interface *incomingInterface, const std::vector<uint8_t> &rawBytes)
    {
        if (!incomingInterface)
        {
            return;
        }

        EthernetFrame frame;
        try
        {
            frame.fromBytes(rawBytes);
        }
        catch (...)
        {
            return;
        }

        if (frame.dstMac != kBroadcastMac && frame.dstMac != incomingInterface->getMacAddress())
        {
            return;
        }

        if (frame.etherType == kEtherTypeArp)
        {
            handleArpFrame(incomingInterface, frame);
            return;
        }

        if (frame.etherType == kEtherTypeIpv4)
        {
            handleIpv4Frame(incomingInterface, frame);
        }
    }

    void Router::handleArpFrame(Interface *incomingInterface, const EthernetFrame &frame)
    {
        ARPMessage arp;
        try
        {
            arp.fromBytes(frame.payload);
        }
        catch (...)
        {
            return;
        }

        const uint32_t portNumber = incomingInterface->getPortNumber();
        arpCache[makeArpKey(portNumber, frame.vlanId, arp.senderIp)] = arp.senderMac;

        const RouterLogicalInterface *logicalInterface = getLogicalInterface(portNumber, frame.vlanId);
        if (arp.opcode == 1 && logicalInterface != nullptr &&
            arp.targetIp == iputil::stripCidr(logicalInterface->cidr))
        {
            logEvent("packet", name, arp.senderIp, "ARP",
                     "ARP request from " + arp.senderIp + " on port " + std::to_string(portNumber));
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
            logEvent("packet", name, arp.senderIp, "ARP", "ARP reply sent to " + arp.senderIp);
            return;
        }

        if (arp.opcode == 2)
        {
            logEvent("packet", name, arp.senderIp, "ARP", "ARP reply from " + arp.senderIp);
            flushQueuedPackets(portNumber, frame.vlanId, arp.senderIp);
        }
    }

    void Router::handleIpv4Frame(Interface *incomingInterface, const EthernetFrame &frame)
    {
        IPv4Packet packet;
        try
        {
            packet.fromBytes(frame.payload);
        }
        catch (...)
        {
            return;
        }

        if (!packet.validateChecksum())
        {
            logEvent("error", name, packet.srcIp, "IPv4", "Invalid checksum on ingress port " + std::to_string(incomingInterface->getPortNumber()));
            return;
        }

        const uint32_t ingressPort = incomingInterface->getPortNumber();
        arpCache[makeArpKey(ingressPort, frame.vlanId, packet.srcIp)] = frame.srcMac;
        logEvent("packet", name, packet.srcIp, "IPv4","Received " + packet.srcIp + "->" + packet.dstIp + " proto=" + std::to_string(packet.protocol) +" on port " + std::to_string(ingressPort));

        // Extract port information for ACL and NAT
        uint16_t srcPort = 0, dstPort = 0;
        extractPortsFromPayload(packet.payload, packet.protocol, srcPort, dstPort);

        // Apply ingress ACL
        if (!applyIngressACL(packet, packet.protocol, srcPort, dstPort))
        {
            logEvent("packet", name, packet.srcIp, "ACL", "Ingress ACL denied " + packet.srcIp + "->" + packet.dstIp);
            return; // Packet denied by ingress ACL
        }

        // Apply NAT translation for incoming packets
        packet = applyNATTranslation(packet, false, ingressPort);

        const RouterLogicalInterface *localInterface = findInterfaceByIp(packet.dstIp);
        bool isRipDest = (packet.dstIp == kRipBroadcastAddr || packet.dstIp == kRipMulticastAddr);
        bool isLocalOrRip = (localInterface != nullptr || isRipDest);

        if (isLocalOrRip && packet.protocol == 17)
        {
            UDPSegment udp;
            try
            {
                udp.fromBytes(packet.payload);
            }
            catch (...)
            {
                if (!localInterface)
                    return;
            }

            if (udp.destinationPort == kRipPort && ripEnabled)
            {
                logEvent("route", name, packet.srcIp, "RIP", "RIP update received from " + packet.srcIp);
                processRipUpdate(packet.srcIp, udp.payload, ingressPort, frame.vlanId);
                if (!localInterface)
                    return;
            }

            if (!localInterface)
                return;
        }

        if (localInterface != nullptr)
        {
            if (packet.protocol != 1)
            {
                return;
            }

            ICMPMessage icmp;
            try
            {
                icmp.fromBytes(packet.payload);
            }
            catch (...)
            {
                return;
            }

            if (!icmp.validateChecksum())
            {
                return;
            }

            if (icmp.type == kICMPEchoRequest)
            {
                sendIcmpEchoReply(packet, icmp);
            }
            return;
        }

        if (packet.ttl <= 1)
        {
            logEvent("packet", name, packet.srcIp, "ICMP", "TTL exceeded for " + packet.dstIp);
            sendIcmpError(packet, kICMPTimeExceeded, 0);
            return;
        }

        RouterResolvedRoute route;
        if (!findRoute(packet.dstIp, route))
        {
            logEvent("packet", name, packet.srcIp, "ICMP", "Destination unreachable " + packet.dstIp);
            sendIcmpError(packet, kICMPDestinationUnreachable, 0);
            return;
        }

        IPv4Packet forwarded = packet;
        forwarded.ttl = static_cast<uint8_t>(forwarded.ttl - 1);

        // Re-extract ports after potential NAT translation
        extractPortsFromPayload(forwarded.payload, forwarded.protocol, srcPort, dstPort);

        // Apply NAT translation for outgoing packets
        forwarded = applyNATTranslation(forwarded, true, route.outPortNumber);

        // Re-extract ports after NAT translation to get updated values
        extractPortsFromPayload(forwarded.payload, forwarded.protocol, srcPort, dstPort);

        // Apply egress ACL check
        if (!applyEgressACL(forwarded, forwarded.protocol, srcPort, dstPort))
        {
            logEvent("packet", name, packet.srcIp, "ACL", "Egress ACL denied " + forwarded.srcIp + "->" + forwarded.dstIp);
            return; // Packet denied by egress ACL
        }

        forwarded.updateChecksum();

        const std::string nextHopIp = route.nextHopIp.empty() ? forwarded.dstIp : route.nextHopIp;
        logEvent("packet", name, forwarded.dstIp, "IPv4",
                 "Forward " + forwarded.srcIp + "->" + forwarded.dstIp +
                     " via port " + std::to_string(route.outPortNumber) + " nextHop=" + nextHopIp);
        sendPacketOut(forwarded, nextHopIp, route.outPortNumber, route.outVlanId);
    }

    std::string Router::toJson() const
    {
        std::stringstream ss;
        ss << "    {\n";
        ss << "      \"name\": \"" << name << "\",\n";
        ss << "      \"num_ports\": " << numPorts << ",\n";
        ss << "      \"interfaces\": [\n";
        bool first = true;
        for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
             it != logicalInterfaces.end();
             ++it)
        {
            if (!first)
            {
                ss << ",\n";
            }
            first = false;
            ss << "        {\"port\": " << it->second.portNumber;
            if (it->second.vlanId != iputil::kUntaggedVlan)
            {
                ss << ", \"vlan_id\": " << it->second.vlanId;
            }
            ss << ", \"ip_address\": \"" << it->second.cidr << "\"}";
        }
        ss << "\n      ],\n";
        ss << "      \"routing_table\": [\n";
        first = true;
        for (size_t i = 0; i < staticRoutes.size(); ++i)
        {
            if (!first)
            {
                ss << ",\n";
            }
            first = false;
            ss << "        {\"destination\": \"" << staticRoutes[i].destinationCidr
               << "\", \"next_hop\": \"" << staticRoutes[i].nextHopIp
               << "\", \"interface\": " << staticRoutes[i].outPortNumber;
            if (staticRoutes[i].outVlanId != iputil::kUntaggedVlan)
            {
                ss << ", \"vlan_id\": " << staticRoutes[i].outVlanId;
            }
            ss << "}";
        }
        ss << "\n      ]\n";
        ss << "    }";
        return ss.str();
    }

    void Router::printInfo() const
    {
        std::cout << "[Router: " << name << "]" << std::endl;
        std::cout << "  Physical Ports:" << std::endl;
        for (std::map<uint32_t, std::shared_ptr<Interface>>::const_iterator it = interfaces.begin();
             it != interfaces.end();
             ++it)
        {
            std::cout << "    Port " << it->first << ": " << it->second->getMacAddress();
            if (it->second->isConnected())
            {
                std::cout << " (Connected)";
            }
            else
            {
                std::cout << " (Disconnected)";
            }
            std::cout << std::endl;
        }

        std::cout << "  Logical Interfaces:" << std::endl;
        if (logicalInterfaces.empty())
        {
            std::cout << "    (none)" << std::endl;
        }
        else
        {
            for (std::map<std::string, RouterLogicalInterface>::const_iterator it = logicalInterfaces.begin();
                 it != logicalInterfaces.end();
                 ++it)
            {
                std::cout << "    Port " << it->second.portNumber;
                if (it->second.vlanId != iputil::kUntaggedVlan)
                {
                    std::cout << "." << it->second.vlanId;
                }
                std::cout << " -> " << it->second.cidr << std::endl;
            }
        }

        std::cout << std::endl;
        printRoutingTable();
    }

}
