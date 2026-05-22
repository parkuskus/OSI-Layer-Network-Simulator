#include "host.hpp"
#include "core/interface.hpp"
#include "layer2/ethernet.hpp"

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
    uint16_t gNextEchoIdentifier = 1;

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

        if (getPrimaryIp().empty())
        {
            std::cout << "[" << name << "] IP host belum dikonfigurasi." << std::endl;
            return false;
        }

        const std::string nextHopIp = resolveNextHop(packet.dstIp);
        if (nextHopIp.empty())
        {
            std::cout << "[" << name << "] Tidak ada route ke " << packet.dstIp
                      << " (default gateway belum di-set)." << std::endl;
            return false;
        }

        const std::vector<uint8_t> ipv4Bytes = packet.toBytes();
        const std::map<std::string, std::string>::const_iterator cacheIt = arpCache.table.find(nextHopIp);
        if (cacheIt != arpCache.table.end())
        {
            EthernetFrame frame;
            frame.dstMac = cacheIt->second;
            frame.srcMac = iface->getMacAddress();
            frame.etherType = kEtherTypeIpv4;
            frame.vlanId = iputil::kUntaggedVlan;
            frame.payload = ipv4Bytes;
            iface->send(frame.toBytes());
            return true;
        }

        std::vector<std::vector<uint8_t>> &pending = arpCache.queue[nextHopIp];
        pending.push_back(ipv4Bytes);

        if (pending.size() > 1)
        {
            return true;
        }

        sendArpRequest(iface.get(), nextHopIp);
        return true;
    }

    void Host::registerListeningSocket(uint16_t port, std::shared_ptr<TCPSocket> socket)
    {
        listeningSockets[port] = socket;
    }

    void Host::registerActiveSocket(const std::string &localIp, uint16_t localPort,
                                    const std::string &remoteIp, uint16_t remotePort,
                                    std::shared_ptr<TCPSocket> socket)
    {
        std::ostringstream oss;
        oss << localIp << ":" << localPort << ":" << remoteIp << ":" << remotePort;
        activeSockets[oss.str()] = socket;
    }

    void Host::unregisterListeningSocket(uint16_t port)
    {
        listeningSockets.erase(port);
    }

    void Host::unregisterActiveSocket(const std::string &localIp, uint16_t localPort,
                                      const std::string &remoteIp, uint16_t remotePort)
    {
        std::ostringstream oss;
        oss << localIp << ":" << localPort << ":" << remoteIp << ":" << remotePort;
        activeSockets.erase(oss.str());
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

    bool Host::extractEmbeddedEchoKey(const ICMPMessage &icmp,
                                      uint16_t &identifier,
                                      uint16_t &sequenceNumber) const
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

        if (!packet.validateChecksum())
        {
            return;
        }

        arpCache.table[packet.srcIp] = frame.srcMac;

        if (packet.dstIp != getPrimaryIp())
        {
            return;
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
                sendIpv4Packet(response);
                return;
            }

            if (icmp.type == kICMPEchoReply)
            {
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
                return;
            }

            std::ostringstream keyoss;
            keyoss << packet.dstIp << ":" << tcp.destinationPort << ":" << packet.srcIp << ":" << tcp.sourcePort;
            std::string key = keyoss.str();
            auto ait = activeSockets.find(key);
            if (ait != activeSockets.end())
            {
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
                    resp.updateChecksum();
                    sendIpv4Packet(resp);
                }
                return;
            }

            auto lit = listeningSockets.find(tcp.destinationPort);
            if (lit != listeningSockets.end())
            {
                auto serverSock = lit->second;
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
                    resp.updateChecksum();
                    sendIpv4Packet(resp);
                }

                registerActiveSocket(packet.dstIp, tcp.destinationPort, packet.srcIp, tcp.sourcePort, serverSock);
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

}
