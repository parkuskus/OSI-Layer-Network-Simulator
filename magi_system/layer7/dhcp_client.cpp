#include "layer7/dhcp_client.hpp"
#include "layer7/magi_socket.hpp"
#include "layer2/host.hpp"
#include "core/interface.hpp"
#include "layer3/ip_utils.hpp"

#include <iostream>
#include <chrono>
#include <thread>

namespace magi {

std::string DHCPClient::discover(Host *host, int timeoutMs, int maxAttempts)
{
    if (!host)
        return "";

    if (timeoutMs <= 0)
        timeoutMs = 3000;
    if (maxAttempts <= 0)
        maxAttempts = 1;

    std::shared_ptr<MagiSocket> sock = std::make_shared<MagiSocket>(host, MagiSocket::AF_INET, MagiSocket::SOCK_DGRAM);

    std::string bindIp = host->getIpAddress().empty() ? "0.0.0.0" : iputil::stripCidr(host->getIpAddress());
    if (!sock->bind(bindIp, 68))
    {
        std::cout << "[DHCPClient] Failed to bind UDP socket to port 68" << std::endl;
        return "";
    }

    // Build discover payload: DISCOVER:<mac>
    std::string mac = "";
    auto iface = host->getInterface(1);
    if (iface)
        mac = iface->getMacAddress();

    std::string discoverMsg = std::string("DISCOVER:") + mac;
    std::vector<uint8_t> payload(discoverMsg.begin(), discoverMsg.end());

    const int attemptTimeoutMs = std::max(250, timeoutMs / maxAttempts);
    const int retryPauseMs = std::max(50, std::min(250, attemptTimeoutMs / 4));
    std::string srcIp;
    uint16_t srcPort = 0;
    std::string assignedCidr = "";

    auto parseLeaseValue = [](const std::string &leaseValue, std::string &outIp, std::string &outCidr) -> bool
    {
        if (leaseValue.empty())
        {
            return false;
        }

        if (iputil::isValidCidr(leaseValue))
        {
            outIp = iputil::stripCidr(leaseValue);
            outCidr = leaseValue;
            return true;
        }

        if (iputil::isValidIp(leaseValue))
        {
            outIp = leaseValue;
            outCidr = leaseValue + "/24";
            return true;
        }

        return false;
    };

    for (int attempt = 1; attempt <= maxAttempts; ++attempt)
    {
        std::cout << "[DHCPClient] Attempt " << attempt << "/" << maxAttempts
                  << " sending DISCOVER from " << host->getName()
                  << " (timeout=" << attemptTimeoutMs << "ms)" << std::endl;

        sock->sendto("255.255.255.255", 67, payload);

        const auto attemptStart = std::chrono::steady_clock::now();
        bool offerReceived = false;

        while (true)
        {
            auto resp = sock->recvfrom(srcIp, srcPort, 2048);
            if (!resp.empty())
            {
                std::string s(resp.begin(), resp.end());
                if (s.rfind("OFFER:", 0) == 0)
                {
                    std::string offerIp;
                    std::string offerCidr;
                    if (!parseLeaseValue(s.substr(std::string("OFFER:").length()), offerIp, offerCidr))
                    {
                        break;
                    }
                    offerReceived = true;
                    std::cout << "[DHCPClient] Received OFFER " << offerIp
                              << " on attempt " << attempt << std::endl;
                    assignedCidr = offerCidr;

                    // Send REQUEST:<ip>:<mac> as broadcast
                    std::string req = std::string("REQUEST:") + offerIp + ":" + mac;
                    std::vector<uint8_t> reqv(req.begin(), req.end());
                    sock->sendto("255.255.255.255", 67, reqv);

                    // wait for ACK
                    const auto ackStart = std::chrono::steady_clock::now();
                    while (true)
                    {
                        auto r2 = sock->recvfrom(srcIp, srcPort, 2048);
                        if (!r2.empty())
                        {
                            std::string s2(r2.begin(), r2.end());
                            if (s2.rfind("ACK:", 0) == 0)
                            {
                                std::string ip;
                                std::string cidr;
                                if (!parseLeaseValue(s2.substr(std::string("ACK:").length()), ip, cidr))
                                {
                                    break;
                                }

                                // Apply IP to host
                                host->setIpAddress(cidr.empty() ? assignedCidr : cidr);
                                host->printInfo();
                                sock->close();
                                std::cout << "[DHCPClient] Lease acquired: " << ip << std::endl;
                                return ip;
                            }
                        }
                        if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - ackStart).count() >= attemptTimeoutMs)
                            break;
                        std::this_thread::sleep_for(std::chrono::milliseconds(retryPauseMs));
                    }

                    std::cout << "[DHCPClient] ACK timeout on attempt " << attempt << std::endl;
                    break;
                }
            }

            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - attemptStart).count() >= attemptTimeoutMs)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(retryPauseMs));
        }

        if (!offerReceived)
        {
            std::cout << "[DHCPClient] No OFFER received on attempt " << attempt << std::endl;
        }
    }

    sock->close();
    std::cout << "[DHCPClient] Discovery failed after " << maxAttempts << " attempts" << std::endl;
    return "";
}

} // namespace magi
