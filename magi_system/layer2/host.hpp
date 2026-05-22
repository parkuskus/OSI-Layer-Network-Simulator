#ifndef HOST_HPP
#define HOST_HPP

#include <cstdint>
#include <string>
#include <map>
#include <memory>
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

    // Host Node
    class Host : public Node
    {
    private:
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

        std::string getPrimaryIp() const;
        uint32_t makeEchoKey(uint16_t sequenceNumber) const;
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
        // Initiate TCP close (send FIN) for an active socket matching endpoints
        bool initiateCloseToRemote(const std::string &localIp,
                                   const std::string &remoteIp,
                                   uint16_t remotePort);
    };

} // namespace magi

#endif // HOST_HPP