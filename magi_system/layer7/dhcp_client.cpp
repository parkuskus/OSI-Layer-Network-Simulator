#include "layer7/dhcp_client.hpp"
#include "layer7/udp_socket.hpp"
#include "layer2/host.hpp"
#include "core/interface.hpp"
#include "layer3/ip_utils.hpp"

#include <iostream>
#include <chrono>
#include <thread>

namespace magi {

std::string DHCPClient::discover(Host *host, int timeoutMs)
{
    if (!host)
        return "";

    std::shared_ptr<UDPSocket> sock = std::make_shared<UDPSocket>(host);

    std::string bindIp = host->getIpAddress().empty() ? "0.0.0.0" : iputil::stripCidr(host->getIpAddress());
    if (!sock->bind(bindIp, 68))
    {
        std::cout << "[DHCPClient] Failed to bind UDP socket to port 68" << std::endl;
        return "";
    }

    host->registerUdpSocket(68, sock);

    // Build discover payload: DISCOVER:<mac>
    std::string mac = "";
    auto iface = host->getInterface(1);
    if (iface)
        mac = iface->getMacAddress();

    std::string discoverMsg = std::string("DISCOVER:") + mac;
    std::vector<uint8_t> payload(discoverMsg.begin(), discoverMsg.end());

    sock->sendto("255.255.255.255", 67, payload);

    // wait for OFFER
    const auto start = std::chrono::steady_clock::now();
    std::string srcIp;
    uint16_t srcPort = 0;
    while (true)
    {
        auto resp = sock->recvfrom(srcIp, srcPort, 2048);
        if (!resp.empty())
        {
            std::string s(resp.begin(), resp.end());
            if (s.rfind("OFFER:", 0) == 0)
            {
                std::string offerIp = s.substr(std::string("OFFER:").length());
                // Send REQUEST:<ip>:<mac> as broadcast
                std::string req = std::string("REQUEST:") + offerIp + ":" + mac;
                std::vector<uint8_t> reqv(req.begin(), req.end());
                sock->sendto("255.255.255.255", 67, reqv);

                // wait for ACK
                auto ackStart = std::chrono::steady_clock::now();
                while (true)
                {
                    auto r2 = sock->recvfrom(srcIp, srcPort, 2048);
                    if (!r2.empty())
                    {
                        std::string s2(r2.begin(), r2.end());
                        if (s2.rfind("ACK:", 0) == 0)
                        {
                            std::string ip = s2.substr(std::string("ACK:").length());
                            // Apply IP to host
                            host->setIpAddress(ip + "/24");
                            host->printInfo();
                            host->unregisterUdpSocket(68);
                            return ip;
                        }
                    }
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - ackStart).count() > timeoutMs)
                        break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
        }
        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count() > timeoutMs)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    host->unregisterUdpSocket(68);
    std::cout << "[DHCPClient] Discovery timed out" << std::endl;
    return "";
}

} // namespace magi
