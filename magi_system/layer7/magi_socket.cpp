#include "layer7/magi_socket.hpp"

#include "layer2/host.hpp"
#include "layer3/ip_utils.hpp"
#include "layer3/ipv4.hpp"
#include "layer4/tcp.hpp"
#include "layer4/tcp_socket.hpp"

#include <memory>

namespace magi
{
    const int MagiSocket::AF_INET;
    const int MagiSocket::SOCK_STREAM;
    const int MagiSocket::SOCK_DGRAM;

    MagiSocket::MagiSocket(Host *host, int family, int sockType)
        : host(host), family(family), sockType(sockType), localPort(0), remotePort(0), backlog(0), bound(false), listening(false)
    {
    }

    std::string MagiSocket::normalizeIp(const std::string &ip) const
    {
        return iputil::stripCidr(ip);
    }

    void MagiSocket::ensureTcpTransport()
    {
        if (!tcpTransport)
        {
            tcpTransport = std::make_shared<TCPSocket>(localIp, localPort, remoteIp, remotePort);
        }
    }

    bool MagiSocket::bind(const std::string &ip, uint16_t port)
    {
        if (!host || family != AF_INET)
        {
            return false;
        }

        localIp = normalizeIp(ip);
        localPort = port;
        bound = iputil::isValidIp(localIp) && localPort != 0;
        return bound;
    }

    bool MagiSocket::listen(int backlogValue)
    {
        if (sockType != SOCK_STREAM || !host || !bound)
        {
            return false;
        }

        backlog = backlogValue;
        ensureTcpTransport();
        tcpTransport->localIP = localIp;
        tcpTransport->localPort = localPort;
        tcpTransport->remoteIP.clear();
        tcpTransport->remotePort = 0;
        tcpTransport->setState(TCPState::LISTEN);

        host->registerListeningSocket(localPort, tcpTransport);
        listening = true;
        return true;
    }

    std::shared_ptr<MagiSocket> MagiSocket::accept()
    {
        if (sockType != SOCK_STREAM || !tcpTransport || !tcpTransport->isConnected())
        {
            return nullptr;
        }

        std::shared_ptr<MagiSocket> accepted = std::make_shared<MagiSocket>(host, family, sockType);
        accepted->localIp = localIp;
        accepted->localPort = localPort;
        accepted->remoteIp = remoteIp;
        accepted->remotePort = remotePort;
        accepted->bound = bound;
        accepted->listening = false;
        accepted->tcpTransport = tcpTransport;
        return accepted;
    }

    bool MagiSocket::connect(const std::string &ip, uint16_t port)
    {
        if (sockType != SOCK_STREAM || !host)
        {
            return false;
        }

        remoteIp = normalizeIp(ip);
        remotePort = port;

        if (!bound)
        {
            const std::string primary = normalizeIp(host->getIpAddress());
            if (!bind(primary, 49152))
            {
                return false;
            }
        }

        ensureTcpTransport();
        tcpTransport->localIP = localIp;
        tcpTransport->localPort = localPort;
        tcpTransport->remoteIP = remoteIp;
        tcpTransport->remotePort = remotePort;

        host->registerActiveSocket(localIp, localPort, remoteIp, remotePort, tcpTransport);

        std::shared_ptr<TCPSegment> syn = tcpTransport->initiateConnection();
        if (!syn)
        {
            return false;
        }

        IPv4Packet synPacket;
        synPacket.srcIp = localIp;
        synPacket.dstIp = remoteIp;
        synPacket.protocol = 6;
        synPacket.ttl = 64;
        synPacket.identification = 0;
        synPacket.payload = syn->toBytes();
        synPacket.updateChecksum();

        return host->sendIpv4(synPacket) && tcpTransport->isConnected();
    }

    bool MagiSocket::sendTcpSegment(const std::shared_ptr<TCPSegment> &segment)
    {
        if (!segment || !host)
        {
            return false;
        }

        IPv4Packet packet;
        packet.srcIp = localIp;
        packet.dstIp = remoteIp;
        packet.protocol = 6;
        packet.ttl = 64;
        packet.identification = 0;
        packet.payload = segment->toBytes();
        packet.updateChecksum();
        return host->sendIpv4(packet);
    }

    size_t MagiSocket::send(const std::vector<uint8_t> &data)
    {
        if (sockType != SOCK_STREAM || !tcpTransport || !tcpTransport->canSendData())
        {
            return 0;
        }

        std::shared_ptr<TCPSegment> segment = tcpTransport->sendData(data);
        if (!segment || !sendTcpSegment(segment))
        {
            return 0;
        }

        return data.size();
    }

    std::vector<uint8_t> MagiSocket::recv(size_t bufferSize)
    {
        if (sockType != SOCK_STREAM || !tcpTransport)
        {
            return {};
        }

        std::vector<uint8_t> data = tcpTransport->getReceivedData();
        if (bufferSize < data.size())
        {
            data.resize(bufferSize);
        }

        return data;
    }

    bool MagiSocket::close()
    {
        if (sockType != SOCK_STREAM || !tcpTransport)
        {
            return false;
        }

        std::shared_ptr<TCPSegment> segment = tcpTransport->initiateClose();
        if (segment)
        {
            sendTcpSegment(segment);
        }

        if (tcpTransport->isClosed() && host)
        {
            if (listening)
            {
                host->unregisterListeningSocket(localPort);
            }
            if (!remoteIp.empty() && remotePort != 0)
            {
                host->unregisterActiveSocket(localIp, localPort, remoteIp, remotePort);
            }
        }

        listening = false;
        return true;
    }

    bool MagiSocket::isConnected() const
    {
        return tcpTransport && tcpTransport->isConnected();
    }

    std::string MagiSocket::getStateString() const
    {
        if (tcpTransport)
        {
            return tcpTransport->getStateString();
        }
        return listening ? "LISTEN" : (bound ? "BOUND" : "CLOSED");
    }

} // namespace magi