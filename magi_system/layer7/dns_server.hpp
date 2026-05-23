#ifndef MAGI_LAYER7_DNS_SERVER_HPP
#define MAGI_LAYER7_DNS_SERVER_HPP

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace magi
{
    class Host;
    class UDPSocket;

    class DNSServer
    {
    public:
        explicit DNSServer(Host *host);
        ~DNSServer();

        bool start();
        void stop();

        void addRecord(const std::string &name, const std::string &ip);
        std::string resolve(const std::string &name) const;

        bool isRunning() const { return running; }

    private:
        Host *host;
        std::shared_ptr<UDPSocket> socket;
        bool running;
        std::map<std::string, std::string> records;

        static std::string normalizeName(const std::string &name);
        void seedDefaultRecords();
        void handleQuery(const std::string &srcIp, uint16_t srcPort, const std::vector<uint8_t> &data);
    };
}

#endif // MAGI_LAYER7_DNS_SERVER_HPP