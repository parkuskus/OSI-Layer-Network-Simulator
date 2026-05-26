#include "layer7/dns_client.hpp"

#include "layer2/host.hpp"
#include "layer7/magi_socket.hpp"
#include "layer3/ip_utils.hpp"

#include <chrono>
#include <iostream>

namespace magi
{
    std::string DNSClient::resolve(std::shared_ptr<Host> sourceHost,
                                   const std::string &name,
                                   const std::vector<std::shared_ptr<Host>> &allHosts,
                                   int timeoutMs,
                                   int attempts)
    {
        (void)allHosts;

        if (!sourceHost)
        {
            return "";
        }

        if (iputil::isValidIp(name))
        {
            return name;
        }

        if (timeoutMs <= 0)
        {
            timeoutMs = 1000;
        }
        if (attempts <= 0)
        {
            attempts = 1;
        }

        std::shared_ptr<MagiSocket> socket = std::make_shared<MagiSocket>(sourceHost.get(), MagiSocket::AF_INET, MagiSocket::SOCK_DGRAM);
        std::string bindIp = iputil::stripCidr(sourceHost->getIpAddress());
        if (bindIp.empty())
        {
            bindIp = "0.0.0.0";
        }

        const uint16_t localPort = 53053;
        if (!socket->bind(bindIp, localPort))
        {
            return "";
        }

        const std::string query = std::string("QUERY:") + name;
        const std::vector<uint8_t> payload(query.begin(), query.end());

        std::string srcIp;
        uint16_t srcPort = 0;

        for (int attempt = 1; attempt <= attempts; ++attempt)
        {
            socket->sendto("255.255.255.255", 53, payload);

            const auto start = std::chrono::steady_clock::now();
            while (true)
            {
                std::vector<uint8_t> response = socket->recvfrom(srcIp, srcPort, 2048);
                if (!response.empty())
                {
                    std::string responseText(response.begin(), response.end());
                    if (responseText.rfind("ANSWER:", 0) == 0)
                    {
                        std::string ip = responseText.substr(7);
                        if (iputil::isValidIp(ip))
                        {
                            socket->close();
                            return ip;
                        }
                    }
                    else if (responseText.rfind("NXDOMAIN", 0) == 0)
                    {
                        break;
                    }
                }

                const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
                if (elapsed >= timeoutMs)
                {
                    break;
                }
            }
        }

        socket->close();
        return "";
    }

} // namespace magi
