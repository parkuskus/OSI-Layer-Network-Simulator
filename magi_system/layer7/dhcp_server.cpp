#include "layer7/dhcp_server.hpp"
#include "layer2/host.hpp"
#include "layer7/magi_socket.hpp"
#include "layer3/ip_utils.hpp"

#include <iostream>
#include <sstream>

namespace magi {

    DHCPServer::DHCPServer(Host *host)
        : host(host)
    {
    }

    DHCPServer::~DHCPServer()
    {
        stop();
    }

    void DHCPServer::buildPoolFromHost()
    {
        pool.clear();
        std::string cidr = host->getIpAddress();
        magi::iputil::ParsedCidr parsed;
        if (!magi::iputil::parseCidr(cidr, parsed))
            return;

        // start assigning from network + 10
        uint32_t base = parsed.networkValue;
        for (uint32_t i = 10; i < 100; ++i)
        {
            uint32_t ipval = base + i;
            std::ostringstream ss;
            ss << magi::iputil::toIp(ipval);
            pool.push_back(ss.str());
        }
    }

    bool DHCPServer::start()
    {
        if (!host)
            return false;

        socket = std::make_shared<MagiSocket>(host, MagiSocket::AF_INET, MagiSocket::SOCK_DGRAM);
        std::string bindIp = magi::iputil::stripCidr(host->getIpAddress());
        if (bindIp.empty())
            bindIp = "0.0.0.0";

        if (!socket->bind(bindIp, 67))
            return false;

        buildPoolFromHost();

        socket->setRecvHandler([this](const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data) {
            handleMessage(srcIp, srcPort, data);
        });

        std::cout << "[DHCP] Server started on " << bindIp << ":67 (pool size=" << pool.size() << ")" << std::endl;
        return true;
    }

    void DHCPServer::stop()
    {
        if (socket)
        {
            socket->close();
            socket.reset();
        }
    }

    std::string DHCPServer::chooseOfferForMac(const std::string &mac)
    {
        // If already leased, return it
        auto it = leases.find(mac);
        if (it != leases.end())
            return it->second;

        if (pool.empty())
            return "";

        std::string ip = pool.back();
        pool.pop_back();
        leases[mac] = ip;
        return ip;
    }

    void DHCPServer::handleMessage(const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data)
    {
        std::string s(data.begin(), data.end());
        if (s.rfind("DISCOVER:", 0) == 0)
        {
            std::string mac = s.substr(std::string("DISCOVER:").length());
            std::string offer = chooseOfferForMac(mac);
            if (offer.empty())
                return;

            std::string payload = std::string("OFFER:") + offer;
            std::vector<uint8_t> out(payload.begin(), payload.end());
            // Reply to client using unicast to srcIp:srcPort if possible, otherwise broadcast
            if (!srcIp.empty() && srcIp != "0.0.0.0")
            {
                socket->sendto(srcIp, srcPort, out);
            }
            else
            {
                socket->sendto("255.255.255.255", 68, out);
            }

            std::cout << "[DHCP] Received DISCOVER from " << mac << ", offered " << offer << std::endl;
        }
        else if (s.rfind("REQUEST:", 0) == 0)
        {
            // format REQUEST:<ip>:<mac>
            std::string rest = s.substr(std::string("REQUEST:").length());
            size_t pos = rest.find(':');
            if (pos == std::string::npos)
                return;
            std::string ip = rest.substr(0, pos);
            std::string mac = rest.substr(pos + 1);

            // assign and ACK
            leases[mac] = ip;

            std::string payload = std::string("ACK:") + ip;
            std::vector<uint8_t> out(payload.begin(), payload.end());
            if (!srcIp.empty() && srcIp != "0.0.0.0")
            {
                socket->sendto(srcIp, srcPort, out);
            }
            else
            {
                socket->sendto("255.255.255.255", 68, out);
            }

            std::cout << "[DHCP] Received REQUEST from " << mac << ", ACK " << ip << std::endl;
        }
    }

} // namespace magi
