#ifndef MAGI_LAYER7_UDP_SOCKET_HPP
#define MAGI_LAYER7_UDP_SOCKET_HPP

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include "layer4/udp.hpp"

namespace magi {

class Host;

class UDPSocket {
public:
    UDPSocket(Host *host);

    bool bind(const std::string &ip, uint16_t port);
    size_t sendto(const std::string &dstIp, uint16_t dstPort, const std::vector<uint8_t> &data);
    std::vector<uint8_t> recvfrom(std::string &outSrcIp, uint16_t &outSrcPort, size_t bufSize);

    // Called by Host when an IPv4 datagram with UDP payload arrives
    void onReceive(const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data);

    // Optional immediate receive handler (called by onReceive). If not set, packets are queued.
    using RecvHandler = std::function<void(const std::string& srcIp, uint16_t srcPort, const std::vector<uint8_t>& data)>;
    void setRecvHandler(RecvHandler h);

    uint16_t getLocalPort() const { return localPort; }
    std::string getLocalIp() const { return localIp; }

private:
    Host *host;
    std::string localIp;
    uint16_t localPort;

    // Simple receive queue holding tuples of (srcIp, srcPort, payload)
    struct RecvEntry { std::string srcIp; uint16_t srcPort; std::vector<uint8_t> payload; };
    std::queue<RecvEntry> recvQueue;
    RecvHandler recvHandler;
};

} // namespace magi

#endif // MAGI_LAYER7_UDP_SOCKET_HPP
