#include "layer7/udp_socket.hpp"

#include "layer2/host.hpp"
#include "layer3/ipv4.hpp"

#include <iostream>

namespace magi {

UDPSocket::UDPSocket(Host *host)
    : host(host), localIp(""), localPort(0)
{
}

bool UDPSocket::bind(const std::string &ip, uint16_t port)
{
    if (!host)
        return false;

    localIp = ip;
    localPort = port;
    return true;
}

size_t UDPSocket::sendto(const std::string &dstIp, uint16_t dstPort, const std::vector<uint8_t> &data)
{
    if (!host || localPort == 0)
        return 0;

    UDPSegment seg;
    seg.sourcePort = localPort;
    seg.destinationPort = dstPort;
    seg.payload = data;
    seg.updateChecksum(localIp, dstIp);

    IPv4Packet packet;
    packet.srcIp = localIp;
    packet.dstIp = dstIp;
    packet.protocol = 17; // UDP
    packet.ttl = 64;
    packet.identification = 0;
    packet.payload = seg.toBytes();
    packet.updateChecksum();

    bool ok = host->sendIpv4(packet);
    if (!ok)
    {
        std::cerr << "[UDPSocket] Failed to send IPv4 packet to " << dstIp << std::endl;
        return 0;
    }

    return data.size();
}

std::vector<uint8_t> UDPSocket::recvfrom(std::string &outSrcIp, uint16_t &outSrcPort, size_t bufSize)
{
    if (recvQueue.empty())
        return {};

    RecvEntry e = recvQueue.front();
    recvQueue.pop();

    outSrcIp = e.srcIp;
    outSrcPort = e.srcPort;

    std::vector<uint8_t> data = e.payload;
    if (data.size() > bufSize)
        data.resize(bufSize);
    return data;
}

void UDPSocket::onReceive(const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data)
{
    RecvEntry e;
    e.srcIp = srcIp;
    e.srcPort = srcPort;
    e.payload = data;
    recvQueue.push(std::move(e));
}

} // namespace magi
