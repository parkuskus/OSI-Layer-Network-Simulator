#ifndef HOST_HPP
#define HOST_HPP

#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <vector>
#include <chrono>
#include "core/node.hpp"
#include "layer2/arp.hpp"
#include "layer3/icmp.hpp"
#include "layer3/ipv4.hpp"
#include "layer4/tcp_socket.hpp"

namespace magi
{

    class EthernetFrame;
    class Interface;
    class Packet;
    class TCPSocket;
    struct TCPSegment;
    class HTTPServer;
    class DHCPServer;
    class DNSServer;
    class UDPSocket;

    // Host Node
    class Host : public Node
    {
    private:
        struct AcceptedSocketEntry
        {
            std::string key;
            std::shared_ptr<TCPSocket> socket;
        };

        struct EchoProbeState
        {
            std::chrono::steady_clock::time_point sentAt;
            bool tracerouteProbe;
            bool completed;
            std::string targetIp;
            std::string responderIp;
            uint8_t responseType;
            uint8_t responseCode;
            uint8_t replyTtl;
            double rttMs;
        };

        std::string ipAddress;
        std::string defaultGateway;
        ARPCache arpCache;
        uint16_t echoIdentifier;
        uint16_t nextSequenceNumber;
        uint16_t nextIpIdentification;
        std::map<uint32_t, EchoProbeState> pendingEchoes;

        // Simple socket maps for in-simulator TCP delivery
        std::map<uint16_t, std::shared_ptr<TCPSocket>> listeningSockets;
        std::map<std::string, std::shared_ptr<TCPSocket>> activeSockets;
        std::map<uint16_t, std::queue<AcceptedSocketEntry>> acceptedSockets;
        std::set<std::string> queuedAcceptedSocketKeys;
        std::map<uint16_t, std::shared_ptr<UDPSocket>> udpSockets;

        std::string getPrimaryIp() const;
        uint32_t makeEchoKey(uint16_t sequenceNumber) const;
        std::string makeSocketKey(const std::string &localIp, uint16_t localPort,
                                  const std::string &remoteIp, uint16_t remotePort) const;
        std::string resolveNextHop(const std::string &targetIp) const;
        void sendArpRequest(Interface *iface, const std::string &targetIp);
        void flushQueuedPackets(Interface *iface, const std::string &nextHopIp, int vlanId);
        bool sendIpv4Packet(const IPv4Packet &packet);
        void sendEchoProbe(const std::string &targetIp, uint8_t ttl, bool tracerouteProbe);
        void completeEchoProbe(uint16_t identifier,
                               uint16_t sequenceNumber,
                               const std::string &responderIp,
                               uint8_t responseType,
                               uint8_t responseCode,
                               uint8_t replyTtl);
        bool extractEmbeddedEchoKey(const ICMPMessage &icmp,
                                    uint16_t &identifier,
                                    uint16_t &sequenceNumber) const;
        void handleArpFrame(Interface *incomingInterface, const EthernetFrame &frame);
        void handleIpv4Frame(const EthernetFrame &frame);

    public:
        Host(const std::string &name, const std::string &ipAddress = "", const std::string &defaultGateway = "");
        ~Host() override;

        // Getters dan Setters
        std::string getIpAddress() const { return ipAddress; }
        std::string getDefaultGateway() const { return defaultGateway; }
        void setIpAddress(const std::string &ip) { ipAddress = ip; }
        void setDefaultGateway(const std::string &gateway) { defaultGateway = gateway; }

        // Override virtual methods
        void handleReceive(Interface *incomingInterface, const std::vector<uint8_t> &rawBytes) override;
        std::string getType() const override { return "host"; }
        std::string toJson() const override;
        void printInfo() const override;
        void printArpCache() const;

        void sendPing(const std::string &targetIp);
        void traceroute(const std::string &targetIp, uint8_t maxHops = 30);

        // Allow upper layers to send IPv4 packets through this host (wrapper)
        bool sendIpv4(const IPv4Packet &packet);

        // TCP socket registration (simple in-simulator TCP delivery)
        void registerListeningSocket(uint16_t port, std::shared_ptr<TCPSocket> socket);
        void registerActiveSocket(const std::string &localIp, uint16_t localPort,
                                  const std::string &remoteIp, uint16_t remotePort,
                                  std::shared_ptr<TCPSocket> socket);
        std::shared_ptr<TCPSocket> findActiveSocket(const std::string &localIp, uint16_t localPort,
                                                    const std::string &remoteIp, uint16_t remotePort) const;
        std::shared_ptr<TCPSocket> acceptConnection(uint16_t port);
        void unregisterListeningSocket(uint16_t port);
        void unregisterActiveSocket(const std::string &localIp, uint16_t localPort,
                                    const std::string &remoteIp, uint16_t remotePort);
        // UDP socket registration
        void registerUdpSocket(uint16_t port, std::shared_ptr<UDPSocket> socket);
        void unregisterUdpSocket(uint16_t port);
        // Initiate TCP close (send FIN) for an active socket matching endpoints
        bool initiateCloseToRemote(const std::string &localIp,
                                   const std::string &remoteIp,
                                   uint16_t remotePort);

        // HTTP Server L7 Integration
        void startHttpServer(const std::string &file = "index.html");
        void stopHttpServer();
        std::shared_ptr<HTTPServer> getHttpServer() const { return httpServer; }

        void startDhcpServer();
        void stopDhcpServer();
        std::shared_ptr<DHCPServer> getDhcpServer() const { return dhcpServer; }

        void startDnsServer();
        void stopDnsServer();
        std::shared_ptr<DNSServer> getDnsServer() const { return dnsServer; }

        static std::vector<Host *> getAllHosts();

    private:
        std::shared_ptr<HTTPServer> httpServer;
        std::shared_ptr<DHCPServer> dhcpServer;
        std::shared_ptr<DNSServer> dnsServer;
    };

} // namespace magi

#endif // HOST_HPP
