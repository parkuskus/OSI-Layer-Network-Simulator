#ifndef MAGI_LAYER7_MAGI_SOCKET_HPP
#define MAGI_LAYER7_MAGI_SOCKET_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace magi
{

    class Host;
    class TCPSocket;
    class TCPSegment;

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

        std::string normalizeIp(const std::string &ip) const;
        void ensureTcpTransport();
        bool sendTcpSegment(const std::shared_ptr<TCPSegment> &segment);
    };

} // namespace magi

#endif // MAGI_LAYER7_MAGI_SOCKET_HPP