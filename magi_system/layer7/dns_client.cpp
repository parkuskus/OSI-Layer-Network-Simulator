#include "layer7/dns_client.hpp"

#include "layer2/host.hpp"
#include "layer7/udp_socket.hpp"
#include "layer3/ip_utils.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <iostream>

namespace magi
{
    namespace
    {
        std::string lowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            return value;
        }

        std::string normalizeName(const std::string &name)
        {
            std::string value = lowerCopy(name);
            const std::string httpPrefix = "http://";
            const std::string httpsPrefix = "https://";

            if (value.compare(0, httpPrefix.size(), httpPrefix) == 0)
            {
                value = value.substr(httpPrefix.size());
            }
            else if (value.compare(0, httpsPrefix.size(), httpsPrefix) == 0)
            {
                value = value.substr(httpsPrefix.size());
            }

            if (value.compare(0, 4, "www.") == 0)
            {
                value = value.substr(4);
            }

            if (value.size() > 4 && value.compare(value.size() - 4, 4, ".com") == 0)
            {
                value = value.substr(0, value.size() - 4);
            }

            return value;
        }

        std::string aliasForHost(const std::string &hostName)
        {
            return std::string("www.") + normalizeName(hostName) + ".com";
        }

        std::string findLocalFallbackIp(const std::string &query, const std::vector<std::shared_ptr<Host>> &allHosts)
        {
            const std::string normalizedQuery = normalizeName(query);

            for (const auto &host : allHosts)
            {
                if (!host)
                {
                    continue;
                }

                const std::string hostIp = iputil::stripCidr(host->getIpAddress());
                const std::string hostName = normalizeName(host->getName());
                const std::string hostAlias = normalizeName(aliasForHost(host->getName()));

                if (hostIp == query || hostName == normalizedQuery || hostAlias == normalizedQuery)
                {
                    return hostIp;
                }
            }

            if (iputil::isValidIp(query))
            {
                return query;
            }

            return "";
        }
    } // namespace

    std::string DNSClient::resolve(std::shared_ptr<Host> sourceHost,
                                   const std::string &name,
                                   const std::vector<std::shared_ptr<Host>> &allHosts,
                                   int timeoutMs,
                                   int attempts)
    {
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

        std::shared_ptr<UDPSocket> socket = std::make_shared<UDPSocket>(sourceHost.get());
        std::string bindIp = iputil::stripCidr(sourceHost->getIpAddress());
        if (bindIp.empty())
        {
            bindIp = "0.0.0.0";
        }

        const uint16_t localPort = 53053;
        if (!socket->bind(bindIp, localPort))
        {
            return findLocalFallbackIp(name, allHosts);
        }

        sourceHost->registerUdpSocket(localPort, socket);

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
                            sourceHost->unregisterUdpSocket(localPort);
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

        sourceHost->unregisterUdpSocket(localPort);
        return findLocalFallbackIp(name, allHosts);
    }

} // namespace magi