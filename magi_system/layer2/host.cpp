#include "host.hpp"
#include "core/interface.hpp"
#include "core/link.hpp"
#include "core/event_log.hpp"
#include "layer2/ethernet.hpp"
#include "layer7/http_server.hpp"
#include "layer7/dhcp_server.hpp"
#include "layer7/dns_server.hpp"
#include "layer4/udp.hpp"
#include "layer7/udp_socket.hpp"

#include "layer3/ip_utils.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace
{

    const short kEtherTypeIpv4 = static_cast<short>(0x0800);
    const short kEtherTypeArp = static_cast<short>(0x0806);
    const std::string kBroadcastMac = "ff:ff:ff:ff:ff:ff";
    const char kPingPayloadPrefix[] = "MAGI_ECHO_";
    const uint32_t kDefaultLinkMtu = 1500;
    uint16_t gNextEchoIdentifier = 1;

    struct ReassemblyBuffer
    {
        std::map<uint32_t, std::vector<uint8_t>> fragmentsByOffset;
        size_t expectedPayloadSize = 0;
        bool finalFragmentSeen = false;
        std::chrono::steady_clock::time_point lastUpdated = std::chrono::steady_clock::now();
    };

    std::map<std::string, ReassemblyBuffer> gReassemblyBuffers;
    const std::chrono::seconds kReassemblyBufferTtl(60);
    std::vector<magi::Host *> gHostRegistry;

    void pruneExpiredReassemblyBuffers(const std::chrono::steady_clock::time_point &now)
    {
        for (std::map<std::string, ReassemblyBuffer>::iterator it = gReassemblyBuffers.begin(); it != gReassemblyBuffers.end();)
        {
            if (now - it->second.lastUpdated > kReassemblyBufferTtl)
            {
                it = gReassemblyBuffers.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }

    std::string makeReassemblyKey(const magi::Host *host,
                                  const std::string &srcIp,
                                  const std::string &dstIp,
                                  uint8_t protocol,
                                  uint16_t identification)
    {
        std::ostringstream oss;
        oss << reinterpret_cast<uintptr_t>(host) << ':'
            << srcIp << ':' << dstIp << ':'
            << static_cast<int>(protocol) << ':' << identification;
        return oss.str();
    }

    uint32_t getLinkMtu(const std::shared_ptr<magi::Interface> &iface)
    {
        if (iface && iface->getLink())
        {
            return iface->getLink()->getMtu();
        }
        return kDefaultLinkMtu;
    }

    std::vector<magi::IPv4Packet> fragmentIpv4Packet(const magi::IPv4Packet &packet, uint32_t mtu)
    {
        const size_t headerSize = static_cast<size_t>(packet.ihl) * 4;
        const size_t maxPayloadPerFragment = (mtu > headerSize)
                                                 ? ((static_cast<size_t>(mtu) - headerSize) / 8) * 8
                                                 : 0;

        if (maxPayloadPerFragment == 0 || packet.payload.size() <= maxPayloadPerFragment)
        {
            return std::vector<magi::IPv4Packet>{packet};
        }

        std::vector<magi::IPv4Packet> fragments;
        size_t offset = 0;
        while (offset < packet.payload.size())
        {
            const size_t remaining = packet.payload.size() - offset;
            size_t fragmentPayloadSize = remaining;
            if (remaining > maxPayloadPerFragment)
            {
                fragmentPayloadSize = maxPayloadPerFragment;
            }

            magi::IPv4Packet fragment = packet;
            fragment.flags = static_cast<uint8_t>(packet.flags & static_cast<uint8_t>(~0x1));
            if (remaining > fragmentPayloadSize)
            {
                fragment.flags = static_cast<uint8_t>(fragment.flags | 0x1);
            }
            fragment.fragmentOffset = static_cast<uint16_t>(offset / 8);
            fragment.payload.assign(packet.payload.begin() + static_cast<long>(offset),
                                    packet.payload.begin() + static_cast<long>(offset + fragmentPayloadSize));
            fragment.updateChecksum();
            fragments.push_back(fragment);

            offset += fragmentPayloadSize;
        }

        return fragments;
    }

    bool tryReassembleIpv4Packet(const magi::Host *host,
                                 const magi::IPv4Packet &fragment,
                                 magi::IPv4Packet &outPacket)
    {
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        pruneExpiredReassemblyBuffers(now);

        if (fragment.fragmentOffset == 0 && (fragment.flags & 0x1) == 0)
        {
            outPacket = fragment;
            return true;
        }

        const std::string key = makeReassemblyKey(host,
                                                  fragment.srcIp,
                                                  fragment.dstIp,
                                                  fragment.protocol,
                                                  fragment.identification);
        ReassemblyBuffer &buffer = gReassemblyBuffers[key];
        buffer.lastUpdated = now;
        const uint32_t byteOffset = static_cast<uint32_t>(fragment.fragmentOffset) * 8u;
        buffer.fragmentsByOffset[byteOffset] = fragment.payload;

        if ((fragment.flags & 0x1) == 0)
        {
            buffer.finalFragmentSeen = true;
            buffer.expectedPayloadSize = static_cast<size_t>(byteOffset + fragment.payload.size());
        }

        if (!buffer.finalFragmentSeen)
        {
            return false;
        }

        size_t expectedOffset = 0;
        std::vector<uint8_t> payload;
        payload.reserve(buffer.expectedPayloadSize);

        for (std::map<uint32_t, std::vector<uint8_t>>::const_iterator it = buffer.fragmentsByOffset.begin();
             it != buffer.fragmentsByOffset.end();
             ++it)
        {
            if (it->first != expectedOffset)
            {
                return false;
            }
            payload.insert(payload.end(), it->second.begin(), it->second.end());
            expectedOffset += it->second.size();
        }

        if (expectedOffset != buffer.expectedPayloadSize)
        {
            return false;
        }

        outPacket = fragment;
        outPacket.flags = 0;
        outPacket.fragmentOffset = 0;
        outPacket.payload = payload;
        outPacket.updateChecksum();
        gReassemblyBuffers.erase(key);
        return true;
    }

    std::string formatRtt(double milliseconds)
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << milliseconds;
        return oss.str();
    }

    std::string buildPingPayload(const std::string &hostName, uint16_t sequenceNumber)
    {
        std::ostringstream oss;
        oss << kPingPayloadPrefix << hostName << "_" << sequenceNumber;
        const std::string payload = oss.str();
        return payload;
    }

} // namespace

namespace magi
{

    Host::Host(const std::string &name, const std::string &ipAddress, const std::string &defaultGateway)
        : Node(name),
          ipAddress(ipAddress),
          defaultGateway(defaultGateway),
          echoIdentifier(gNextEchoIdentifier++),
          nextSequenceNumber(1),
          nextIpIdentification(1)
    {
        // Host biasanya memiliki 1 interface
        addInterface();
        gHostRegistry.push_back(this);
    }

    Host::~Host()
    {
        stopHttpServer();
        stopDhcpServer();
        stopDnsServer();

        std::vector<Host *>::iterator it = std::find(gHostRegistry.begin(), gHostRegistry.end(), this);
        if (it != gHostRegistry.end())
        {
            gHostRegistry.erase(it);
        }
    }

    void Host::handleReceive(Interface *incomingInterface, const std::vector<uint8_t> &rawBytes)
    {
        EthernetFrame frame;
        try
        {
            frame.fromBytes(rawBytes);
        }
        catch (...)
        {
            return;
        }

        if (!incomingInterface)
        {
            return;
        }

        const std::string myMac = incomingInterface->getMacAddress();
        if (frame.dstMac != kBroadcastMac && frame.dstMac != myMac)
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
            handleIpv4Frame(frame);
        }
    }

    std::string Host::toJson() const
    {
        std::stringstream ss;
        ss << "    {\n";
        ss << "      \"name\": \"" << name << "\",\n";
        ss << "      \"ip_address\": \"" << ipAddress << "\",\n";
        ss << "      \"default_gateway\": \"" << defaultGateway << "\"\n";
        ss << "    }";
        return ss.str();
    }

    void Host::printInfo() const
    {
        std::cout << "[Host: " << name << "]" << std::endl;
        std::cout << "  IP Address: " << (ipAddress.empty() ? "(not set)" : ipAddress) << std::endl;
        std::cout << "  Default Gateway: " << (defaultGateway.empty() ? "(not set)" : defaultGateway) << std::endl;
        std::cout << "  Interfaces:" << std::endl;
        for (const auto &pair : interfaces)
        {
            std::cout << "    Port " << pair.first << ": " << pair.second->getMacAddress();
            if (pair.second->isConnected())
            {
                std::cout << " (Connected)";
            }
            std::cout << std::endl;
        }
    }

    void Host::printArpCache() const
    {
        std::cout << "[ARP Cache: " << name << "]" << std::endl;
        if (arpCache.table.empty())
        {
            std::cout << "  (empty)" << std::endl;
            return;
        }

        for (const auto &entry : arpCache.table)
        {
            std::cout << "  " << entry.first << " -> " << entry.second << std::endl;
        }
    }

    std::string Host::getPrimaryIp() const
    {
        return iputil::stripCidr(ipAddress);
    }

    uint32_t Host::makeEchoKey(uint16_t sequenceNumber) const
    {
        return (static_cast<uint32_t>(echoIdentifier) << 16) | static_cast<uint32_t>(sequenceNumber);
    }

    std::string Host::makeSocketKey(const std::string &localIp, uint16_t localPort,
                                    const std::string &remoteIp, uint16_t remotePort) const
    {
        std::ostringstream oss;
        oss << localIp << ":" << localPort << ":" << remoteIp << ":" << remotePort;
        return oss.str();
    }

    std::string Host::resolveNextHop(const std::string &targetIp) const
    {
        if (ipAddress.empty())
        {
            return "";
        }
        if (iputil::sameSubnet(ipAddress, targetIp))
        {
            return targetIp;
        }
        return defaultGateway;
    }

    void Host::sendArpRequest(Interface *iface, const std::string &targetIp)
    {
        if (!iface)
        {
            return;
        }

        ARPMessage request;
        request.opcode = 1;
        request.senderMac = iface->getMacAddress();
        request.senderIp = getPrimaryIp();
        request.targetMac = "00:00:00:00:00:00";
        request.targetIp = targetIp;

        EthernetFrame frame;
        frame.dstMac = kBroadcastMac;
        frame.srcMac = iface->getMacAddress();
        frame.etherType = kEtherTypeArp;
        frame.vlanId = iputil::kUntaggedVlan;
        frame.payload = request.toBytes();
        iface->send(frame.toBytes());
    }

    void Host::flushQueuedPackets(Interface *iface, const std::string &nextHopIp, int vlanId)
    {
        if (!iface)
        {
            return;
        }

        const std::map<std::string, std::string>::const_iterator macIt = arpCache.table.find(nextHopIp);
        if (macIt == arpCache.table.end())
        {
            return;
        }

        std::map<std::string, std::vector<std::vector<uint8_t>>>::iterator queueIt = arpCache.queue.find(nextHopIp);
        if (queueIt == arpCache.queue.end())
        {
            return;
        }

        for (size_t i = 0; i < queueIt->second.size(); ++i)
        {
            EthernetFrame out;
            out.dstMac = macIt->second;
            out.srcMac = iface->getMacAddress();
            out.etherType = kEtherTypeIpv4;
            out.vlanId = vlanId;
            out.payload = queueIt->second[i];
            iface->send(out.toBytes());
        }

        arpCache.queue.erase(queueIt);
    }

    bool Host::sendIpv4Packet(const IPv4Packet &packet)
    {
        auto iface = getInterface(1);
        if (!iface)
        {
            std::cout << "[" << name << "] Tidak ada interface untuk mengirim paket." << std::endl;
            return false;
        }

        const std::string nextHopIp = resolveNextHop(packet.dstIp);
        const uint32_t linkMtu = getLinkMtu(iface);
        const std::vector<IPv4Packet> packetsToSend = fragmentIpv4Packet(packet, linkMtu);

        if (packetsToSend.size() > 1)
        {
            std::cout << "[Host] " << name << " memecah IPv4 packet menjadi " << packetsToSend.size()
                      << " fragment (MTU " << linkMtu << ")" << std::endl;
        }

        // Special-case: broadcast IP -> send as Ethernet broadcast without ARP
        if (packet.dstIp == "255.255.255.255")
        {
            if (getPrimaryIp().empty() && packet.srcIp != "0.0.0.0")
            {
                std::cout << "[" << name << "] IP host belum dikonfigurasi." << std::endl;
                return false;
            }

            for (const auto &fragment : packetsToSend)
            {
                EthernetFrame frame;
                frame.dstMac = "ff:ff:ff:ff:ff:ff";
                frame.srcMac = iface->getMacAddress();
                frame.etherType = kEtherTypeIpv4;
                frame.vlanId = iputil::kUntaggedVlan;
                frame.payload = fragment.toBytes();
                std::cout << "[Host] " << name << " sending IPv4 broadcast " << fragment.srcIp << "->" << fragment.dstIp << " via MAC " << frame.srcMac << std::endl;
                logEvent("packet", name, fragment.dstIp, "IPv4", "Broadcast " + fragment.srcIp + "->" + fragment.dstIp + " proto=" + std::to_string(fragment.protocol));
                iface->send(frame.toBytes());
            }
            return true;
        }

        if (getPrimaryIp().empty())
        {
            std::cout << "[" << name << "] IP host belum dikonfigurasi." << std::endl;
            return false;
        }
        if (nextHopIp.empty())
        {
            std::cout << "[" << name << "] Tidak ada route ke " << packet.dstIp
                      << " (default gateway belum di-set)." << std::endl;
            return false;
        }

        const std::map<std::string, std::string>::const_iterator cacheIt = arpCache.table.find(nextHopIp);
        if (cacheIt != arpCache.table.end())
        {
            for (const auto &fragment : packetsToSend)
            {
                EthernetFrame frame;
                frame.dstMac = cacheIt->second;
                frame.srcMac = iface->getMacAddress();
                frame.etherType = kEtherTypeIpv4;
                frame.vlanId = iputil::kUntaggedVlan;
                frame.payload = fragment.toBytes();
                std::cout << "[Host] " << name << " sending IPv4 unicast " << fragment.srcIp << "->" << fragment.dstIp << " dstMac=" << frame.dstMac << std::endl;
                logEvent("packet", name, fragment.dstIp, "IPv4",
                         "Send " + fragment.srcIp + "->" + fragment.dstIp + " proto=" + std::to_string(fragment.protocol));
                iface->send(frame.toBytes());
            }
            return true;
        }

        std::vector<std::vector<uint8_t>> &pending = arpCache.queue[nextHopIp];
        for (const auto &fragment : packetsToSend)
        {
            pending.push_back(fragment.toBytes());
        }

        sendArpRequest(iface.get(), nextHopIp);
        return true;
    }

    void Host::registerListeningSocket(uint16_t port, std::shared_ptr<TCPSocket> socket)
    {
        listeningSockets[port] = socket;
    }

    void Host::registerUdpSocket(uint16_t port, std::shared_ptr<UDPSocket> socket)
    {
        if (socket)
        {
            udpSockets[port] = socket;
        }
    }

    void Host::unregisterUdpSocket(uint16_t port)
    {
        udpSockets.erase(port);
    }

    void Host::registerActiveSocket(const std::string &localIp, uint16_t localPort,
                                    const std::string &remoteIp, uint16_t remotePort,
                                    std::shared_ptr<TCPSocket> socket)
    {
        activeSockets[makeSocketKey(localIp, localPort, remoteIp, remotePort)] = socket;
    }

    std::shared_ptr<TCPSocket> Host::findActiveSocket(const std::string &localIp, uint16_t localPort,
                                                      const std::string &remoteIp, uint16_t remotePort) const
    {
        std::map<std::string, std::shared_ptr<TCPSocket>>::const_iterator it =
            activeSockets.find(makeSocketKey(localIp, localPort, remoteIp, remotePort));
        if (it != activeSockets.end())
        {
            return it->second;
        }
        return nullptr;
    }

    std::shared_ptr<TCPSocket> Host::acceptConnection(uint16_t port)
    {
        std::map<uint16_t, std::queue<AcceptedSocketEntry>>::iterator it = acceptedSockets.find(port);
        if (it == acceptedSockets.end() || it->second.empty())
        {
            return nullptr;
        }

        AcceptedSocketEntry entry = it->second.front();
        it->second.pop();
        if (it->second.empty())
        {
            acceptedSockets.erase(it);
        }
        return entry.socket;
    }

    void Host::unregisterListeningSocket(uint16_t port)
    {
        listeningSockets.erase(port);
        std::map<uint16_t, std::queue<AcceptedSocketEntry>>::iterator it = acceptedSockets.find(port);
        if (it != acceptedSockets.end())
        {
            while (!it->second.empty())
            {
                queuedAcceptedSocketKeys.erase(it->second.front().key);
                it->second.pop();
            }
            acceptedSockets.erase(it);
        }
    }

    void Host::unregisterActiveSocket(const std::string &localIp, uint16_t localPort,
                                      const std::string &remoteIp, uint16_t remotePort)
    {
        const std::string key = makeSocketKey(localIp, localPort, remoteIp, remotePort);
        activeSockets.erase(key);
        queuedAcceptedSocketKeys.erase(key);
    }

    bool Host::initiateCloseToRemote(const std::string &localIp,
                                     const std::string &remoteIp,
                                     uint16_t remotePort)
    {
        std::ostringstream oss;
        // We don't know the local port here; search activeSockets for matching localIp/remoteIp/remotePort
        for (auto &kv : activeSockets)
        {
            const std::string &key = kv.first;
            // key format: localIp:localPort:remoteIp:remotePort
            std::istringstream iss(key);
            std::string partLocalIp, partLocalPort, partRemoteIp, partRemotePort;
            if (!std::getline(iss, partLocalIp, ':'))
                continue;
            if (!std::getline(iss, partLocalPort, ':'))
                continue;
            if (!std::getline(iss, partRemoteIp, ':'))
                continue;
            if (!std::getline(iss, partRemotePort, ':'))
                continue;

            if (partLocalIp == localIp && partRemoteIp == remoteIp && static_cast<uint16_t>(std::stoi(partRemotePort)) == remotePort)
            {
                auto sock = kv.second;
                if (!sock)
                    return false;
                auto finSeg = sock->initiateClose();
                if (!finSeg)
                    return false;

                IPv4Packet finPacket;
                finPacket.srcIp = partLocalIp;
                finPacket.dstIp = partRemoteIp;
                finPacket.protocol = 6;
                finPacket.ttl = 64;
                finPacket.identification = nextIpIdentification++;
                finPacket.payload = finSeg->toBytes();
                finPacket.updateChecksum();

                return sendIpv4Packet(finPacket);
            }
        }

        return false;
    }

    bool Host::sendIpv4(const IPv4Packet &packet)
    {
        return sendIpv4Packet(packet);
    }

    void Host::sendEchoProbe(const std::string &targetIp, uint8_t ttl, bool tracerouteProbe)
    {
        const uint16_t sequenceNumber = nextSequenceNumber++;
        const uint32_t echoKey = makeEchoKey(sequenceNumber);

        EchoProbeState probeState;
        probeState.sentAt = std::chrono::steady_clock::now();
        probeState.tracerouteProbe = tracerouteProbe;
        probeState.completed = false;
        probeState.targetIp = targetIp;
        probeState.responderIp = "";
        probeState.responseType = 0xFF;
        probeState.responseCode = 0;
        probeState.replyTtl = 0;
        probeState.rttMs = 0.0;
        pendingEchoes[echoKey] = probeState;

        ICMPMessage icmp;
        icmp.type = kICMPEchoRequest;
        icmp.code = 0;
        icmp.identifier = echoIdentifier;
        icmp.sequenceNumber = sequenceNumber;
        const std::string payload = buildPingPayload(name, sequenceNumber);
        icmp.payload.assign(payload.begin(), payload.end());
        icmp.updateChecksum();

        IPv4Packet packet;
        packet.srcIp = getPrimaryIp();
        packet.dstIp = targetIp;
        packet.protocol = 1;
        packet.ttl = ttl;
        packet.identification = nextIpIdentification++;
        packet.payload = icmp.toBytes();
        packet.updateChecksum();

        if (!sendIpv4Packet(packet))
        {
            pendingEchoes.erase(echoKey);
        }
    }

    void Host::completeEchoProbe(uint16_t identifier,
                                 uint16_t sequenceNumber,
                                 const std::string &responderIp,
                                 uint8_t responseType,
                                 uint8_t responseCode,
                                 uint8_t replyTtl)
    {
        if (identifier != echoIdentifier)
        {
            return;
        }

        std::map<uint32_t, EchoProbeState>::iterator it = pendingEchoes.find(makeEchoKey(sequenceNumber));
        if (it == pendingEchoes.end() || it->second.completed)
        {
            return;
        }

        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        it->second.completed = true;
        it->second.responderIp = responderIp;
        it->second.responseType = responseType;
        it->second.responseCode = responseCode;
        it->second.replyTtl = replyTtl;
        it->second.rttMs = std::chrono::duration<double, std::milli>(now - it->second.sentAt).count();
    }

    bool Host::extractEmbeddedEchoKey(const ICMPMessage &icmp, uint16_t &identifier,uint16_t &sequenceNumber) const
    {
        try
        {
            IPv4Packet originalPacket;
            originalPacket.fromBytes(icmp.payload);
            if (originalPacket.protocol != 1)
            {
                return false;
            }

            ICMPMessage originalIcmp;
            originalIcmp.fromBytes(originalPacket.payload);
            identifier = originalIcmp.identifier;
            sequenceNumber = originalIcmp.sequenceNumber;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    void Host::handleArpFrame(Interface *incomingInterface, const EthernetFrame &frame)
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

        arpCache.table[arp.senderIp] = arp.senderMac;

        if (arp.opcode == 1 && arp.targetIp == getPrimaryIp())
        {
            logEvent("packet", name, arp.senderIp, "ARP", "ARP request from " + arp.senderIp + " for " + arp.targetIp);
            EthernetFrame out;
            ARPMessage reply;
            reply.opcode = 2;
            reply.senderMac = incomingInterface->getMacAddress();
            reply.senderIp = getPrimaryIp();
            reply.targetMac = arp.senderMac;
            reply.targetIp = arp.senderIp;

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
            flushQueuedPackets(incomingInterface, arp.senderIp, frame.vlanId);
        }
    }

    void Host::handleIpv4Frame(const EthernetFrame &frame)
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

        std::cout << "[Host] " << name << " received IPv4 frame: " << packet.srcIp << " -> " << packet.dstIp << ", ether src=" << frame.srcMac << " dst=" << frame.dstMac << std::endl;
        logEvent("packet", name, packet.srcIp, "IPv4",
                 "Received IPv4 from " + packet.srcIp + " to " + packet.dstIp + " proto=" + std::to_string(packet.protocol));

        if (!packet.validateChecksum())
        {
            std::cout << "[Host] " << name << " dropping IPv4 packet due to invalid checksum: " << packet.srcIp << "->" << packet.dstIp << std::endl;
            logEvent("error", name, packet.srcIp, "IPv4", "Invalid checksum dropped");
            return;
        }

        arpCache.table[packet.srcIp] = frame.srcMac;

        // Allow packets targeted to this host or broadcast/zero-address (for DHCP)
        if (packet.dstIp != getPrimaryIp() && packet.dstIp != "255.255.255.255" && packet.dstIp != "0.0.0.0")
        {
            return;
        }

        IPv4Packet reassembledPacket;
        if (!tryReassembleIpv4Packet(this, packet, reassembledPacket))
        {
            std::cout << "[Host] " << name << " menyimpan fragment IPv4 id=" << packet.identification
                      << " offset=" << packet.fragmentOffset << " MF=" << static_cast<int>(packet.flags & 0x1) << std::endl;
            return;
        }

        packet = reassembledPacket;
        if (packet.fragmentOffset == 0 && packet.flags == 0 && packet.payload.size() != 0)
        {
            if (frame.payload.size() != packet.payload.size() + static_cast<size_t>(packet.ihl) * 4)
            {
                std::cout << "[Host] " << name << " berhasil reassembly IPv4 id=" << packet.identification
                          << " size=" << packet.payload.size() << std::endl;
            }
        }

        // ICMP handling
        if (packet.protocol == 1)
        {
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
                ICMPMessage reply;
                reply.type = kICMPEchoReply;
                reply.code = 0;
                reply.identifier = icmp.identifier;
                reply.sequenceNumber = icmp.sequenceNumber;
                reply.payload = icmp.payload;
                reply.updateChecksum();

                IPv4Packet response;
                response.srcIp = getPrimaryIp();
                response.dstIp = packet.srcIp;
                response.protocol = 1;
                response.ttl = 64;
                response.identification = nextIpIdentification++;
                response.payload = reply.toBytes();
                response.updateChecksum();
                logEvent("packet", name, packet.srcIp, "ICMP", "Echo reply to " + packet.srcIp);
                sendIpv4Packet(response);
                return;
            }

            if (icmp.type == kICMPEchoReply)
            {
                logEvent("packet", name, packet.srcIp, "ICMP", "Echo reply from " + packet.srcIp);
                completeEchoProbe(icmp.identifier,
                                  icmp.sequenceNumber,
                                  packet.srcIp,
                                  icmp.type,
                                  icmp.code,
                                  packet.ttl);
                return;
            }

            if (icmp.type == kICMPTimeExceeded || icmp.type == kICMPDestinationUnreachable)
            {
                uint16_t identifier = 0;
                uint16_t sequenceNumber = 0;
                if (extractEmbeddedEchoKey(icmp, identifier, sequenceNumber))
                {
                    completeEchoProbe(identifier,
                                      sequenceNumber,
                                      packet.srcIp,
                                      icmp.type,
                                      icmp.code,
                                      packet.ttl);
                }
            }

            return;
        }

        // UDP handling
        if (packet.protocol == 17)
        {
            UDPSegment udp;
            try
            {
                udp.fromBytes(packet.payload);
            }
            catch (...)
            {
                return;
            }

            if (!udp.validateChecksum(packet.srcIp, packet.dstIp))
            {
                return;
            }

            logEvent("packet", name, packet.srcIp, "UDP",
                     "UDP " + std::to_string(udp.sourcePort) + "->" + std::to_string(udp.destinationPort) +
                         " len=" + std::to_string(udp.payload.size()));

            // Deliver to registered UDP socket if any
            auto uit = udpSockets.find(udp.destinationPort);
            if (uit != udpSockets.end())
            {
                auto sock = uit->second;
                if (sock)
                {
                    sock->onReceive(packet.srcIp, udp.sourcePort, udp.payload);
                }
            }

            return;
        }

        // TCP handling
        if (packet.protocol == 6)
        {
            TCPSegment tcp;
            try
            {
                tcp.fromBytes(packet.payload);
            }
            catch (...)
            {
                return;
            }

            if (!tcp.validateChecksum(packet.srcIp, packet.dstIp))
            {
                logEvent("error", name, packet.srcIp, "TCP", "Invalid TCP checksum dropped");
                return;
            }

            logEvent("packet", name, packet.srcIp, "TCP","TCP " + std::to_string(tcp.sourcePort) + "->" + std::to_string(tcp.destinationPort) +" flags=0x" + std::to_string(tcp.flags) + " seq=" + std::to_string(tcp.seqNum));

            const std::string key = makeSocketKey(packet.dstIp, tcp.destinationPort, packet.srcIp, tcp.sourcePort);
            std::cout << "[Host] " << name << " TCP dispatch key=" << key << std::endl;
            auto ait = activeSockets.find(key);
            if (ait != activeSockets.end())
            {
                std::cout << "[Host] " << name << " matched active socket for key=" << key << std::endl;
                auto sock = ait->second;
                std::shared_ptr<TCPSegment> response = sock->handleIncomingSegment(tcp);
                if (response)
                {
                    IPv4Packet resp;
                    resp.srcIp = packet.dstIp;
                    resp.dstIp = packet.srcIp;
                    resp.protocol = 6;
                    resp.ttl = 64;
                    resp.identification = nextIpIdentification++;
                    resp.payload = response->toBytes();
                    try
                    {
                        TCPSegment seg;
                        seg.fromBytes(resp.payload);
                        seg.sourcePort = tcp.destinationPort;
                        seg.destinationPort = tcp.sourcePort;
                        seg.updateChecksum(resp.srcIp, resp.dstIp);
                        resp.payload = seg.toBytes();
                    }
                    catch (...)
                    {
                    }
                    resp.updateChecksum();
                    sendIpv4Packet(resp);
                }

                if (listeningSockets.find(tcp.destinationPort) != listeningSockets.end() &&
                    sock->isConnected() &&
                    queuedAcceptedSocketKeys.find(key) == queuedAcceptedSocketKeys.end())
                {
                    AcceptedSocketEntry entry;
                    entry.key = key;
                    entry.socket = sock;
                    acceptedSockets[tcp.destinationPort].push(entry);
                    queuedAcceptedSocketKeys.insert(key);
                }

                // HTTP Server Tick Hook: only when the segment actually carries request data
                if (tcp.destinationPort == 80 && httpServer && httpServer->isRunning() && tcp.getPayloadSize() > 0)
                {
                    httpServer->tick(packet.srcIp, tcp.sourcePort);
                }

                if (sock->isClosed())
                {
                    unregisterActiveSocket(packet.dstIp, tcp.destinationPort, packet.srcIp, tcp.sourcePort);
                }

                return;
            }

            auto lit = listeningSockets.find(tcp.destinationPort);
            if (lit != listeningSockets.end())
            {
                if (!tcp.hasSYN() || tcp.hasACK())
                {
                    return;
                }

                std::shared_ptr<TCPSocket> serverSock =
                    std::make_shared<TCPSocket>(packet.dstIp, tcp.destinationPort, packet.srcIp, tcp.sourcePort);
                serverSock->setState(TCPState::LISTEN);
                registerActiveSocket(packet.dstIp, tcp.destinationPort, packet.srcIp, tcp.sourcePort, serverSock);

                std::shared_ptr<TCPSegment> response = serverSock->handleIncomingSegment(tcp);
                if (response)
                {
                    IPv4Packet resp;
                    resp.srcIp = packet.dstIp;
                    resp.dstIp = packet.srcIp;
                    resp.protocol = 6;
                    resp.ttl = 64;
                    resp.identification = nextIpIdentification++;
                    resp.payload = response->toBytes();
                    try
                    {
                        TCPSegment seg;
                        seg.fromBytes(resp.payload);
                        seg.sourcePort = tcp.destinationPort;
                        seg.destinationPort = tcp.sourcePort;
                        seg.updateChecksum(resp.srcIp, resp.dstIp);
                        resp.payload = seg.toBytes();
                    }
                    catch (...)
                    {
                    }
                    resp.updateChecksum();
                    sendIpv4Packet(resp);
                }
                return;
            }

            return;
        }
    }

    void Host::sendPing(const std::string &targetIp)
    {
        if (!iputil::isValidIp(targetIp))
        {
            std::cout << "[" << name << "] Target IP tidak valid: " << targetIp << std::endl;
            return;
        }

        const uint16_t sequenceNumber = nextSequenceNumber;
        sendEchoProbe(targetIp, 64, false);

        const uint32_t echoKey = makeEchoKey(sequenceNumber);
        const std::map<uint32_t, EchoProbeState>::iterator it = pendingEchoes.find(echoKey);
        if (it == pendingEchoes.end() || !it->second.completed)
        {
            std::cout << "Request timeout for icmp_seq " << sequenceNumber << std::endl;
            pendingEchoes.erase(echoKey);
            return;
        }

        if (it->second.responseType == kICMPEchoReply)
        {
            std::cout << "Reply from " << it->second.responderIp
                      << ": icmp_seq=" << sequenceNumber
                      << " ttl=" << static_cast<int>(it->second.replyTtl)
                      << " time=" << formatRtt(it->second.rttMs) << " ms" << std::endl;
        }
        else if (it->second.responseType == kICMPTimeExceeded)
        {
            std::cout << "From " << it->second.responderIp
                      << ": icmp_seq=" << sequenceNumber
                      << " Time Exceeded" << std::endl;
        }
        else if (it->second.responseType == kICMPDestinationUnreachable)
        {
            std::cout << "From " << it->second.responderIp
                      << ": icmp_seq=" << sequenceNumber
                      << " Destination Unreachable" << std::endl;
        }

        pendingEchoes.erase(echoKey);
    }

    void Host::traceroute(const std::string &targetIp, uint8_t maxHops)
    {
        if (!iputil::isValidIp(targetIp))
        {
            std::cout << "[" << name << "] Target IP tidak valid: " << targetIp << std::endl;
            return;
        }

        std::cout << "traceroute to " << targetIp << ", max " << static_cast<int>(maxHops) << " hops" << std::endl;

        for (uint8_t ttl = 1; ttl <= maxHops; ++ttl)
        {
            const uint16_t sequenceNumber = nextSequenceNumber;
            sendEchoProbe(targetIp, ttl, true);

            const uint32_t echoKey = makeEchoKey(sequenceNumber);
            std::map<uint32_t, EchoProbeState>::iterator it = pendingEchoes.find(echoKey);
            if (it == pendingEchoes.end() || !it->second.completed)
            {
                std::cout << std::setw(2) << static_cast<int>(ttl) << "  *" << std::endl;
                pendingEchoes.erase(echoKey);
                continue;
            }

            std::cout << std::setw(2) << static_cast<int>(ttl) << "  " << it->second.responderIp
                      << "  " << formatRtt(it->second.rttMs) << " ms";

            const bool reachedDestination = (it->second.responseType == kICMPEchoReply);
            const bool unreachable = (it->second.responseType == kICMPDestinationUnreachable);
            if (unreachable)
            {
                std::cout << "  !N";
            }
            std::cout << std::endl;

            pendingEchoes.erase(it);
            if (reachedDestination || unreachable)
            {
                break;
            }
        }
    }

    void Host::startHttpServer(const std::string &file)
    {
        if (!httpServer)
        {
            httpServer = std::make_shared<HTTPServer>(this, file);
        }
        httpServer->start();
    }

    void Host::stopHttpServer()
    {
        if (httpServer)
        {
            httpServer->stop();
            httpServer.reset();
        }
    }

    void Host::startDhcpServer()
    {
        if (!dhcpServer)
        {
            dhcpServer = std::make_shared<DHCPServer>(this);
            dhcpServer->start();
        }
    }

    void Host::stopDhcpServer()
    {
        if (dhcpServer)
        {
            dhcpServer->stop();
            dhcpServer.reset();
        }
    }

    void Host::startDnsServer()
    {
        if (!dnsServer)
        {
            dnsServer = std::make_shared<DNSServer>(this);
            dnsServer->start();
        }
    }

    void Host::stopDnsServer()
    {
        if (dnsServer)
        {
            dnsServer->stop();
            dnsServer.reset();
        }
    }

    std::vector<Host *> Host::getAllHosts()
    {
        return gHostRegistry;
    }

}
