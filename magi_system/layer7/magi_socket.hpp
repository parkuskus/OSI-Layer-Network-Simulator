#ifndef MAGI_LAYER7_MAGI_SOCKET_HPP
#define MAGI_LAYER7_MAGI_SOCKET_HPP

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace magi
{

    class Host;
    class TCPSocket;
    struct TCPSegment;
    class UDPSocket;

    class MagiSocket : public std::enable_shared_from_this<MagiSocket>
    {
    public:
        static const int AF_INET = 2;
        static const int SOCK_STREAM = 1;
        static const int SOCK_DGRAM = 2;

        MagiSocket(Host *host, int family, int sockType);

        bool bind(const std::string &ip, uint16_t port);
        bool listen(int backlog = 5);
        std::shared_ptr<MagiSocket> accept();
        bool connect(const std::string &ip, uint16_t port);
        size_t send(const std::vector<uint8_t> &data);
        std::vector<uint8_t> recv(size_t bufferSize);
        size_t sendto(const std::string &dstIp, uint16_t dstPort, const std::vector<uint8_t> &data);
        std::vector<uint8_t> recvfrom(std::string &outSrcIp, uint16_t &outSrcPort, size_t bufferSize);
        using RecvHandler = std::function<void(const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data)>;
        void setRecvHandler(RecvHandler handler);
        bool close();

        std::string getLocalIp() const { return localIp; }
        uint16_t getLocalPort() const { return localPort; }
        std::string getRemoteIp() const { return remoteIp; }
        uint16_t getRemotePort() const { return remotePort; }
        bool isBound() const { return bound; }
        bool isListening() const { return listening; }
        bool isConnected() const;
        std::string getStateString() const;

    private:
        Host *host;
        int family;
        int sockType;

        std::string localIp;
        uint16_t localPort;
        std::string remoteIp;
        uint16_t remotePort;
        int backlog;

        bool bound;
        bool listening;

        std::shared_ptr<TCPSocket> tcpTransport;
        std::shared_ptr<UDPSocket> udpTransport;

        std::string normalizeIp(const std::string &ip) const;
        void ensureTcpTransport();
        void ensureUdpTransport();
        bool sendTcpSegment(const std::shared_ptr<TCPSegment> &segment);
        bool sendUdpTo(const std::string &dstIp, uint16_t dstPort, const std::vector<uint8_t> &data);
        std::vector<uint8_t> recvFromUdp(std::string &outSrcIp, uint16_t &outSrcPort, size_t bufSize);
    };

} // namespace magi

#endif // MAGI_LAYER7_MAGI_SOCKET_HPP
