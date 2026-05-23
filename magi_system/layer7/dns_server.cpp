#include "layer7/dns_server.hpp"

#include "layer2/host.hpp"
#include "layer7/magi_socket.hpp"
#include "layer3/ip_utils.hpp"

#include <algorithm>
#include <cctype>
#include <iostream>

namespace magi
{
    namespace
    {
        std::string lowerCopy(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
                           { return static_cast<char>(std::tolower(ch)); });
            return value;
        }

    } // namespace

    std::string DNSServer::normalizeName(const std::string &name)
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

    DNSServer::DNSServer(Host *host)
        : host(host), running(false)
    {
    }

    DNSServer::~DNSServer()
    {
        stop();
    }

    void DNSServer::seedDefaultRecords()
    {
        if (!host)
        {
            return;
        }

        const std::string hostIp = iputil::stripCidr(host->getIpAddress());
        if (hostIp.empty())
        {
            return;
        }

        const std::string hostName = normalizeName(host->getName());
        if (!hostName.empty())
        {
            records.emplace(hostName, hostIp);
        }

        records.emplace(hostIp, hostIp);
    }

    bool DNSServer::start()
    {
        if (!host || running)
        {
            return false;
        }

        socket = std::make_shared<MagiSocket>(host, MagiSocket::AF_INET, MagiSocket::SOCK_DGRAM);
        std::string bindIp = iputil::stripCidr(host->getIpAddress());
        if (bindIp.empty())
        {
            bindIp = "0.0.0.0";
        }

        if (!socket->bind(bindIp, 53))
        {
            std::cout << "[DNS] Failed to bind UDP socket on port 53" << std::endl;
            return false;
        }

        seedDefaultRecords();

        socket->setRecvHandler([this](const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data)
                               { handleQuery(srcIp, srcPort, data); });

        running = true;
        std::cout << "[DNS] Server started on " << bindIp << ":53" << std::endl;
        return true;
    }

    void DNSServer::stop()
    {
        if (!running)
        {
            return;
        }

        if (socket)
        {
            socket->close();
            socket.reset();
        }
        running = false;
        std::cout << "[DNS] Server stopped." << std::endl;
    }

    void DNSServer::addRecord(const std::string &name, const std::string &ip)
    {
        if (!iputil::isValidIp(ip))
        {
            return;
        }

        records[normalizeName(name)] = ip;
    }

    std::string DNSServer::resolve(const std::string &name) const
    {
        const std::string normalized = normalizeName(name);
        std::map<std::string, std::string>::const_iterator it = records.find(normalized);
        if (it != records.end())
        {
            return it->second;
        }

        return "";
    }

    void DNSServer::handleQuery(const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data)
    {
        if (!socket)
        {
            return;
        }

        std::string query(data.begin(), data.end());
        if (query.rfind("QUERY:", 0) != 0)
        {
            return;
        }

        const std::string name = query.substr(6);
        const std::string normalized = normalizeName(name);
        std::string ip = resolve(normalized);

        std::string response;
        if (!ip.empty())
        {
            response = std::string("ANSWER:") + ip;
            std::cout << "[DNS] QUERY " << name << " -> " << ip << std::endl;
        }
        else
        {
            response = "NXDOMAIN";
            std::cout << "[DNS] QUERY " << name << " -> NXDOMAIN" << std::endl;
        }

        std::vector<uint8_t> payload(response.begin(), response.end());
        if (!srcIp.empty() && srcIp != "0.0.0.0")
        {
            socket->sendto(srcIp, srcPort, payload);
        }
        else
        {
            socket->sendto("255.255.255.255", srcPort, payload);
        }
    }

} // namespace magi
