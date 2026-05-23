#include "layer7/http_client.hpp"
#include "layer2/host.hpp"
#include "layer7/magi_socket.hpp"
#include "layer7/dns_client.hpp"
#include "layer3/ip_utils.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>

namespace magi
{
    void HTTPClient::get(std::shared_ptr<Host> sourceHost, const std::string &url, const std::vector<std::shared_ptr<Host>> &allHosts)
    {
        if (!sourceHost)
        {
            std::cout << "Error: Host sumber kosong." << std::endl;
            return;
        }

        // 1. Parse URL (e.g. http://www.magi.com/index.html or www.magi.com/info or 192.168.1.10)
        std::string rawUrl = url;
        std::string prefix = "http://";
        if (rawUrl.compare(0, prefix.length(), prefix) == 0)
        {
            rawUrl = rawUrl.substr(prefix.length());
        }
        else
        {
            prefix = "https://";
            if (rawUrl.compare(0, prefix.length(), prefix) == 0)
            {
                rawUrl = rawUrl.substr(prefix.length());
            }
        }

        size_t slashPos = rawUrl.find('/');
        std::string hostStr = (slashPos == std::string::npos) ? rawUrl : rawUrl.substr(0, slashPos);
        std::string pathStr = (slashPos == std::string::npos) ? "/" : rawUrl.substr(slashPos);

        // 2. Resolve domain/host to IP address through the in-simulator DNS path,
        // then fall back to the topology list if no DNS server answers.
        std::string targetIp = DNSClient::resolve(sourceHost, hostStr, allHosts);
        std::shared_ptr<Host> targetHost = nullptr;

        if (targetIp.empty())
        {
            std::cout << "Error: Tidak dapat melakukan resolusi nama host / IP untuk '" << hostStr << "'" << std::endl;
            return;
        }

        for (const auto &h : allHosts)
        {
            if (iputil::stripCidr(h->getIpAddress()) == targetIp)
            {
                targetHost = h;
                break;
            }
        }

        if (!targetHost)
        {
            std::cout << "Error: Host tujuan dengan IP " << targetIp << " tidak ditemukan di topologi." << std::endl;
            return;
        }

        std::cout << "[HTTP Client] Memulai HTTP GET ke " << targetHost->getName() << " (" << targetIp << ":80) dengan path '" << pathStr << "'..." << std::endl;

        // 3. Initiate client MagiSocket and connect
        auto clientSock = std::make_shared<MagiSocket>(sourceHost.get(), MagiSocket::AF_INET, MagiSocket::SOCK_STREAM);
        if (!clientSock->connect(targetIp, 80))
        {
            std::cout << "[HTTP Client] Gagal terhubung ke server HTTP di " << targetIp << ":80" << std::endl;
            return;
        }

        // 4. Formulate and send HTTP Request
        std::stringstream reqStream;
        reqStream << "GET " << pathStr << " HTTP/1.1\r\n"
                  << "Host: " << hostStr << "\r\n"
                  << "Connection: close\r\n\r\n";

        std::string requestStr = reqStream.str();
        std::vector<uint8_t> reqBytes(requestStr.begin(), requestStr.end());

        std::cout << "[HTTP Client] Mengirimkan HTTP Request:\n" << requestStr << std::endl;
        clientSock->send(reqBytes);

        // 5. Receive response (synchronously delivered to receiveBuffer after server tick)
        std::vector<uint8_t> respBytes = clientSock->recv(65535);
        if (respBytes.empty())
        {
            std::cout << "[HTTP Client] Tidak menerima data response dari server." << std::endl;
        }
        else
        {
            std::string responseStr(respBytes.begin(), respBytes.end());
            std::cout << "[HTTP Client] Menerima HTTP Response:\n" << std::endl;
            std::cout << "------------------------------------------------------------" << std::endl;
            std::cout << responseStr << std::endl;
            std::cout << "------------------------------------------------------------" << std::endl;
        }

        // 6. Close socket
        clientSock->close();
    }
}
